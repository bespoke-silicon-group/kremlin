#ifndef _DEBUG_H
#define _DEBUG_H

#include <signal.h>
#include "kremlin.h"
//#define KREMLIN_DEBUG	1
#define KREMLIN_DEBUG_LEVEL 0

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

void dbg_int(int sig);
void DebugDeinit();

#ifdef KREMLIN_DEBUG 
    void MSG(int level, char* format, ...);
	void updateTabString();
	void incIndentTab();
	void decIndentTab();
	void iDebugHandler();
	#define idbgAction(op, ...) 	((void)0)
#if 0
	#define idbgAction(op, ...) { \
		if (__kremlin_idbg) { \
			if (__kremlin_idbg_run_state == Waiting) { \
				fprintf(stdout, __VA_ARGS__); \
			} \
			iDebugHandler(op); \
		} \
	}
#endif
#else
    #define MSG(level, a, args...)  ((void)0)
    #define incIndentTab()          ((void)0)
    #define decIndentTab()          ((void)0)
    #define updateTabString()       ((void)0)
	#define idbgAction(op, ...) 	((void)0)
#endif


#endif
