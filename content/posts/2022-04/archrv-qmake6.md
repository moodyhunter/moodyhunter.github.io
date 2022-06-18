---
title: "记一次 debug qmake"
date: "2022-04-14T00:32:37+01:00"
tags:
  - "Qt 6"
  - "qmake"
  - "ArchRV"
  - "坑"
---

> 原本想甩锅 Qt，但后来发现~~小丑竟是 qemu~~ :)

## 正片开始

初来乍到 `PLCT::archrv-pkg`，作为新人总想快点贡献点什么，于是各种翻 build log。

qalculate-qt:

```log
/usr/lib/qt6/mkspecs/features/toolchain.prf:76: Variable QMAKE_CXX.COMPILER_MACROS is not defined.
Project ERROR: failed to parse default search paths from compiler output
```

打包机扔出了这么个错误，深受 qmake 其害的我看到这就准备开 [bugreports.qt.io](https://bugreports.qt.io)

但是转头一想：肯定群里的大佬早就看到了这个问题，应该已经在 WIP 了吧，于是在群消息记录里搜了一下，
发现果然早在上个月 24 号（两周前左右）就在讨论这个问题了。

可惜没能解决，翻记录说是 qemu 里独有的问题，换到物理机上就没事了。

抱着试一试万一能给 Qt 水个 bugreport 的心态，我打开了这个文件：`features/toolchain.prf` 并找到第 76 行：

```qmake
73:         cache($${target_prefix}.$$v, set stash, $$v)
74:         $${target_prefix}.COMPILER_MACROS += $$v
75:     }
76:     cache($${target_prefix}.COMPILER_MACROS, set stash) <- 这里
77: } else {
78:     # load from the cache
```

很明显是一个 qmake 变量 "`某些东西.COMPILER_MACROS`" 的解引用（取值）。

根据报错能看出 `某些东西` 应该是 `QMAKE_CXX`，于是继续向上翻：

```qmake
66: for (v, vars) {
67:    !contains(v, "[A-Z_]+ = .*"): next()
68:     # Set both <varname> for the outer scope ...
69:     eval($$v)
70:     v ~= s/ .*//
71:     isEmpty($$v): error("Compiler produced empty value for $${v}.")
72:     # ... and save QMAKE_(HOST_)?CXX.<varname> in the cache.
73:     cache($${target_prefix}.$$v, set stash, $$v)
74:     $${target_prefix}.COMPILER_MACROS += $$v
75: }
76: cache($${target_prefix}.COMPILER_MACROS, set stash) <- 核爆中心
```

稍有常识的人都知道这是个 for loop，根据 L74 可以看出这个循环是用来向
`COMPILER_MACROS` append 一些值，我想起了 log 调试大法，于是眼疾手快就在这附近加了
几个 `message("....")`。

随着 qmake6 重新运行，我发现在 `for` 循环里并没有任何输出出现，这使人不禁怀疑这个
`for` 循环究竟有没有跑。带着这个问题，又在 `for` 上面一行输出 `vars`，也就是循环对象。

我还特地打了引号，避免输出的是个空格然后消失在 Konsole 大海中，结果发现这个 `var` 里啥也
没有。

> 咋就成空的了？

继续向上翻，整段代码被包在了一个 `isEmpty($${target_prefix}.COMPILER_MACROS) {`
分支，说明这个变量为空，其实是在预期情况内。紧接着是两个分支：`msvc` 和 `gcc|ghs` 很明显
应该是走 `gcc` 分支：

```qmake
63: } else: gcc|ghs {
64:     vars = $$qtVariablesFromGCC($$QMAKE_CXX)
65: }
```

找到 `vars` 的来源了！这里调用了一个名为 `qtVariablesFromGCC` 的函数，传入单个参数
`$$QMAKE_CXX`，返回一个 list （也就是 `vars`），于是继续跟进函数去 debug。

函数定义很简单（节省地方稍微改了下缩进）：

```qmake
37: defineReplace(qtVariablesFromGCC) {
38:   ret = $$system("$$1 -E $$system_quote($$PWD/data/macros.cpp) \
39:     2>$$QMAKE_SYSTEM_NULL_DEVICE", lines, ec)
40:   !equals(ec, 0): qtCompilerError($$1, $$ret)
41:   return($$ret)
42: }
```

函数开头第一句话一个 `system`，意思是要调用个子进程，然后读取输出。40 行的判断 ec 按照
标准缩写应该是 `error code`，先猜应该是个返回值。

于是开始 `message()` 大法。

于是发现这 `ret` 为空，我将 `system()` 的参数单独拿出来跑：

```bash
# $$1 -E $$system_quote($$PWD/data/macros.cpp) 2>$$QMAKE_SYSTEM_NULL_DEVICE
# 参数替换可得
$ c++ -E $(pwd)/data/macros.cpp 2>/dev/null
> # 0 "./macros.cpp"
> # 0 "<built-in>"
> # 0 "<command-line>"
> # 1 "/usr/include/stdc-predef.h" 1 3 4
> # 0 "<command-line>" 2
> # 1 "./macros.cpp"
>
> QT_COMPILER_STDCXX = 201703L
> # 26 "./macros.cpp"
> QMAKE_GCC_MAJOR_VERSION = 11
> QMAKE_GCC_MINOR_VERSION = 2
> QMAKE_GCC_PATCH_VERSION = 0
```

就是预处理结果嘛。。。

在进行了两分钟（没计时）的思考后，我认为是 qmake 没能正确执行 `system` 函数，于是还是去
了 [bugreports.qt.io](https://bugreports.qt.io) 搜索 `qmake system`

> [**QTBUG-98951: qmake system() does not work under qemu-user**](https://bugreports.qt.io/browse/QTBUG-98951)

这不就是我遇到的问题吗？正文里说：

> work-around: configure with -no-feature-forkfd_pidfd

`no-feature-forkfd_pidfd` 是什么 feature？ 于是我继续按关键字搜索，找到了 [Make usage of forkfd_pidfd in QProcess a configurable feature (#313894)](https://codereview.qt-project.org/c/qt/qtbase/+/313894) 中的一处改动：

```cpp
// QTBUG-86285
#if !QT_CONFIG(forkfd_pidfd)
    ffdflags |= FFD_USE_FORK;
#endif
```

那么关掉 `forkfd_pidfd` 就使 `!QT_CONFIG(forkfd_pidfd)` 为 `TRUE`，从而强制添加
`FFD_USE_FORK` flag 给了 `ffdflags`，而这就应该是避开了 qemu 未能实现的一个功能吧。

另外感谢肥猫找到了对应给 qemu 的 [patch](https://patchew.org/QEMU/mvm4kadwyrm.fsf@suse.de/)，但很可惜的是没能合并。

在经历了一番折腾后，我在本机应用了给 qemu 的 patch，发现 qmake 的 `system` 果然能正确执行了。

至此，一个由 `qalculate-qt` 引发的，被以为是 Qt bug 的，但实际上是 qemu 的问题终于
真相大白 :) 撒花！

## 小结

以后遇到问题还是要先搜索，虽说本次 qmake 属于极端边角问题：

- 首先是 qmake 现在（应该？）很少人用了吧？
- 其次还是 qemu-user （非 system，上篇文章说道 ArchRV 还没有能跑的 img）

但确实仍然有人先遇到了，而且进行了汇报和 patch

下一篇文章（可能）写写 Qt Webengine 的 patch 过程（如果我能打出来的话）
