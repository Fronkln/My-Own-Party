#pragma once
#pragma once
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <algorithm>
#include <vector>

using std::vector;


std::uint8_t* PatternScan(void* module, const char* signature)
{
    static auto pattern_to_byte = [](const char* pattern) {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    };

    auto dosHeader = (PIMAGE_DOS_HEADER)module;
    auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

    auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto patternBytes = pattern_to_byte(signature);
    auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

    auto s = patternBytes.size();
    auto d = patternBytes.data();

    for (auto i = 0ul; i < sizeOfImage - s; ++i) {
        bool found = true;
        for (auto j = 0ul; j < s; ++j) {
            if (scanBytes[i + j] != d[j] && d[j] != -1) {
                found = false;
                break;
            }
        }
        if (found) {
            return &scanBytes[i];
        }
    }
    return nullptr;
}

//99% of revelant functions are in the first 0x10000000 bytes of a game's entry point
static intptr_t FindPattern(intptr_t baseAddress, intptr_t length, const char* lpPattern, const char* pszMask, intptr_t offset = 0, intptr_t resultUsage = 0)
{
    // 0 base address = moduleaddress

    if (baseAddress == 0)
        baseAddress = (uintptr_t)GetModuleHandle(NULL);

    std::vector<unsigned char> data((unsigned char*)baseAddress, baseAddress + (unsigned char*)length);

    // Build vectored pattern.. 
    std::vector<std::pair<unsigned char, bool>> pattern;
    for (size_t x = 0, y = strlen(pszMask); x < y; x++)
        pattern.push_back(std::make_pair(lpPattern[x], pszMask[x] == 'x'));

    auto scanStart = data.begin();
    auto resultCnt = 0;

    while (true)
    {
        // Search for the pattern.. 
        auto ret = std::search(scanStart, data.end(), pattern.begin(), pattern.end(),
            [&](char curr, std::pair<char, bool> currPattern)
            {
                return (!currPattern.second) || curr == currPattern.first;
            });

        // Did we find a match.. 
        if (ret != data.end())
        {
            // If we hit the usage count, return the result.. 
            if (resultCnt == resultUsage || resultUsage == 0)
                return (std::distance(data.begin(), ret) + baseAddress) + offset;

            // Increment the found count and scan again.. 
            ++resultCnt;
            scanStart = ++ret;
        }
        else
            break;
    }

    return 0;
}