---
title: "Qt6 Porting Guide - CMake"
date: "2021-09-20"
tags:
  - "Qt 6"
  - "cmake"
---

~~随着 Qt6.2 进入 rc 阶段~~ (Update: 24 Feb 2022: 其实现在 6.3 都出 Beta 了)，标志着 Qt6 各模块已经趋近完整，可以看到身边越来越多的项目开始了从 Qt5 迁移到 Qt6 ~~繁重工作~~，
作为从 Qt6 alpha 还没发就尝试迁移的~~资深人员~~。鄙人自认为在 Qt6 / CMake 方面算是比较了解。

## 直入主题

不同于 6.0，Qt 6.2 包含了更多的 CMake API，本文就要说说个人认为最晦涩难懂的 [`qt_add_qml_module`](https://doc-snapshots.qt.io/qt6-dev/qt-add-qml-module.html#qt6-add-qml-module)：

`qt_add_qml_module` 非常复杂，单参数就有 26 个，首先来看一下这个巨大函数的 signature

```cmake
qt_add_qml_module(
    # 必需参数
    target
    URI uri
    VERSION version

    # 可选参数
    [PAST_MAJOR_VERSIONS ...] [STATIC | SHARED] [PLUGIN_TARGET plugin_target]
    [OUTPUT_DIRECTORY output_dir] [RESOURCE_PREFIX resource_prefix] [CLASS_NAME class_name]
    [TYPEINFO typeinfo] [IMPORTS ...] [OPTIONAL_IMPORTS ...] [DEPENDENCIES ...] [IMPORT_PATH ...]
    [SOURCES ...] [QML_FILES ...] [OUTPUT_TARGETS out_targets_var] [DESIGNER_SUPPORTED]

    # No 系列可选参数
    [NO_PLUGIN_OPTIONAL] [NO_CREATE_PLUGIN_TARGET] [NO_GENERATE_PLUGIN_SOURCE] [NO_GENERATE_QMLTYPES]
    [NO_GENERATE_QMLDIR] [NO_LINT] [NO_CACHEGEN] [NO_RESOURCE_TARGET_PATH]
)
```

---

### 必需参数

在这 26 个奇奇怪怪的参数中，有 3 个必需参数（`target`，`uri`，`version`）

#### Target

对于这个 Target 参数，要提供的是一个符合 CMake 命名规范的 target name，这个 target 既可以是一个已经存在的 library（别跟我说 INTERFACE library），也可以是一个现存 executable target，甚至也可以不存在（如果不存在的话，此函数会以此名称创建一个 library）

有关这个 Target 的作用与他和我们 QML module 的关系，将在下文慢慢讲解

#### URI

一个很通俗易懂的参数（吧？），就是这个 QML 模块的 uri （可以为 `qml.myapp.me` 这样的域名形式，也可以是 `MyApp` 一类）

所以你可以用这个 URI 来导入此 QML module 到任何其他程序（还记得 `import QtQuick.Controls` 吧，你现在可以 `import qml.myapp.me` 了）

#### VERSION

大概用不着多说，此 QML module 的版本号，遵循 `MAJOR.MINOR` 格式（但其实你可以偷偷写 `MAJOR.MINOR.PATCH`，只不过是没用罢了）

---

### 可选参数

再来说说不带 `NO_` 开头的那些可选参数：

#### PAST_MAJOR_VERSIONS

在 Qt5 时代，一个 QML module 里是可以包含很多小版本（大版本）的，这也就是 `import Something 1.15` 中最后版本号的由来，而这在 Qt6 中变了，一个不带版本号的 `import` 语句默认导入的是最新版本的 module。

> 如果我的 module 里有多个版本呢？

那就通过 `PAST_MAJOR_VERSIONS 1 2 3 4 5` 来告诉函数你的 module 包含五个之前的版本，另外，你需要给对应版本的 `qml` 文件添加 `properties` （`QT_QML_SOURCE_VERSIONS`）来标记此文件输入哪个版本的 module

_我觉得用到这个参数的人不多吧_

#### [STATIC | SHARED]

这个选项指定了此 QML module 将会是一个静态库还是一个动态库，与 CMake 中 `add_library` 时使用的 `STATIC` 或 `SHARED` 是同一效果

> 要注意的是，这一参数只适用于给定 Target 不存在（即当此函数负责创建 target）的时候，如果必需参数 Target 已经存在，则不能指定这个参数

#### PLUGIN_TARGET

_建议放松心情慢慢看_

在 Qt QML 系统中，一个 _"模块"_ 是以 `QQml*Plugin` （`QQmlExtensionPlugin`，`QQmlEngineExtensionPlugin`） 插件形式存在的

此参数中要填写的就是这个 Plugin 的 CMake target 名称，这时会出现以下几种情况：

1. 必需参数 `Target` 为非 executable 或不存在，且 `PLUGIN_TARGET` 与 `Target` 相同
2. 必需参数 `Target` 为非 executable 或不存在，且 `PLUGIN_TARGET` 与 `Target` 不相同或未指定
3. 必需参数 `Target` 为 executable

##### 情况 1: 必需参数 `Target` 为非 executable 或不存在，且 `PLUGIN_TARGET` 与 `Target` 相同

在这种情况下，如果 `Target` 不存在，则会创建一个 library target，其动态或静态跟随上一个参数

##### 情况 2: 必需参数 `Target` 为非 executable 或不存在，且 `PLUGIN_TARGET` 与 `Target` 不相同或未指定

在这种情况下，本函数将会创建一个名称为 `${PLUGIN_TARGET}` 的 library，如果 `PLUGIN_TARGET` 未指定，则会重置为默认值 `${Target}plugin`，动态或静态跟随上一个参数，并生成一个 class 作为此 library 的实现，这部分代码实现了一个 `QQmlEngineExtensionPlugin` （文件名为 `${Target}plugin_${URI}Plugin.cpp`），此类将自动在运行时进行 QML 类型注册（具体注册流程可能要等下一篇文章了）。
