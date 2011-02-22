#include "defs.h"
#include "debug.h"
#include "table.h"

#if KREMLIN_DEBUG == 1
static char tabString[2000];
static int tabLevel = 0;

void MSG(int level, char* format, ...) {
    if (level > DEBUGLEVEL) {
        return;
    }

    fprintf(stderr, "%s", tabString);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

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
#endif

