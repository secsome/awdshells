# awdshells
... is a reverse shell manager for awd. This project is a C++ rewritten for [ShellSession](https://github.com/KPGhat/ShellSession)

## Requirements
- C++23 compatitable compiler
- `CMake` >= 3.20

## Build
1. `mkdir build`
2. `cd build`
3. `cmake -G "Unix Makefiles" ..`
4. `cmake --build . -- -j $(nproc)`
