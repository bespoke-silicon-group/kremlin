#ifndef _DEBUG_H
#define _DEBUG_H

#include "defs.h"
//#define KREMLIN_DEBUG	1

/* WARNING!!!!
 *
 * debug functions must be replaced with an empty macro
 * when compiled for non-debug mode.
 * otherwise, Kremlin's performance gets 10X slower!
 */

#ifdef KREMLIN_DEBUG 
    void MSG(int level, char* format, ...);
	void updateTabString();
	void incIndentTab();
	void decIndentTab();

#else
    #define MSG(level, a, args...)  ((void)0)
    #define incIndentTab()          ((void)0)
    #define decIndentTab()          ((void)0)
    #define updateTabString()       ((void)0)
#endif


#endif
