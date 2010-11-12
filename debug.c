#include "defs.h"
#include "debug.h"

#if PYRPROF_DEBUG == 1
static char            tabString[2000];
static int tabLevel = 0;

extern int regionNum;
char* toStringTEntry(TEntry* entry) {
	int level = regionNum;
	int i;
	char temp[50];

	// XXX: Causes memory leak?
	char* ret = (char*)malloc(300);
	ret[0] = 0;

	for (i = 0; i < level; i++) {
		sprintf(temp, " %llu (%u)", entry->time[i], entry->version[i]);
		strcat(ret, temp);
	}
	return ret;
}

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

