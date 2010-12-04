#ifndef _DEBUG_H
#define _DEBUG_H

#if PYRPROF_DEBUG == 1
    void MSG(int level, char* format, ...);
	void updateTabString();
	void incIndentTab();
	void decIndentTab();
	char* toStringTEntry(TEntry* entry);
#else
    #define MSG(level, a, args...)  ((void)0)
    #define incIndentTab()          ((void)0)
    #define decIndentTab()          ((void)0)
    #define updateTabString()       ((void)0)
    #define toStringTEntry(entry)       ((void)0)

#endif

#endif
