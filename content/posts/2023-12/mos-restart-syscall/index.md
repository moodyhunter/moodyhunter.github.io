---
title: Signals, and automatic restart of signal-interrupted syscalls
slug: signal-and-syscall-restart-in-mos
date: "2023-12-09T00:00:00.000Z"
tags:
  - MOS
  - OS
  - Operating System
  - Syscall
  - Signal
---

This post is about your sleep quality, or not yours, but your program's.

Thoughout this post, implementation details of MOS are used as examples, but
the concepts should be applicable to other OS kernels (e.g. Linux).

- [Signals](#signals)
  - [When are Signals Delivered?](#when-are-signals-delivered)
  - [How Long Can a System Call Last?](#how-long-can-a-system-call-last)
    - [What Wakes You Up?](#what-wakes-you-up)
- [Signals Ruin Everything](#signals-ruin-everything)
  - [Where are we returning to?](#where-are-we-returning-to)
- [Can we do better?](#can-we-do-better)
  - [SA_RESTART and siginterrupt(3p)](#sa_restart-and-siginterrupt3p)
  - [What does the kernel need to know, to restart the syscall?](#what-does-the-kernel-need-to-know-to-restart-the-syscall)
  - [-ERESTARTSYS Comes to the Rescue](#-erestartsys-comes-to-the-rescue)
  - [How This Works?](#how-this-works)
  - [Why mangling the instruction pointer?](#why-mangling-the-instruction-pointer)
  - [Concerns and Limitations](#concerns-and-limitations)
- [Conclusion](#conclusion)
- [References](#references)

It is always said...

> Upon a signal being delivered to a process, the kernel <u>**interrupts**</u> the
> process and <u>**invokes**</u> the signal handler. If the handler returns, the
> kernel <u>**resumes**</u> the process from <u>**the instruction where it was interrupted**</u>.

...but how?

## Signals

A signal is like a notification sent to a process to inform it that something
~~interesting~~ has occurred:

```c
#define SIGFPE 8    // floating point exception
#define SIGILL 4    // illegal instruction
#define SIGINT 2    // interrupt
#define SIGSEGV 11  // segmentation fault
#define SIGTERM 15  // polite request to terminate
```

Applications can decide either to ignore signals or to handle them by installing
signal handlers using `sigaction()`, or an outdated API, `signal()`.

```c
int sigaction(int sig, const struct sigaction *newact, struct sigaction *oldact);
__attribute_deprecated__ sighandler_t signal(int sig, sighandler_t handler);
```

### When are Signals Delivered?

According to the POSIX standard:

> <h4>Execution of signal handlers:</h4>
>
> Whenever there is a transition from <u>**kernel-mode to user-mode**</u>
> execution, e.g., on:
>
> - return from a system call, or
> - scheduling of a thread onto the CPU.
>
> The kernel checks whether there is a
> pending unblocked signal for which the process has established a
> signal handler. If there is such a pending signal, the following
> steps occur...

Delivering signals is straightforward when it's the case of
the second bullet point, as shown in the code below:

```c
void return_to_userspace()
{
    if (has_pending_signal) {
        save_context();
        setup_signal_frame();
        jump_to_signal_handler();
        unreachable();
    }

    do_return_to_userspace();
    unreachable();
}
```

more about [unreachable()](https://en.cppreference.com/w/c/program/unreachable).

How about the first one?

The first bullet point effectively implies that a signal must occur **after**
a system call returns.

- What if that system call lasts a long time?
- What if, for the worst case, the system call just never returns?

In those cases, how can the kernel deliver signals in time? Before moving on,
let's focus on how a system call is executed, and how long it can last.

### How Long Can a System Call Last?

A system call can last for a long time, for example, `read()` can last
forever if the file is never written to (imagine a named pipe), or the
user never presses any key on the keyboard.

```c
char my_function() {
    // even Alan Turing cannot tell when this will return
    return getchar();
}
```

To make fair and efficient use of the CPU, `read()` and similar syscalls are
not implemented as a busy waiting loop, which just wastes CPU cycles.
Instead, it lets other threads run and comes back only when required resources
become available.

```c
ssize_t read(int fd, void *buf, size_t count) {
  check_resource:
    // ...
    if (!available) {
        // yields the CPU to other threads
        sleep_until_resource_available();
        // woken up by someone, go back and check again
        goto check_resource;
    }
    memcpy(...); // or whatever code to read the resource
}
```

In MOS, a `waitlist` is used to keep track of threads that are waiting for resources:

```c
check_server:
// ...
if (!server_found)
{
    pr_info("no server '%s' found, waiting for it to be created...", name);
    waitlist_append(server_waitlist);
    blocked_reschedule(); // marks the thread as blocked and reschedules

    pr_info("woken up, check again...");
    // woken up by someone, go back and check again
    goto check_server;
}

pr_info("server '%s' found", name);
```

#### What Wakes You Up?

When the resource becomes available, the resource owner will iterate through its
`waitlist` and set the thread's state to `READY`. The thread will then enter the
scheduling candidate queue and is ready to run again in the next scheduling round.

```c
// something like this
void waitlist_wakeup(waitlist_t *waitlist)
{
    list_foreach(thread_t, thread, waitlist->threads) {
        thread->state = READY;
    }
}
```

There seems to be nothing notable. When the thread wakes up, it simply
continues to execute the syscall with its desired resource available.

---

## Signals Ruin Everything

**What Happens When a Signal is Delivered?**

> 'signal handler is invoked...' --- POSIX

In MOS, whenever a signal is delivered to a thread, it is stored in a per-thread
linked list called `sigpending`, and the thread's state is set to `READY`.

```c
// something like this
void signal_deliver(thread_t *thread, int signo)
{
    // ...
    list_append(thread->sigpending_list, signo);
    thread->state = READY;
}
```

To get signals handled as soon as possible, the thread should leave the
syscall and handle the signal first.

**So...**

A check-and-return is added in the syscall to handle wakeups like this. If the
thread is interrupted by a signal, it should return immediately, without executing
further code in the syscall.

```diff
void my_syscall() {
  check_resource:
    bool available = check_resource();
    if (!available) {
        waitlist_append(resource_waitlist);
        blocked_reschedule(); // marks the thread as blocked and reschedules

+         if (signal_has_pending()) {
+             // cleanup and return immediately
+             return -EINTR; // we are interrupted by a signal
+         }

        // woken up by someone, go back and check again
        goto check_resource;
    }

    use_resource();
}
```

The signal handler is then invoked as normal, followed by a `sigreturn` trampoline
to return to a _**'pre-signal context'**_.

### Where are we returning to?

Tl;dr: The instruction after the syscall instruction, with `-EINTR` as the return
value.

The entire flow is like this:

```txt
userspace                 kernel space
|
|- `syscall` instruction
|------------------------>|
                          |- do_syscall_XXX()
                          |- return_to_userspace()
                               |- check_signal_pending() => true
                                   |- save_context()
                                   |- jump_to_signal_handler()
    |<-----------------------------|
    |- signal handler
    |- sigreturn()
    |- `syscall`
    |-------------------->|
                          |- do_syscall_sigreturn()
                          |- return_to_userspace()
                                |- check_signal_pending() => false
                                |- restore_context()
                                |- return_to_userspace()
|<------------------------------|
|- syscall returns
|- ... program continues
```

The program will get the return value of the syscall, but the return value is `-EINTR`,
which means the syscall is interrupted by a signal.

This return value is rarely a desired one, a program in this case should check for
the return value and decide what to do next. If the program wants to retry
the syscall, it should call syscall again with the same arguments.

```c
ssize_t boilerplate_read(int fd, void *buf, size_t count)
{
    ssize_t ret;
    do {
        ret = syscall_read(fd, buf, count);
    } while (ret == -EINTR);
    return ret;
}
```

The code above looks like a boilerplate, and it is. It is also error-prone, as
the programmer may forget to check the return value and retry the syscall.

## Can we do better?

Given the fact that some signals are only 'informative', and has no
side-effects on neither the program nor the kernel, it is safe to restart
the interrupted syscall automatically after signal handler returns.

Examples of these signals are `SIGCHLD` and `SIGWINCH`.

Is there a way to restart such syscalls automatically, so that programmers don't
have to deal with `-EINTR` return values?

Short answer: Yes.

### SA_RESTART and siginterrupt(3p)

In POSIX, one can tell the kernel to restart the interrupted syscall automatically
upon receiving a signal by calling `siginterrupt(3p)`. Or by setting the `SA_RESTART`
flag in `sigaction(2)`.

```c
int siginterrupt(int sig, int flag);

struct sigaction {
    // ...
    int sa_flags;
    // ...
};

handler_t sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
```

In MOS (specifically, in `mlibc`). The first one is a compatibility wrapper of
the latter.

### What does the kernel need to know, to restart the syscall?

The kernel needs to know at least these things to restart the syscall:

1. The syscall is **interrupted** by a signal.\
   This one is easy, upon receiving a signal, syscall handlers can return `-EINTR`
   to indicate that the syscall is interrupted by a signal.

2. The user **wants to** restart an interrupted syscall for **this signal**.\
   This is also trivial, the kernel can simply check the `SA_RESTART` flag in
   the `sigaction` struct.

3. The exact syscall number and arguments to restart.\
   This is kinda easy, they are already in the registers when the syscall is
   interrupted.

4. The syscall is **'restartable'**, i.e. the syscall makes sense to be restarted.\
   This is the most difficult one. A special return value is needed to distinguish
   whether the syscall wants itself to be restarted or not.

   `-EINTR` is not a good choice, we need a new return value.

### -ERESTARTSYS Comes to the Rescue

In Linux, the kernel returns `-ERESTARTSYS` to indicate that the syscall is
interrupted by a signal, and the syscall is restartable. This is also the case
in MOS.

```c
// in Linux
if (signal_pending(current))
    return -ERESTARTSYS;

// in MOS
if (signal_has_pending(current))
    return -ERESTARTSYS;
```

The signal checker is also modified to check if `-ERESTARTSYS` is returned and if
`SA_RESTART` is set:

```c
reg_t real_ret = syscall_ret;
if (real_ret == (reg_t) -ERESTARTSYS) {
    // interrupted by a signal
    real_ret = -EINTR; // we don't leak -ERESTARTSYS to userspace
    if (action.sa_flags & SA_RESTART)
        // restart the syscall
        set_syscall_restart(regs, syscall_nr);
    else
        // return -EINTR
        set_syscall_result(regs, real_ret);
} else {
    // normal return value
    set_syscall_result(regs, real_ret);
}

handle_signal();
```

Note that `-ERESTARTSYS` is not leaked to userspace, it is solely used by the
kernel to indicate that the syscall is interrupted by a signal and is restartable.

What happens here is:

1. after a syscall function detects that the syscall is interrupted by a signal
   - it returns `-ERESTARTSYS` or `-EINTR`
   - based on whether it is restartable or not
2. before returning to userspace, signal handling code checks:
   - if a signal is pending, yes in this case
   - if `SA_RESTART` for this signal is set
     - if yes, modify the context so that the syscall can be restarted
       - automatically (see below)
     - if no, return `-EINTR` to the userspace program (instead of `-ERESTARTSYS`)
       - because the user does not want to restart the syscall
3. invoke the signal handler
4. if the signal handler returns by `sigreturn()`
   - restore pre-signal context
   - return to userspace
5. the syscall is restarted (automatically) if `SA_RESTART` is set

### How This Works?

The `set_syscall_restart()` function is modifies the context of the thread
(i.e. registers) so that the syscall can be restarted automatically.

It achieves this by playing with the instruction pointer:

```c
void set_syscall_restart(regs_t *regs, int syscall_nr)
{
    // in MOS, syscall_nr is in rax
    regs->rax = syscall_nr;
    regs->rip -= 2;
}
```

It places the syscall number in `rax`, the register that stores the syscall number
in MOS, and decrements the instruction pointer by 2.

```asm
int 0x88    ; 2 bytes (0xcd 0x88)
syscall     ; 2 bytes (0x0f 0x05)
```

2 in this case is the length of the `syscall` instruction (or `int 0x88`) when encoded
in x86-64. Whether it is a coincidence or not, the lengths of these two instructions
are the same.

After the context has been modified, when the thread returns to userspace, instead
of going to the next instruction, it will execute the `syscall` (or `int 0x88`)
instruction again. From the outside, it looks like the syscall is restarted
automatically.

The overall flow of automatic syscall restart is in the diagram below:

```diff
 userspace                 kernel space
 |
 |- `syscall` instruction
 |------------------------>|
                           |- do_syscall_XXX()
+                          |  // returns -ERESTARTSYS
                           |- return_to_userspace()
                                |- check_signal_pending() => true
+                                   |- set_syscall_restart() // ip -= 2
                                    |- save_context()
                                    |- jump_to_signal_handler()
     |<-----------------------------|
     |- signal handler
     |- sigreturn()
     |- `syscall`
     |-------------------->|
                           |- do_syscall_sigreturn()
                           |- return_to_userspace()
                                 |- check_signal_pending() => assumed false
                                 |- restore_context()
                                 |- return_to_userspace()
 |<------------------------------|
+|- `syscall` instruction (again)
+|------------------------>|
+                          |- do_syscall_XXX()
+                          |  // returns normally
+                          |- return_to_userspace()
+                                |- check_signal_pending() => assumed false
+                                |- restore_context()
+                                |- return_to_userspace()
+|<------------------------------|
 |- syscall returns
 |- ... program continues
```

### Why mangling the instruction pointer?

Several other approaches exist, for example:

1. store the syscall number, arguments somewhere in the thread's context, and call
   the syscall in kernel mode from `sigreturn()`

   - it requires more space to store all arguments (10+ registers \* 8 bytes)
   - it is also unnecessary to store them somewhere else, given they are already
     in the registers when the syscall is interrupted.
   - syscall entry point is sometimes a tracepoint, it helps the kernel to
     trace syscalls' entry and exit, and it's unsuitable to be invoked from
     kernel mode code.

2. place the `-EINTR` return value checker in `libc` and restart the syscall
   from there

   - it requires `libc` to be aware of the syscall restart mechanism
   - it is not a good idea to put syscall restart logic in `libc`, as it is
     a userspace library, and it is not the only one. Caller libraries may
     also want to handle `-EINTR` return values differently.

### Concerns and Limitations

1. Imagine a syscall that takes a pointer to a buffer as its argument, and the
   buffer is modified by the syscall. If the syscall is interrupted by a signal,
   and the syscall is restarted automatically, the buffer will be modified twice.

   - This senario just doen't exist. `-EINTR` and `-ERESTARTSYS` are only returned
     when the syscall is interrupted and when the syscall haven't done anything
     yet. Examples of these syscalls includes `read(2)`, if any data is read,
     the syscall will return immediately, with the number of bytes read instead
     of `-EINTR` or `-ERESTARTSYS`.

2. What if the syscall is restarted automatically, and the signal interrupts it
   again?

   The syscall function doesn't know whether it is restarted automatically or
   it's the first time it is called. Interrupting it again will result in
   the same behaviour as interrupting it the first time.

3. Certain system calls are not restartable, for example, `nanosleep(2)`,
   `nanosleep(2)` takes a `struct timespec` as its argument, and the kernel needs
   to calculate the remaining time to sleep and restart the syscall with the
   remaining time.

## Conclusion

In this post, signals, and how they interrupt syscalls are discussed. The
automatic restart of signal-interrupted syscalls is also discussed.

The MOS kernel implements automatic restart of signal-interrupted syscalls
in commit [5ba8cc0b5935c608cf4490cc6035ab30649b8db3](https://github.com/moodyhunter/MOS/commit/5ba8cc0b5935c608cf4490cc6035ab30649b8db3)

---

## References

- MOS Source Code
- Linux Source Code
- [POSIX.1-2017](https://pubs.opengroup.org/onlinepubs/9699919799/)
- `signal(7)`
