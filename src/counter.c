#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "counter.h"


static double CounterFreq = 0.0;

void CounterStart(QPCounter *counterStartTime)
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    CounterFreq = (double)(li.QuadPart) / 1000.0;
    QueryPerformanceCounter(&li);
    *counterStartTime = li.QuadPart;
}

double CounterGet(QPCounter *counterStartTime)
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (double)(li.QuadPart - *counterStartTime) / CounterFreq;
}
