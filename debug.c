#include "defs.h"

#if PYRPROF_DEBUG == 1
    void MSG(int level, char* format, ...);
#else
    #define MSG(level, a, args...)  ((void)0)
    #define incIndentTab()          ((void)0)
    #define decIndentTab()          ((void)0)
    #define updateTabString()       ((void)0)

#endif

#if PYRPROF_DEBUG == 1
char            tabString[MAX_REGION_LEVEL*2+1];
int tabLevel = 0;

void MSG(int level, char* format, ...) {
    if (level > DEBUGLEVEL) {
        return;
    }

    int strSize = strlen(format) + strlen(tabString);
    char* buf = malloc(strSize + 5);
    strcpy(buf, tabString);
    strcat(buf, format);
    //printf("%s\n", buf);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, buf, args);
    va_end(args);
    free(buf);
}

inline void updateTabString() {
    int i;
    for (i = 0; i < tabLevel*2; i++) {
        tabString[i] = ' ';
    }
    tabString[i] = 0;
}

inline void incIndentTab() {
    tabLevel++;
    updateTabString();
}

inline void decIndentTab() {
    tabLevel--;
    updateTabString();
}
#endif

