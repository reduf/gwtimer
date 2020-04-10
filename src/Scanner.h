#pragma once

#include <Windows.h>

class Scanner
{
public:
    Scanner(LPCSTR name = nullptr)
    {
        HMODULE hModule = GetModuleHandle(name);

        MODULEINFO ModuleInfo;
        GetModuleInformation(
            GetCurrentProcess(),
            hModule,
            &ModuleInfo,
            sizeof(ModuleInfo));

        base = ModuleInfo.lpBaseOfDll;
        size = ModuleInfo.SizeOfImage;
    }

    Scanner(LPCSTR name, ULONG size)
    {
        HMODULE module = GetModuleHandle(name);
        base = (LPVOID)module;

        this->size = size;
    }

    LPVOID FindPattern(LPCSTR pattern, LPCSTR mask, ULONG offset)
    {
        ULONG len = strlen(mask);
        LPCSTR data = (LPCSTR)base;

        ULONG j = 0;
        for (ULONG i = 0; i < size; i++, data++)
        {
            for (j = 0; j < len; j++)
            {
                if (mask[j] == 'x' && pattern[j] != data[j])
                {
                    break;
                }
            }
            if (j == len)
            {
                return (LPVOID)((ULONG)data + offset);
            }
        }

        return NULL;
    }

private:
    LPVOID base;
    ULONG size;
};
