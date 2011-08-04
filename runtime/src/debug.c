#include <stdio.h>
#include "defs.h"
#include "debug.h"

int	__kremlin_debug = 1;
int  __kremlin_debug_level = 1;

static char tabString[2000];
static int tabLevel = 0;
static FILE* stream;

void DebugInit(char* str) {
	stream = fopen(str, "w");
}

void DebugDeinit() {
	fclose(stream);
}


void MSG(int level, char* format, ...) {
    if (!__kremlin_debug || level > __kremlin_debug_level) {
        return;
    }

    fprintf(stream, "%s", tabString);
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
	fflush(stream);

}

void updateTabString() {
    int i;
    for (i = 0; i < tabLevel*2; i++) {
        tabString[i] = ' ';
    }
    tabString[i] = 0;
}

void incIndentTab() {
    tabLevel++;
    updateTabString();
}

void decIndentTab() {
    tabLevel--;
    updateTabString();
}
