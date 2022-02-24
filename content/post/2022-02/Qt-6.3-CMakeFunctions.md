---
title: "Qt 6.3 中的一些 CMake 函数"
date: 2022-02-24T22:56:28Z
draft: true
---

# 写在前面

今天心血来潮想写点东西，发现之前挖的各种大坑都没填好…… dbq）

# 说点什么呢

翻消息记录突然看到下面这条评论：

> 作者写的挺好的，如果能把下面这个官网上面对这个模块功能描述加进去完美了...

哇） 居然在认真的看我的 Blog，~~突然有点被感动到~~。 那么今天就先来聊 Qt 6.3 中的 CMake 函数

## 1. qt6_add_qml_module

先来说说大名鼎鼎（？）的 `qt6_add_qml_module`：

细数近期的 Qt Declarative 库中的 [commits](https://github.com/qt/qtdeclarative/blame/9b56042f8445fa8c8119cf2d980d8a1698950483/src/qml/Qt6QmlMacros.cmake) 不难发现，Qt 还在给这个本就已经很复杂的函数添加参数：`FOLLOW_FOREIGN_VERSIONING`，`SKIP_TYPE_REGISTRATION`，`NO_IMPORT_SCAN`，`NO_RESOURCE_TARGET_PATH`，`NO_PLUGIN`

### 这都啥玩意？

#### a. FOLLOW_FOREIGN_VERSIONING

> 没用过，也没看懂 Doc 在说什么

#### b. SKIP_TYPE_REGISTRATION

震惊！这个函数居然没有对应官方文档！ 

根据相关 [commit](https://github.com/qt/qtdeclarative/commit/caa062e30a719911d88c9197c4783f5bff50f044#diff-73cbd2ad4c1d08551953953d4a049d0673d3ec1041c1e2acbd77536896c93bf0R64) 记载：
如果指定了参数，那么生成出的 `qmldir` 文件中将不会出现任何 QML 文件中的类型。

意思就是运行时 "我想手动注册自己的类型" （使用 `qmlRegister[Anonymous|Singleton]Type<T>(...)` 一类的函数）

#### c. NO_IMPORT_SCAN

理论上只适用于静态编译的 Qt （因为 Shared Qt 是支持加载动态链接的插件的（QML 组件高强度依赖 Qt Plugin System），而静态无法进行运行时加载 DLLs）

默认情况下，静态编译的 Qt 会在执行 CMake 时运行 `qmlimportscanner` 扫描指定目录下所有 `qml` 文件的 `import` 列表，从而列出需要进行链接的库
（比如 QtCharts, QtWebEngine, QtPositioning, ~~QtGraphicalEffects~~），这些库将会以 `target_link_libraries` 的形式链接到依赖他们的
QML 模块中，从而进行 「另一种形式的运行时加载」（可能会再水一篇说这个？）

加了 `NO_IMPORT_SCAN` 就不一样了，啥也不干，全靠自己（所以完全有可能在运行时产生 `Module .... is not installed.` 错误）。

#### d. NO_RESOURCE_TARGET_PATH

~~是一个偷懒小技巧~~：仅适用于 Executable Target

一般情况下，假设有一个 QML Module 的 URI 是 `mymodule.mooody.me`，那么对应的 QML 文件应该位于 `Prefix/mymodule/mooody/me/*.qml`。
（也就是将 URI 中的 `.` 替换为目录分隔符），加了此参数后，所有的 QML 文件将会直接被放到 `Prefix/` （没有很长一段 URI 了）

Qt 文档说：`...which may only be used if the backing target is an executable...`

确实如此，在调用 `QQmlApplicationEngine::load(const [QUrl|QString] &)` 时，不再需要指定过于冗长的 URI 了。

#### e. NO_PLUGIN

前一篇文似乎说过 Backing Target 和 Plugin Target 的区别，此参数会让 Qt "不再" 生成一个 Plugin Target，并且直接链接到 Backing Target

## 2. QT_ANDROID_ABIS

经历了万水千山，从 Qt5 中加入 MultiAbi 到 Qt6 `qmake` 转 CMake 时万不得已 Drop 掉 MultiAbi

现在多 ABI 支持又回来了（以一种看起来极为 Hacky 的方式……）

~~不太干净，不想说~~

## 3. qt_target_compile_qml_to_cpp

是 `qmltc`！

用法大概就是 `qt_target_compile_qml_to_cpp(qmltc_test FILES item.qml)` 能将 QML 文件生成为 C++ 类，并从 C++ side 创建和修改

注意：在一月份测试时，QMLTC 生成的类并不能正常操作从 `QQmlApplicationEngine` （也就是常规方式）初始化的 QML 对象

不过还是蛮好玩的

## 4. qt_standard_project_setup

顾名思义：`Qt 标准项目设置`

### 设置啥了？

1. 导入了 `GNUInstallDirs` （也就是 `CMAKE_INSTALL_??DIR`）
2. 对非 Windows 亦或 macOS （Linux / Android / iOS / WASM / ……） 设置合理的 RPATH
3. 设置了 `CMAKE_AUTOMOC` 和 `CMAKE_AUTOUIC` 为 `TRUE`

问：怎么没有 `CMAKE_AUTORCC`？
答：新时代了，快去用 `qt_add_resources`，别手写 `qrc` 了

-------

今天大概就罗嗦这么多，愿天下太平

Seeya `:)`