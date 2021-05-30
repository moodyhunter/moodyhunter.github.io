---
title: "从源码编译 Qt6 for WASM - Part 2"
date: '2021-05-29'
image: qt.png
---

~~时间过得好快，自从上次发 post 已经过了一周~~

- 趁着深夜停电正好能总结一下这周掉进的坑（我保证没有咕咕咕）

# 1. "Qt Host Build"?

你看：

```
-- Searching for tool 'Qt6::moc' in package Qt6CoreTools.
CMake Warning at cmake/QtToolHelpers.cmake:83 (find_package):
  Could not find a configuration file for package "Qt6CoreTools" that is
  compatible with requested version "6.2.0".

  The following configuration files were considered but not accepted:

    /lib/cmake/Qt6CoreTools/Qt6CoreToolsConfig.cmake, version: 6.1.0

Call Stack (most recent call first):
  src/tools/moc/CMakeLists.txt:8 (qt_internal_add_tool)


CMake Error at cmake/QtToolHelpers.cmake:109 (message):
  The tool "Qt6::moc" was not found in the Qt6CoreTools package.  Package
  found: 0
Call Stack (most recent call first):
  src/tools/moc/CMakeLists.txt:8 (qt_internal_add_tool)


-- Configuring incomplete, errors occurred!
```

看到这么一大串错误信息我一下子就慌了，CMake 告诉我 `Could not find "Qt6CoreTools" .... with requested version "6.2.0", The following configuration files were considered but not accepted .... version: 6.1.0`

好家伙那看来我还需要一个 Qt6 的 Host Build

- 所谓 Host Build 即为本地编译平台环境下的 Qt 编译，或者可以称为 "原生平台"

这个可怕的事实告诉我： **我还需要编译一个 6.2 的 Linux 版 Qt**

# 2. ~~To Build Qt, Build Qt First~~

既然 Qt 6.1 无法满足编译的要求，那么就得整一个 6.2 版本 Native Platform 的 Qt 当 Host

首先使用 **传 统 艺 能**： `mkdir build; cd build; cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=host_installed`

> ......
> 💥 BOOM 💥
> ......

QtGraphicalEffects 和 Qt5Compat 里面的 GraphicalEffects 撞了 Target
- 手动删掉了 qt5/qt5compat 文件夹来避免被 build
- 或许正赶上 Qt 在进行 repo 转移，后续尝试的时候就没有出现这个错误了

> **第一次尝试的时候忘记加 ccache，编译一次大概半个小时，用 ccache 能节约至少三分之一**
> 
> Qt 的编译参数很人性化，直接一个 `-DQT_USE_CCACHE=ON` 就完事了
> - 因为在 [这个地方](https://github.com/qt/qtbase/blob/9db7cc79a26ced4997277b5c206ca15949133240/cmake/QtSetup.cmake#L217) 会自动查找 ccache
> 
> 根据 QtCreator 的 CI 配置来看，ccache 要加一些 sloppiness 设置（比如 `time_macros`, `pch_defines` 和 `file_macro`）于是照搬了过来


# 3. 重新编译 WASM 版本

经过了千辛万苦，终于成功编译出了 Native Platform Qt，下面就要进行紧张刺激的 Cross-Compile 环节

## 3.1 错误的编译参数

使用上一篇文章中的 CMake 参数，并修改对应 `QT_HOST_PATH`：

```bash
cmake ~/Work/qt5/qtbase/ \
  -GNinja \
  -DFEATURE_developer_build=ON \
  -DFEATURE_headersclean=OFF \
  -DFEATURE_precompile_header=OFF \
  -DWARNINGS_ARE_ERRORS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF \
  -DCMAKE_INSTALL_PREFIX=~/Work/qt-build/wasm-installed \
  -DCMAKE_TOOLCHAIN_FILE=~/.local/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
  -DQT_HOST_PATH=~/Work/qt-build/host-installed
```

看这节标题就能猜到了，CMake 抱怨 `ERROR: Static builds don't support RPATH`

> 喵？ 都 Target WASM 了还不知道自己关掉 RPATH？

于是搜索相关参数： `-DFEATURE_rpath=OFF`，并清除 CMake 缓存重新编译

----------

漫长的一个小时过去了，QtBase 模块终于编译完，心情激动直接当场 `cmake --install .` 并开始编译其他模块：
- QtShaderTools
- QtSvg
- QtImageFormats
- QtDeclarative
- QtCharts
- QtRemoteObjects
- QtWebSocket
- QtQuick3D、
- QtQuickTimeLine
- QtDataVisualization
- ...

Qt Company 很人性化的提供了 `$QT_INSTALL_DIR/bin/qt-configure-modules` 于是直接带着源代码文件夹当作参数用就完事了

```
$QT_INSTALL_DIR/bin/qt-configure-modules ~/Work/qt5/-MODULE_NAME-
```

就这样编译好了所有需要的 Modules 却在使用上遇到了一些问题

⬅️ To Be Continued
