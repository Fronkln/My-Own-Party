// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"
#include <iostream>

//Helper string function
inline bool ends_with(std::wstring const& value, std::wstring const& ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

inline void* resolve_relative_addr(void* instruction_start, int instruction_length = 7)
{
    void* instruction_end = (void*)((unsigned long long)instruction_start + instruction_length);
    unsigned int* offset = (unsigned int*)((unsigned long long)instruction_start + (instruction_length - 4));

    void* addr = (void*)(((unsigned long long)instruction_start + instruction_length) + *offset);

    return addr;
}

#endif //PCH_H
