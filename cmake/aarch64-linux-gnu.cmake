# Cross-compile for 64-bit Raspberry Pi OS (Pi 3/4/5) from an x86-64 host:
#   cmake -S . -B build-rpi -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
# Needs g++-aarch64-linux-gnu (Debian/Ubuntu). Tests can run under qemu-user.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

find_program(QEMU_AARCH64 qemu-aarch64)
if(QEMU_AARCH64)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${QEMU_AARCH64} -L /usr/aarch64-linux-gnu)
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
