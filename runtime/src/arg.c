// C++ headers
#include <vector>
#include <string>
#include <sstream>

// C headers
#include <signal.h> // for catching CTRL-V during debug

#include "kremlin.h"
#include "arg.h"
#include "config.h"

#include "debug.h"

//char * __kremlin_output_filename;
std::string __kremlin_output_filename;
int __kremlin_level_to_log = -1;

extern int __kremlin_debug;
extern int __kremlin_debug_level;

extern "C" int __main(int argc, char** argv);

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

char* parseOptionStr(char* option_str) {
	char *dbg_level_str = strtok(option_str,"= ");
	dbg_level_str = strtok(NULL,"= ");

	if(dbg_level_str) {
		return dbg_level_str;
	}
	else {
		fprintf(stderr,"ERROR: Couldn't parse int from option (%s)\n",option_str);
	}
}


void createOutputFilename() {
	__kremlin_output_filename.clear(); // clear out old name
	__kremlin_output_filename += "kremlin-L";
	
	// TODO: update to use to_string for C++11
	std::stringstream ss;
	ss << KConfigGetMinLevel();
	__kremlin_output_filename += ss.str();
	ss.flush();

	if(KConfigGetMaxLevel() != (KConfigGetMinLevel())) {
		__kremlin_output_filename += "_";
		ss << KConfigGetMaxLevel();
		__kremlin_output_filename += ss.str();
		ss.flush();
	}
	
	__kremlin_output_filename += ".bin";
}

void parseKremlinOptions(int argc, char* argv[], int& num_args, char**& real_args) {
	std::vector<char*> true_args;

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
		// TODO FIXME XXX: These option names are too generic: they need to be
		// unique to Kremlin. These may come up in normal program usage
		// and cause weird errors. This was exactly what happened for the
		// streamcluster benchmark in rodinia since one of its inputs had the
		// word "output" in it (which was previously a kremlin option). 
		str_start = strstr(argv[i],"disable-rsummary");
		if(str_start) {
			KConfigDisableRSummary();
			continue;
		}

		str_start = strstr(argv[i],"kremlin-output");
		if(str_start) {
			KConfigSetOutFileName(parseOptionStr(argv[i]));
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
			KConfigSetSkaduCacheSize(parseOptionInt(argv[i]));
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
			//true_args[num_true_args] = strdup(argv[i]);
			true_args.push_back(strdup(argv[i]));
			//num_true_args++;
		}
	}

	createOutputFilename();

	//true_args = realloc(true_args,num_true_args*sizeof(char*));
	
	real_args = new char*[true_args.size()];
	for (unsigned i = 0; i < true_args.size(); ++i) {
		real_args[i] = strdup(true_args[i]);
	}
	num_args = true_args.size();

	//*num_args = num_true_args;
	//*real_args = true_args;
}

// look for any kremlin specific inputs to the program
int main(int argc, char* argv[]) {
	int num_args = 0;;
	char** real_args;

	KConfigInit();
	__kremlin_idbg = 0;

	__kremlin_output_filename = "kremlin.bin";

	parseKremlinOptions(argc,argv,num_args,real_args);

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

	fprintf(stderr,"[kremlin] writing data to: %s\n", __kremlin_output_filename.c_str());

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
