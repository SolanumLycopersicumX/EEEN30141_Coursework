# Toolchain file for MSYS2 UCRT64 clang++.
# Usage:
#   cmake -S . -B build-ucrt64-clang -G "Ninja"
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ucrt64-clang.cmake

set(CMAKE_SYSTEM_NAME Windows)

set(TOOLCHAIN_ROOT "C:/msys64/ucrt64" CACHE PATH "MSYS2 UCRT64 root directory")

set(CMAKE_C_COMPILER "${TOOLCHAIN_ROOT}/bin/clang.exe" CACHE FILEPATH "MSYS2 UCRT64 clang compiler" FORCE)
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/bin/clang++.exe" CACHE FILEPATH "MSYS2 UCRT64 clang++ compiler" FORCE)
set(CMAKE_RC_COMPILER "${TOOLCHAIN_ROOT}/bin/windres.exe" CACHE FILEPATH "MSYS2 UCRT64 resource compiler" FORCE)

# Prefer Ninja with clang; fall back to mingw32-make if Ninja is unavailable.
find_program(NINJA_EXECUTABLE ninja.exe HINTS "${TOOLCHAIN_ROOT}/bin")
if (NINJA_EXECUTABLE)
    set(CMAKE_MAKE_PROGRAM "${NINJA_EXECUTABLE}" CACHE FILEPATH "Ninja build tool" FORCE)
else ()
    set(CMAKE_MAKE_PROGRAM "${TOOLCHAIN_ROOT}/bin/mingw32-make.exe" CACHE FILEPATH "MSYS2 make program" FORCE)
endif ()

list(PREPEND CMAKE_PROGRAM_PATH "${TOOLCHAIN_ROOT}/bin")
list(PREPEND CMAKE_PREFIX_PATH "${TOOLCHAIN_ROOT}")

