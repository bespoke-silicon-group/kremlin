#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ktypes.h"
#include "debug.h"
#include "config.h"
//#include "idbg.h"

#include <vector>

std::vector<SID> region_ids;

int	__kremlin_debug = 1;
int  __kremlin_debug_level = KREMLIN_DEBUG_LEVEL;

int __kremlin_idbg = 0;

iDbgRunState __kremlin_idbg_run_state = Waiting;

//static SID regionBreaks;

static char tabString[2000];
static int tabLevel = 0;
static FILE* stream;

void DebugInit() {
	const char* df = kremlin_config.getDebugOutputFilename();

	// TRICKY: With C++, sometimes we have to call DebugInit before we
	// have set up the configuration (including the debug output
	// filename). Here we check if the filename exists: if not, we just
	// write to /dev/null.
	// TODO: Come up with a better way to handle this corner case.
	stream = fopen(df ? df : "/dev/null", "w");
	if (stream == NULL) {
		fprintf(stderr,"ERROR: could not open file for writing: %s\n",kremlin_config.getDebugOutputFilename());
		exit(1);
	}
}

void DebugDeinit() {
	fclose(stream);
	region_ids.clear();
}

void dbg_int(int sig) {
#ifdef KREMLIN_DEBUG
	fprintf(stdout,"\nWelcome to the Kremlin interactive debugger!\n");
	__kremlin_idbg = 1;
	__kremlin_idbg_run_state = Waiting;
#endif
	(void)signal(sig, SIG_DFL);
}

char** tokenizeString(char* str) {
	char** tokens = new char*[3]; //malloc(3*sizeof(char*));

	tokens[0] = strtok(str," \n"); // FIXME: this isn't robust at all
	tokens[1] = strtok(NULL," \n");
	tokens[2] = strtok(NULL," \n");
	/*
	fprintf(stdout,"tokens[0] = %s\n",tokens[0]);
	if(tokens[1] != NULL) {
		fprintf(stdout,"tokens[1] = %s\n",tokens[1]);
	}
	*/

	return tokens;
}

#ifdef KREMLIN_DEBUG
void iDebugHandlerRegionEntry(SID regionId) {
	if (!__kremlin_idbg)
		return;

	if(__kremlin_idbg_run_state == RunUntilFinish) return;

	for(unsigned i = 0; i < region_ids.size(); ++i) {
		SID id = region_ids[i];
		if(id == regionId) {
			fprintf(stdout,"region breakpoint #%d: SID=%llu\n",i,regionId);
			__kremlin_idbg_run_state = Waiting;
			break;
		}
		/*
		else {
			fprintf(stdout,"no match with: %llu\n",id);
		}
		*/
	}
}

void iDebugHandler(UInt kremFunc) {
	if(__kremlin_idbg_run_state == RunUntilFinish) {
		if(kremFunc != KREM_REGION_EXIT) return;
		else {
			__kremlin_idbg_run_state = Waiting;
			fprintf(stdout,"\tAt end of current region.\n");
		}
	}
	else if(__kremlin_idbg_run_state == RunUntilBreak) { return; }
	
	char* dbg_arg = new char[25]; //malloc(25*sizeof(char));

	while(1) {
		fprintf(stdout,"kremlin-dbg> ");
		fgets(dbg_arg,25,stdin);

		char** tokens = tokenizeString(dbg_arg);

		if(tokens[0] == NULL) {} // will be NULL for empty string
		else if(strcmp(tokens[0],"next") == 0
		   || strcmp(tokens[0],"n") == 0
			) {
			return;
		}
		else if(strcmp(tokens[0],"finish") == 0
			    || strcmp(tokens[0],"f") == 0
			   ) {
			fprintf(stdout,"Continuing until end of current region...\n");
			__kremlin_idbg_run_state = RunUntilFinish;
			return;
		}
		else if(strcmp(tokens[0],"regionstack") == 0
			    || strcmp(tokens[0],"rs") == 0
			   ) {
			printActiveRegionStack();
		}
		else if(strcmp(tokens[0],"controldeps") == 0
			    || strcmp(tokens[0],"cds") == 0
			   ) {
			printControlDepTimes();
		}
		else if(strcmp(tokens[0],"register") == 0
			    || strcmp(tokens[0],"reg") == 0
			   ) {
			if(tokens[1] == NULL) {
				fprintf(stderr,"ERROR: incorrect format for register command\n");
			}
			else {
				Reg regNum = atoi(tokens[1]);
				printRegisterTimes(regNum);
			}
		}
		else if(strcmp(tokens[0],"memory") == 0
			    || strcmp(tokens[0],"mem") == 0
			   ) {
			if(tokens[1] == NULL || tokens[2] == NULL) {
				fprintf(stderr,"ERROR: incorrect format for memory command\n");
			}
			else {
				Addr addr;
				sscanf(tokens[1],"%p\n",&addr);
				Index size;
				sscanf(tokens[2],"%X\n",&size);
				printMemoryTimes(addr,size);
			}
		}
		else if(strcmp(tokens[0],"break") == 0
			    || strcmp(tokens[0],"b") == 0
			   ) {
			if(tokens[1] == NULL) {
				fprintf(stderr,"ERROR: incorrect format for break command\n");
			}
			else {
				//SID regionId = atoll(tokens[1]);
				SID regionId;
				sscanf(tokens[1],"%llX\n",&regionId);
				// FIXME: check for dup before pushing value
				region_ids.push_back(regionId);
			}
		}
		else if(strcmp(tokens[0],"info") == 0
			    || strcmp(tokens[0],"i") == 0
			   ) {
			if(tokens[1] == NULL) {
				fprintf(stderr,"ERROR: incorrect format for break command\n");
			}
			else if(strcmp(tokens[1],"break") == 0) {
				fprintf(stdout,"Region breakpoints:\n");

				for(unsigned i = 0; i < region_ids.size(); ++i) {
					SID rid = region_ids[i];
					fprintf(stdout,"\t#%d: %llu (0x%llx)\n",i,rid,rid);
				}
			}
			else {
				fprintf(stderr,"ERROR: unknown subcommand for info: %s\n", tokens[1]);
			}
		}
		else if(strcmp(tokens[0],"continue") == 0
			    || strcmp(tokens[0],"c") == 0
			   ) {
			fprintf(stdout,"Continuing execution...\n");
			__kremlin_idbg_run_state = RunUntilBreak;
			return;
			
		}
		else if(strcmp(tokens[0],"state") == 0
			    || strcmp(tokens[0],"st") == 0
			   ) {
			fprintf(stdout,"Debugging state: ");
			switch(__kremlin_idbg_run_state) {
				case RunUntilBreak:
					fprintf(stdout,"RunUntilBreak\n");
					break;
				case RunUntilFinish:
					fprintf(stdout,"RunUntilFinish\n");
					break;
				case Waiting:
					fprintf(stdout,"Waiting\n");
					break;
				default:
					fprintf(stderr,"\nERROR: erroneous state!\n");
			}
		}
		else if(strcmp(tokens[0],"help") == 0
			    || strcmp(tokens[0],"h") == 0
			   ) {
			// TODO: implement
			fprintf(stderr,"WARNING: help command not implemented!\n");
		}
		else if(strcmp(tokens[0],"stop") == 0) {
			fprintf(stdout,"Exiting Kremlin interactive debugging mode!\n");
			// TODO: implement
			fprintf(stderr,"WARNING: stop command not implemented!\n");
		}
		else if(strcmp(tokens[0],"quit") == 0) {
			fprintf(stdout,"quitting!\n");
			exit(1);
		}
		else {
			fprintf(stdout,"unrecognized command!\n");
		}
	}
}
#endif


#ifdef KREMLIN_DEBUG
void MSG(int level, const char* format, ...) {
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
#endif
