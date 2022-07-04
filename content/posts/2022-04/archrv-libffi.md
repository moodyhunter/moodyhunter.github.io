---
title: "ArchRV - BuggyFFI"
date: "2022-04-19T23:09:10+01:00"
tags:
  - "ArchRV"
  - "坑"
---

> EXPAND MY INTEGER!

很多人看到 `BuggyFFI` 会很不解：“什么 FFI，你再说什么？”

我说的是 [`libffi`](https://github.com/libffi/libffi)

## libffi 是什么？

> A portable foreign-function interface library.

说人话就是：[……算了你自己去看吧](https://github.com/libffi/libffi#what-is-libffi)

## libffi 怎么了？

- TLDR：测试炸了
- AKA：位于 `./testsuite/libffi.call/strlen.c` 处的单元测试代码第 32 行：

```c
30:  s = "a";
31:  ffi_call(&cif, FFI_FN(my_strlen), &rint, values);
32:  CHECK(rint == 1);
```

的 `CHECK` 失败了，顾名思义就是 `rint` 不等于 `1` 了。

## libffi 测试为什么炸了？

首先来看代码（经过简化）：

```C++
// testsuite/libffi.call/strlen.c
size_t ABI_ATTR my_strlen(char *s) { return (strlen(s)); }

int main (void)
{
  // 省略了部分无关代码
  char *s = "a";

  void *values[MAX_ARGS];
  values[0] = (void*) &s;

  ffi_arg rint;
  ffi_call(&cif, FFI_FN(my_strlen), &rint, values);
  CHECK(rint == 1);
}
```

经过两分钟的代码观测，从以上代码块 L12 来看，`rint` 是一个 `ffi_arg` 类型的变量，经过阅读 header 可以得知，`ffi_arg` 是一个 64 位无符号整数类型：

```C++
// src/riscv/ffitarget.h
typedef unsigned long ffi_arg;
typedef   signed long ffi_sarg;
```

首先经过 `gdb` 在对应的 main 函数打断点并尝试复现问题：

```bash
Breakpoint 1, main () at ./strlen.c:16
(gdb) n // 狂躁的几次 n
(gdb) p rint
# 这个 rint 在调用前是一个没经过 0 初始化的变量
# 因此内存里面有脏东西很正常
$1 = 72057593903531392

(gdb) p s
$2 = 0xaaaaaaaaaaab78 "a"

(gdb) n
31        ffi_call(&cif, FFI_FN(my_strlen), &rint, values);
(gdb) n
(gdb) p rint
# 但如此看来，调用后的 rint 则不应该出现这么大的 length，毕竟我们的 s 只有一个字符 "a"
$3 = 72057589742960641
```

看来问题核心出在了 `ffi_call` 函数里。

重启调试，使用 s 指令进入 `ffi_call` 函数继续跟进。发现这东西转头调用了 `ffi_call_int`，而
`ffi_call_int` 是个不短的函数，~~于是决定，还是先看代码再跑 gdb~~

<https://github.com/libffi/libffi/blob/464b4b66e3cf3b5489e730c1466ee1bf825560e0/src/riscv/ffi.c#L331>

仔细看下来，与 `rvalue` 相关的几段代码只有以下内容：

```C++
static int passed_by_ref(call_builder *cb, ffi_type *type, int var) {
#if ABI_FLEN
   if (!var && type->type == FFI_TYPE_STRUCT) {
       float_struct_info fsi = struct_passed_as_elements(cb, type);
       if (fsi.as_elements) return 0;
   }
#endif

   return type->size > 2 * __SIZEOF_POINTER__;
}

static void ffi_call_int (
  // 省略了无关参数
  void *rvalue,       // 返回值（需要关注）
) {
    // 省略了部分无关代码
    size_t rval_bytes = 0;
    if (rvalue == NULL && cif->rtype->size > 2*__SIZEOF_POINTER__)
        rval_bytes = FFI_ALIGN(cif->rtype->size, STKALIGN);

    // ......

    if (rval_bytes)
        rvalue = (void*)(alloc_base + arg_bytes);

    // ......

    int return_by_ref = passed_by_ref(&cb, cif->rtype, 0);
    if (return_by_ref)
        marshal(&cb, &ffi_type_pointer, 0, &rvalue);

    // 省略准备参数的一个 for 循环...

    ffi_call_asm ((void *) alloc_base, cb.aregs, fn, closure);

    if (!return_by_ref && rvalue)
        unmarshal(&cb, cif->rtype, 0, rvalue);
}
```

即使是省略了其他内容，这段代码，一眼看下去还是感觉很难入手……

但有两点值得注意：

1. 在上文 L17 定义的 `rval_bytes` 在本次测试中会恒等于 `0`，因为
   - `rvalue` 传入值不会是 `NULL`（我们知道调用方给了地址的）
2. `passed_by_ref` 恒返回 0，因为
   - 那个 ABI_FLEN 宏究竟是多少不重要（因为我们的 `type->type` 不是 `FFI_TYPE_STRUCT`）
   - 同时 `type->size > 2 * __SIZEOF_POINTER__` 不成立（因为我们的 `type` 是 `ffi_arg`）

基于以上两点，`ffi_call_int` 函数还就被简化为了以下两行：

```C++
// 实际进行调用：
ffi_call_asm ((void *) alloc_base, cb.aregs, fn, closure);
unmarshal(&cb, cif->rtype, 0, rvalue);
```

上面那个 `ffi_call_asm` 定义在一个 assembly 文件里，由于并不是所有的单元测试都炸，我暂且先 assume 其能工作，因此来看 `unmarshal` 函数：

<https://github.com/libffi/libffi/blob/464b4b66e3cf3b5489e730c1466ee1bf825560e0/src/riscv/ffi.c#L254>

以下内容仍有省略，源文件在

```C++
// 这段内容不在 c 文件里，为了方便阅读贴过来了
#if __SIZEOF_POINTER__ == 8
    #define IS_INT(type) ((type) >= FFI_TYPE_UINT8 && (type) <= FFI_TYPE_SINT64)
#else
    #define IS_INT(type) ((type) >= FFI_TYPE_UINT8 && (type) <= FFI_TYPE_SINT32)
#endif

static void *unmarshal(call_builder *cb, ffi_type *type, int var, void *data) {
    size_t realign[2];
    void *pointer;

    // ...省略了对于 FFI_TYPE_STRUCT 和浮点小数点的处理

    if (type->size > 2 * __SIZEOF_POINTER__) {
        // 省略了对于大数据类型的处理（我们的 `type` 是 `ffi_arg`，不够大）
    } else if (IS_INT(type->type) || type->type == FFI_TYPE_POINTER) {
        unmarshal_atom(cb, type->type, data);
        return data;
    } else {
        // 整个 else 不会执行，因为上面的 if 已经满足了：IS_INT(type->type)
    }
}
```

这个 `unmarshal` 函数似乎没有什么值得关注的，因此继续进入 `unmarshal_atom`：

```C++
static void unmarshal_atom(call_builder *cb, int type, void *data) {
    size_t value;

    // 省略浮点小数点的处理

    if (cb->used_integer == NARGREG) {
        value = *cb->used_stack++;
    } else {
        value = cb->aregs->a[cb->used_integer++];
    }

    switch (type) {
        case FFI_TYPE_UINT8: *(uint8_t *)data = value; break;
        case FFI_TYPE_SINT8: *(uint8_t *)data = value; break;
        case FFI_TYPE_UINT16: *(uint16_t *)data = value; break;
        case FFI_TYPE_SINT16: *(uint16_t *)data = value; break;
        case FFI_TYPE_UINT32: *(uint32_t *)data = value; break;
        case FFI_TYPE_SINT32: *(uint32_t *)data = value; break;
#if __SIZEOF_POINTER__ == 8
        case FFI_TYPE_UINT64: *(uint64_t *)data = value; break;
        // 正常情况下应该调用此 case：
        case FFI_TYPE_SINT64: *(uint64_t *)data = value; break;
#endif
        case FFI_TYPE_POINTER: *(size_t *)data = value; break;
        default: FFI_ASSERT(0); break;
    }
}
```

先写到这，剩下的过段时间再说（遇到了另一个奇怪问题）
