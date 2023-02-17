---
title: 记一次修理 evdi
slug: evdi-nvidia-fix
date: "2023-02-17T00:00:00.000Z"
draft: true
tags:
  - "Linux"
  - "坑"
  - "Nvidia"
---

已经很长时间没用过 evdi 了。最近把一直在用的笔记本弄坏掉，就只能被迫切换到备用机，发现了一系列问题：

## 问题出现

现在笔记本有三个显示器：

- 一个笔记本内置屏幕 eDP
- S1: 一个能用 USB-C / HDMI 的 4K 屏
- S2: 一个能用 USB-C 或者 USB-A 的 1080p 屏幕

电脑上有四个可用接口：USB-A，USB-C，HDMI，mDP
其中：

- HDMI 和 mDP 通过 NVIDIA 显卡进行输出
- USB-C 走 iGPU
- USB-A 是 evdi 内核模块

### 那么，可能的 5 种连接方案就是

1. S1 = USB-A，S2 = USB-C
2. S1 = USB-A，S2 = HDMI
3. S1 = USB-A，S2 = mDP 转 HDMI
4. S1 = USB-C，S2 = HDMI
5. S1 = USB-C，S2 = mDP 转 HDMI

前几天已经测出这笔记本的 Type-C 口接屏幕会时不时黑屏，感觉这个问题将无解。

- 所以任何时候方案 1/4/5 都不能用

如果使用 Prime，NVIDIA 官方驱动的两个输出会掉成 10fps

- 所以此时方案 2/3/4/5 不能用，外加上一条 1 无法使用\
  相当于用不了

## 因此只剩下了

- NVIDIA 独显直连
- nouveau PRIME {{<spoiler>}}不会有人还用 nouveau 吧{{</spoiler>}}

于是去固件设置里调成了 PEG Graphics，保存并重启，boom……

## evdi 驱动的屏幕不亮了

倒也不是不亮，而是处于一种有信号输入，但信号是纯黑色的状态。

打开 `dmesg` 一眼映入眼帘的是一串 Kernel NULL pointer deref
