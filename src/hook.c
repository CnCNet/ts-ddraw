#include <windows.h>
#include <stdio.h>
#include "IDirectDraw.h"

#define MAX_HOOKS 16

struct hook { char name[32]; void *func; };
struct hack
{
    char name[32];
    struct hook hooks[MAX_HOOKS];
};

struct hack hacks[] =
{
    {
        "user32.dll",
        {
            { "SetWindowPos", fake_SetWindowPos },
            { "MoveWindow", fake_MoveWindow },
            { "", NULL }
        }
    },
    {
        "",
        {
            { "", NULL }
        }
    }
};

void hack_iat(struct hack *hck)
{
    int i;
    char buf[32];
    struct hook *hk;
    DWORD dwWritten;
    IMAGE_DOS_HEADER dos_hdr;
    IMAGE_NT_HEADERS nt_hdr;
    IMAGE_IMPORT_DESCRIPTOR *dir;
    IMAGE_THUNK_DATA thunk;
    PDWORD ptmp;

    HMODULE base = GetModuleHandle(NULL);
    HANDLE hProcess = GetCurrentProcess();

    ReadProcessMemory(hProcess, (void *)base, &dos_hdr, sizeof(IMAGE_DOS_HEADER), &dwWritten);
    ReadProcessMemory(hProcess, (void *)((char *)base+dos_hdr.e_lfanew), &nt_hdr, sizeof(IMAGE_NT_HEADERS), &dwWritten);
    dir = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (nt_hdr.OptionalHeader.DataDirectory[1].Size));
    ReadProcessMemory(hProcess, (void *)((char *)base+nt_hdr.OptionalHeader.DataDirectory[1].VirtualAddress), dir, nt_hdr.OptionalHeader.DataDirectory[1].Size, &dwWritten);

    while(dir->Name > 0)
    {
        memset(buf, 0, 32);
        ReadProcessMemory(hProcess, (void *)((char *)base+dir->Name), buf, 32, &dwWritten);
        if(stricmp(buf, hck->name) == 0)
        {
            ptmp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DWORD) * 64);
            ReadProcessMemory(hProcess, (void *)((char *)base+dir->Characteristics), ptmp, sizeof(DWORD) * 64, &dwWritten);
            i=0;
            while(*ptmp)
            {
                memset(buf, 0, 32);
                ReadProcessMemory(hProcess, (void *)((char *)base+(*ptmp)+2), buf, 32, &dwWritten);

                hk = &hck->hooks[0];
                while(hk->func)
                {
                    if(stricmp(hk->name, buf) == 0)
                    {
                        thunk.u1.Function = (DWORD)hk->func;
                        thunk.u1.Ordinal = (DWORD)hk->func;
                        thunk.u1.AddressOfData = (DWORD)hk->func;
                        VirtualProtectEx(hProcess, (void *)((char *)base+dir->FirstThunk+(sizeof(IMAGE_THUNK_DATA) * i)), sizeof(IMAGE_THUNK_DATA), PAGE_EXECUTE_READWRITE, &dwWritten);
                        WriteProcessMemory(hProcess, (void *)((char *)base+dir->FirstThunk+(sizeof(IMAGE_THUNK_DATA) * i)), &thunk, sizeof(IMAGE_THUNK_DATA), &dwWritten);
                    }
                    hk++;
                }

                ptmp++;
                i++;
            }
        }
        dir++;
    }

    CloseHandle(hProcess);
}

void hook_init()
{
    static BOOL hook_active = FALSE;
    if (!hook_active)
    {
        hook_active = TRUE;
        hack_iat(&hacks[0]);
    }
}
