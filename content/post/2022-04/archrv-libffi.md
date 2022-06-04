---
title: "ArchRV - BuggyFFI"
date: 2022-04-19T23:09:10+01:00
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
- 长版本：位于 `./testsuite/libffi.call/strlen.c` 处的单元测试第 32 行：

```C
30:  s = "a";
31:  ffi_call(&cif, FFI_FN(my_strlen), &rint, values);
32:  CHECK(rint == 1);
```

的 `CHECK` 失败了，顾名思义就是 `rint` 不等于 `1` 了。

## libffi 测试为什么炸了？

明天再写 :)
