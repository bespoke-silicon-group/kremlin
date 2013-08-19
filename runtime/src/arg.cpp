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

KremlinConfiguration kremlin_config;

std::string __kremlin_output_filename;
int __kremlin_level_to_log = -1;

extern int __kremlin_debug;
extern int __kremlin_debug_level;

extern "C" int __main(int argc, char** argv);

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
	ss << kremlin_config.getMinProfiledLevel();
	__kremlin_output_filename += ss.str();
	ss.flush();

	if(kremlin_config.getMaxProfiledLevel() != (kremlin_config.getMinProfiledLevel())) {
		__kremlin_output_filename += "_";
		ss << kremlin_config.getMaxProfiledLevel();
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
		str_start = strstr(argv[i],"kremlin-disable-rsummary");
		if(str_start) {
			kremlin_config.disableRecursiveRegionSummarization();
			continue;
		}

		str_start = strstr(argv[i],"kremlin-output");
		if(str_start) {
			kremlin_config.setProfileOutputFilename(parseOptionStr(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-log-output");
		if(str_start) {
			kremlin_config.setDebugOutputFilename(parseOptionStr(argv[i]));
			continue;
		}


		str_start = strstr(argv[i],"--kremlin-shadow-mem-type");
		if(str_start) {
			int type = parseOptionInt(argv[i]);
			assert(type >= 0 && type < 4);
			switch(type) {
				case 0: { 
					kremlin_config.setShadowMemType(ShadowMemoryBase); 
					break;
				}
				case 1: { 
					kremlin_config.setShadowMemType(ShadowMemorySTV);
					break;
				}
				case 2: { 
					kremlin_config.setShadowMemType(ShadowMemorySkadu);
					break;
				}
				case 3: { 
					kremlin_config.setShadowMemType(ShadowMemoryDummy);
					break;
				}
				default: { 
					assert(0);
				}
			}
			continue;
		}

		str_start = strstr(argv[i],"kremlin-shadow-mem-gc-period");
		if(str_start) {
			kremlin_config.setShadowMemGarbageCollectionPeriod(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-compress-shadow-mem");
		if(str_start) {
			kremlin_config.enableShadowMemCompression();
			continue;
		}

		str_start = strstr(argv[i],"kremlin-cbuffer-size");
		if(str_start) {
			kremlin_config.setNumCompressionBufferEntries(
							parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-shadow-mem-cache-size");
		if(str_start) {
			kremlin_config.setShadowMemCacheSizeInMB(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-min-level");
		if(str_start) {
			kremlin_config.setMinProfiledLevel(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-max-level");
		if(str_start) {
			kremlin_config.setMaxProfiledLevel(parseOptionInt(argv[i])+1);
			continue;
		}
		else {
			//true_args[num_true_args] = strdup(argv[i]);
			true_args.push_back(strdup(argv[i]));
			//num_true_args++;
		}
	}

	createOutputFilename(); // FIXME: this doesn't have any effect now

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
    	fprintf(stderr, "[kremlin] min level = %d, max level = %d\n", kremlin_config.getMinProfiledLevel(), kremlin_config.getMaxProfiledLevel());
	}
	else {
    	fprintf(stderr, "[kremlin] logging only level %d\n", __kremlin_level_to_log);
	}

	fprintf(stderr,"[kremlin] writing profiling data to: %s\n", kremlin_config.getProfileOutputFilename());
	fprintf(stderr,"[kremlin] writing log to: %s\n", kremlin_config.getDebugOutputFilename());

	int i;
	char** start = &argv[argc - num_args-1];
	start[0] = strdup(argv[0]);

#if 0
	for (i=0; i<=num_args; i++) {
		fprintf(stderr, "arg %d: %s\n", i, start[i]);
	}
#endif
	__main(num_args+1, start);
	delete[] real_args; // don't understand how real_args is being used (-sat)
}
