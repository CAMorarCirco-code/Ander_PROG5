#pragma once

#include <dirent.h>

namespace test {

/// Number of open file descriptors of this process (Linux /proc).
inline int openFdCount()
{
    DIR* dir = opendir("/proc/self/fd");
    if (dir == nullptr) {
        return -1;
    }
    int count = 0;
    while (readdir(dir) != nullptr) {
        ++count;
    }
    closedir(dir);
    return count;   // includes ".", ".." and the DIR's own fd; constant offset
}

} // namespace test
