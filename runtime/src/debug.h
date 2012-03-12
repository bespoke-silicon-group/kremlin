#ifndef _DEBUG_H
#define _DEBUG_H

#include <signal.h>
#include "kremlin.h"
//#define KREMLIN_DEBUG	1

/* WARNING!!!!
 *
 * debug functions must be replaced with an empty macro
 * when compiled for non-debug mode.
 * otherwise, Kremlin's performance gets 10X slower!
 */

typedef enum iDbgRunState {RunUntilBreak, RunUntilFinish, Waiting} iDbgRunState;

extern int __kremlin_idbg;
extern iDbgRunState __kremlin_idbg_run_state;


void printActiveRegionStack();
void printControlDepTimes();
void printRegisterTimes(Reg reg);
void printMemoryTimes(Addr addr, Index size);

#ifdef KREMLIN_DEBUG 
    void MSG(int level, char* format, ...);
	void updateTabString();
	void incIndentTab();
	void decIndentTab();

	void dbg_int(int sig);
	void iDebugHandler();

#else
    #define MSG(level, a, args...)  ((void)0)
    #define incIndentTab()          ((void)0)
    #define decIndentTab()          ((void)0)
    #define updateTabString()       ((void)0)
	void dbg_int(int sig) {
		(void)signal(sig,SIG_DFL);
	}
#endif


#endif
