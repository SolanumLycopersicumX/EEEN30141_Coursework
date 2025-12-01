# Toolchain file for building with the MSYS2 UCRT64 GCC toolchain.
# Usage:
#   cmake -S . -B build-ucrt64 -G "MinGW Makefiles" ^
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ucrt64-gcc.cmake

set(CMAKE_SYSTEM_NAME Windows)

set(TOOLCHAIN_ROOT "C:/msys64/ucrt64" CACHE PATH "MSYS2 UCRT64 root directory")

set(CMAKE_C_COMPILER "${TOOLCHAIN_ROOT}/bin/gcc.exe" CACHE FILEPATH "MSYS2 UCRT64 C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/bin/g++.exe" CACHE FILEPATH "MSYS2 UCRT64 C++ compiler" FORCE)
set(CMAKE_RC_COMPILER "${TOOLCHAIN_ROOT}/bin/windres.exe" CACHE FILEPATH "MSYS2 UCRT64 resource compiler" FORCE)
set(CMAKE_MAKE_PROGRAM "${TOOLCHAIN_ROOT}/bin/mingw32-make.exe" CACHE FILEPATH "MSYS2 make program" FORCE)

# Ensure CMake searches the MSYS2 UCRT64 prefixes first
list(PREPEND CMAKE_PROGRAM_PATH "${TOOLCHAIN_ROOT}/bin")
list(PREPEND CMAKE_PREFIX_PATH "${TOOLCHAIN_ROOT}")

