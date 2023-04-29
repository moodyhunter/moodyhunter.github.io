---
title: 我是如何花了一年也没写出一个 OS 的
slug: my-os-in-one-year
date: "2023-04-29T00:00:00.000Z"
tags:
  - MOS
  - OS
  - Operating System
  - 坑
---

本文包含大量暴论，不喜轻喷

---

从去年 7 月 7 号的 [initial commit](https://github.com/moodyhunter/MOS/commit/c3c498127db2c1c04ed3abe32d7902f562c09086)
到现在 2023 年 4 月 29 日已经过去快一年了。

在这一年里，MOS 支持了很多功能，但又有更多没有实现的功能。

~~谨此文面向读者吐槽一下~~

## 1. 从 Bootloader 开始

2023 年了谁写 kernel 还从实模式引导扇区开始写？

- 只能说部分教程过分陈旧，或者就是拘泥于 x86 的传统不愿改变，
- x86 从实模式启动到保护模式最终切长模式这个过程完全就应该有标准的规范，而不是要自己造轮子
  - 谁愿写 GDT？
- 鄙人认为引导扇区里的代码就不要拿出来当做 kernel 讲

## 2. 沉迷 x86 无法自拔

Please no,

- BIOS / EBDA：？？？exm？？？ 找这俩东西还要去从内存里搜索 magic string
- 神秘的 Serial COM Port IO ��
- 配好了 PIC 但是还是要去配 APIC
- Segmentation：没用，但是用了
  - 看看你的 cs 低两位是什么东西
  - 看看你的 `fsbase` / `gsbase`
- APIC，但是 x2APIC
- RSDT，但是 XSDT
  - 那如果我没 PAE，那我怎么访问 XSDT？
- SMP：哈哈哈你随便 IPI 能启动起来算我输
  - `APIC_DELIVER_MODE_INIT，然后`
  - 睡 50ms，然后
  - `APIC_DELIVER_MODE_INIT_DEASSERT，然后`
  - 再睡 50ms，再然后
  - `APIC_DELIVER_MODE_STARTUP，然后`
  - 睡 50ms，然后
  - 又发一遍 `APIC_DELIVER_MODE_STARTUP？`
- 你中断和异常一开始黏在一起就算了，还要我自己去分开？
- ACPI：AML 语言的 parser 似乎只有两个

## 3. 重造 path resolver

早期写的 path resolution 完全是错误的，直到后来要支持 symlink 才发现大量问题

- 但是这种情况下的 refcount 真的好难做

## 4. 物理内存分配

物理内存被表示为 `pmlist_node_t`，但是要分配这个结构体还需要分配内存，这是一个很大的问题

咋办呢

## 5. 时间不够

嗯

## 6. 没了

{{<spoiler>}}你看出来我就是想吐槽 x86 了吗{{</spoiler>}}
