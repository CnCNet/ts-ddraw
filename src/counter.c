#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>


static LONGLONG CounterStartTime = 0;
static double CounterFreq = 0.0;

void CounterStart()
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    CounterFreq = (double)(li.QuadPart) / 1000.0;
    QueryPerformanceCounter(&li);
    CounterStartTime = li.QuadPart;
}

double CounterGet()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (double)(li.QuadPart - CounterStartTime) / CounterFreq;
}
