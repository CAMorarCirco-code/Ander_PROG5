# Cross-compile for 32-bit Raspberry Pi OS (armhf, Pi 2 and later) from an x86-64 host:
#   cmake -S . -B build-rpi32 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake
# Needs g++-arm-linux-gnueabihf (Debian/Ubuntu). Tests can run under qemu-user.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

find_program(QEMU_ARM qemu-arm)
if(QEMU_ARM)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${QEMU_ARM} -L /usr/arm-linux-gnueabihf)
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
