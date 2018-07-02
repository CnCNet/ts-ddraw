#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef LONGLONG QPCounter;
void CounterStart(QPCounter *counter);
double CounterGet(QPCounter *counter);
