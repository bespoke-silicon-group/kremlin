#include <signal.h> // for catching CTRL-V during debug

#include "kremlin.h"
#include "arg.h"
#include "config.h"

#include "debug.h"

char * __kremlin_output_filename;
int __kremlin_debug;
int __kremlin_debug_level;
int __kremlin_level_to_log = -1;


char * argGetOutputFileName() {
	return __kremlin_output_filename;
}

UInt getKremlinDebugLevel() {
	return __kremlin_debug_level;
}

UInt getKremlinDebugFlag() {
	return __kremlin_debug;
}


int parseOptionInt(char* option_str) {
	char *dbg_level_str = strtok(option_str,"= ");
	dbg_level_str = strtok(NULL,"= ");

	if(dbg_level_str) {
		return atoi(dbg_level_str);
	}
	else {
		fprintf(stderr,"ERROR: Couldn't parse int from option (%s)\n",option_str);
	}
}

void createOutputFilename() {
	__kremlin_output_filename[0] = '\0'; // "clear" the old name

	strcat(__kremlin_output_filename, "kremlin-L");
	char level_str[5];
	//sprintf(level_str,"%d",__kremlin_level_to_log);
	sprintf(level_str, "%d", KConfigGetMinLevel());
	strcat(__kremlin_output_filename, level_str);

	if(KConfigGetMaxLevel() != (KConfigGetMinLevel())) {
		strcat(__kremlin_output_filename,"_");
		level_str[0] - '\0';
		sprintf(level_str,"%d", KConfigGetMaxLevel());
		strcat(__kremlin_output_filename,level_str);
	}
	
	strcat(__kremlin_output_filename,".bin");
}

int parseKremlinOptions(int argc, char* argv[], int* num_args, char*** real_args) {
	int num_true_args = 0;
	char** true_args = malloc(argc*sizeof(char*));

	int parsed = 0;
	int i;
	for(i = 1; i < argc; ++i) {
		fprintf(stderr,"checking %s\n",argv[i]);
		char *str_start;

		str_start = strstr(argv[i],"kremlin-debug");

		if(str_start) {
			__kremlin_debug= 1;
			__kremlin_debug_level = parseOptionInt(argv[i]);
			continue;
		}

#ifdef KREMLIN_DEBUG
		str_start = strstr(argv[i],"kremlin-idbg");

		if(str_start) {
			__kremlin_idbg = 1;
			__kremlin_idbg_run_state = Waiting;
			continue;
		}
#endif

#if 0
		str_start = strstr(argv[i],"kremlin-ltl");
		if(str_start) {
			__kremlin_level_to_log = parseOptionInt(argv[i]);
			setMinLevel(__kremlin_level_to_log);
			setMaxLevel(__kremlin_level_to_log + 1);
			continue;
		}
#endif
		str_start = strstr(argv[i],"disable-rsummary");
		if(str_start) {
			KConfigDisableRSummary();
			continue;
		}

		str_start = strstr(argv[i],"disable-cregion");
		if(str_start) {
			KConfigDisableCRegion();
			continue;
		}



		str_start = strstr(argv[i],"mshadow-type");
		if(str_start) {
			KConfigSetShadowType(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"gc-period");
		if(str_start) {
			KConfigSetGCPeriod(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"compression");
		if(str_start) {
			KConfigSetCompression(parseOptionInt(argv[i]));
			continue;
		}
		str_start = strstr(argv[i],"compress");
		if(str_start) {
			KConfigSetCompression(parseOptionInt(argv[i]));
			continue;
		}
		str_start = strstr(argv[i],"cbuffer-size");
		if(str_start) {
			KConfigSetCBufferSize(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"cache-size");
		if(str_start) {
			KConfigSetCacheSize(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"min-level");
		if(str_start) {
			KConfigSetMinLevel(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"max-level");
		if(str_start) {
			KConfigSetMaxLevel(parseOptionInt(argv[i])+1);
			continue;
		}
		else {
			true_args[num_true_args] = strdup(argv[i]);
			num_true_args++;
		}
	}

	createOutputFilename();

	true_args = realloc(true_args,num_true_args*sizeof(char*));

	*num_args = num_true_args;
	*real_args = true_args;
}

// look for any kremlin specific inputs to the program
int main(int argc, char* argv[]) {
	int num_args = 0;;
	char** real_args;

	KConfigInit();
	__kremlin_idbg = 0;
	__kremlin_output_filename = calloc(sizeof(char), 20);
	strcat(__kremlin_output_filename,"kremlin.bin");

	parseKremlinOptions(argc,argv,&num_args,&real_args);

	if(__kremlin_idbg == 0) {
		(void)signal(SIGINT,dbg_int);
	}
	else {
		fprintf(stderr,"[kremlin] Interactive debugging mode enabled.\n");
	}

	if(__kremlin_level_to_log == -1) {
    	fprintf(stderr, "[kremlin] min level = %d, max level = %d\n", KConfigGetMinLevel(), KConfigGetMaxLevel());
	}
	else {
    	fprintf(stderr, "[kremlin] logging only level %d\n", __kremlin_level_to_log);
	}

	fprintf(stderr,"[kremlin] writing data to: %s\n", argGetOutputFileName());

	int i;
	char** start = &argv[argc - num_args-1];
	start[0] = strdup(argv[0]);

#if 0
	for (i=0; i<=num_args; i++) {
		fprintf(stderr, "arg %d: %s\n", i, start[i]);
	}
#endif
	__main(num_args+1, start);
}
