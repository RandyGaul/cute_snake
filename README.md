<p align="center">
<img src=https://github.com/RandyGaul/cute_snake/blob/master/title.gif>
</p>

# Cute Snake

A silly implementation of the game Snake as an example of how to make a small 2D game using [Cute Framework](https://github.com/RandyGaul/cute_framework) (CF).

## Building

* Install [cmake](https://cmake.org/).
* Run the usual cmake commands to trigger a build (cmake -G ..., and cmake --build, etc.).
* A few scripts are provided for those unfamiliar with cmake, for example on Windows you can build with MSVC 2019 by running `msvc2019.cmd`, or with MingW by running `mingw.cmd`.

```cmd
# Windows batch file to build Cute Snake via cmake with a MingW compiler.
mkdir build_mingw > nul 2> nul
cmake -G "Unix Makefiles" -Bbuild_mingw .
cmake --build build_mingw
```

```bash
#!/bin/bash

# Bash script to build Cute Snake via cmake.
mkdir -p build_bash
cmake -Bbuild_bash .
cmake --build build_bash
```

This example uses cmake's FetchContent feature to download and build CF the first time Snake is built.
