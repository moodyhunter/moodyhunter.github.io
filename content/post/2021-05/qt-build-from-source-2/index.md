---
title: "从源码编译 Qt6 for WASM - Part 2"
date: '2021-05-25'
image: qt.png
draft: true
---

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

```
CMake Warning:
  Manually-specified variables were not used by the project:

    BUILD_EXAMPLES

```

```
cmake \
                    -GNinja \
                    -DCMAKE_INSTALL_PREFIX=~Work/qt-build/host-installed \
                    ~/Work/qt5/qtbase/ \
                    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
                    -DCMAKE_C_COMPILER_LAUNCHER=ccache
```

`  Using ccache ........................... no`

qt-build/host > cmake \
                    -GNinja \
                    -DCMAKE_INSTALL_PREFIX=~Work/qt-build/host-installed \
                    ~/Work/qt5/qtbase/ \
                    -DQT_USE_CCACHE=ON

can't use precompiled header        1903
CCACHE_SLOPPINESS="time_macros"
https://ccache.dev/manual/latest.html#config_sloppiness 

6m10s

CMake Error at cmake/QtExecutableHelpers.cmake:37 (qt6_wasm_add_target_helpers):
  Unknown CMake command "qt6_wasm_add_target_helpers".
Call Stack (most recent call first):
  cmake/QtTestHelpers.cmake:214 (qt_internal_add_executable)
  tests/auto/testlib/qsignalspy/CMakeLists.txt:7 (qt_internal_add_test)
>                    -DFEATURE_developer_build=ON \

ERROR: Static builds don't support RPATH
-DFEATURE_rpath=OFF


```
 cmake \
                    -GNinja \
                    -DFEATURE_headersclean=OFF \
                    -DFEATURE_precompile_header=OFF \
                    -DWARNINGS_ARE_ERRORS=OFF \
                    -DBUILD_EXAMPLES=OFF \
                    -DBUILD_TESTING=OFF \
                    -DQT_USE_CCACHE=ON \
                    -DFEATURE_rpath=OFF \
                    ~/Work/qt5/qtbase/ \
                    -DCMAKE_INSTALL_PREFIX=~/Work/qt-build/wasm-installed \
                    -DCMAKE_TOOLCHAIN_FILE=~/.local/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
                    -DQT_HOST_PATH=~/Work/qt-build/host-installed/
                    ```

CMake Error: Error processing file: /home/leroy/Work/qt-build/wasm/bin/../lib/cmake/Qt6/QtProcessConfigureArgs.cmake

cmake/Qt6 > cp ~/Work/qtbase/cmake/QtProcessConfigureArgs.cmake .                                                                              1s
cmake/Qt6 > cp ~/Work/qtbase/cmake/QtBuildInformation.cmake .

-- Could NOT find Qt6QmlTools (missing: Qt6QmlTools_DIR)
CMake Error at /home/leroy/Work/qt5/qtbase/cmake/QtToolHelpers.cmake:109 (message):
  The tool "Qt6::qmltyperegistrar" was not found in the Qt6QmlTools package.
  Package found: 0
Call Stack (most recent call first):
  src/qmltyperegistrar/CMakeLists.txt:8 (qt_internal_add_tool)


-- Configuring incomplete, errors occurred!
See also "/home/leroy/Work/qt-build/declarative/CMakeFiles/CMakeOutput.log".
CMake Error at /home/leroy/Work/qt-build/wasm/lib/cmake/Qt6/QtProcessConfigureArgs.cmake:932 (message):
  CMake exited with code 1.

  (**** Host tools not built ****)


export CCACHE_SLOPPINESS="time_macros"
export CMAKE_PREFIX_PATH=/usr

-DBUILD_SHARED_LIBS=OFF
