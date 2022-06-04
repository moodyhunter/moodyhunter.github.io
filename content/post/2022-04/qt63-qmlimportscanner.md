---
title: "Qt 6.3 中 QML 的坑"
date: 2022-04-02T13:43:37+01:00
---

## 估计是 Regression 了

### `qmlimportscanner`

我也不知道为什么，由 `androiddeployqt` 调用的 `qmlimportscanner` 找不到就在本目录下的 `QtGraphicalEffects` QML Module。

### Android 平台黑屏卡死

其次，当使用 Qt 6.3 编译出的 Android APK 时，我的 `MoodyAPI Client` 一旦退出就无法再进入界面（Sigh

### 解决方法

~~回退到 Qt6.2~~

> Press F to pay respects.
