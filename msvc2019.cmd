@echo off
mkdir build_msvc_2019 > nul 2> nul
cmake -G "Visual Studio 16 2019" -A x64 -Bbuild_msvc_2019 .
