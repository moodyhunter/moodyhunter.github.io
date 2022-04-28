---
title: "QProcess in qemu"
date: 2022-04-28T15:44:00+01:00
---

It's been a long time since my promise to publish a detailed write-up on what the hell was
actually going on inside qemu.

Qt, for its Unix QProcess implementation, utilises [forkfd library](https://doc.qt.io/qt-6/qtcore-attribution-forkfd.html), (See also: <https://github.com/qt/qtbase/tree/dev/src/3rdparty/forkfd>) as a helper library to fork subprocess.

`pidfd` is a new kernel feature, which has just been added to the kernel upon v5.2.

Since [this merge](https://codereview.qt-project.org/c/qt/qtbase/+/313894), Qt added a new
configuration feature (`forkfd_pidfd`, "CLONE_PIDFD support in forkfd") and it was default
to ON on Linux.

When this feature is ON, Qt does nothing special, forkfd library selectively uses either
`forkfd()` or `fork()` based on kernel version: (it goes into
[this branch](https://github.com/qt/qtbase/blob/dev/src/3rdparty/forkfd/forkfd.c#L654-L656)
and then
[sets CLONE_PIDFD here](https://github.com/qt/qtbase/blob/dev/src/3rdparty/forkfd/forkfd_linux.c#L150)
before calling
[clone](https://github.com/qt/qtbase/blob/dev/src/3rdparty/forkfd/forkfd_linux.c#L68))

OTOH, if the feature is OFF, Qt sets the `FFD_USE_FORK` flag (say: forkfd please use `fork`
anyway) when calling `::forkfd()`, which will cause forkfd to use `fork()` direcly.

Under regular circumstances, this works well, forkfd is smart enough to correctly detect the
kernel version and use the correct syscall, however, things go south fast when `qemu-user`
steps in:

1. Qt has no `FFD_USE_FORK` set, and
2. forkfd thinks the kernel has correct `forkfd` syscall implementation (because it should have), and
3. **QEMU has no support for `clone`-ing with `CLONE_PIDFD` yet (there's a [possibly abandoned patch](https://patchew.org/QEMU/mvm4kadwyrm.fsf@suse.de/))**

(original blog post: [记一次-debug-qmake](https://www.mooody.me/p/记一次-debug-qmake/))
