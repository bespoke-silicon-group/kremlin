// C++ headers
#include <vector>
#include <string>
#include <sstream>

#include "arg.h"
#include "config.h"

#include "debug.h"

static int parseOptionInt(char* option_str) {
	char *dbg_level_str = strtok(option_str,"= ");
	dbg_level_str = strtok(NULL,"= ");

	if(dbg_level_str) {
		return atoi(dbg_level_str);
	}
	else {
		fprintf(stderr,"ERROR: Couldn't parse int from option (%s)\n",option_str);
	}
}

static char* parseOptionStr(char* option_str) {
	char *dbg_level_str = strtok(option_str,"= ");
	dbg_level_str = strtok(NULL,"= ");

	if(dbg_level_str) {
		return dbg_level_str;
	}
	else {
		fprintf(stderr,"ERROR: Couldn't parse string from option (%s)\n",option_str);
	}
}


static void getCustomOutputFilename(KremlinConfiguration &config, std::string& filename) {
	filename = "kremlin-L";
	
	// TODO: update to use to_string for C++11
	std::stringstream ss;
	ss << config.getMinProfiledLevel();
	filename += ss.str();
	ss.flush();

	if(config.getMaxProfiledLevel() != (config.getMinProfiledLevel())) {
		filename += "_";
		ss << config.getMaxProfiledLevel();
		filename += ss.str();
		ss.flush();
	}
	
	filename += ".bin";
}

void parseKremlinOptions(KremlinConfiguration &config, 
							int argc, char* argv[], 
							std::vector<char*> &program_args) {
	program_args.push_back(strdup(argv[0])); // program name is always a program arg

	for(unsigned i = 1; i < argc; ++i) {
		//fprintf(stderr,"parsing arg: %s\n",argv[i]);
		char *str_start;

#ifdef KREMLIN_DEBUG
		str_start = strstr(argv[i],"kremlin-idbg");

		if(str_start) {
			__kremlin_idbg = 1;
			__kremlin_idbg_run_state = Waiting;
			continue;
		}
#endif

		str_start = strstr(argv[i],"kremlin-disable-rsummary");
		if(str_start) {
			config.disableRecursiveRegionSummarization();
			continue;
		}

		str_start = strstr(argv[i],"kremlin-output");
		if(str_start) {
			config.setProfileOutputFilename(parseOptionStr(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-log-output");
		if(str_start) {
			config.setDebugOutputFilename(parseOptionStr(argv[i]));
			continue;
		}


		str_start = strstr(argv[i],"--kremlin-shadow-mem-type");
		if(str_start) {
			int type = parseOptionInt(argv[i]);
			assert(type >= 0 && type < 4);
			switch(type) {
				case 0: { 
					config.setShadowMemType(ShadowMemoryBase); 
					break;
				}
				case 1: { 
					config.setShadowMemType(ShadowMemorySTV);
					break;
				}
				case 2: { 
					config.setShadowMemType(ShadowMemorySkadu);
					break;
				}
				case 3: { 
					config.setShadowMemType(ShadowMemoryDummy);
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
			config.setShadowMemGarbageCollectionPeriod(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-compress-shadow-mem");
		if(str_start) {
			config.enableShadowMemCompression();
			continue;
		}

		str_start = strstr(argv[i],"kremlin-cbuffer-size");
		if(str_start) {
			config.setNumCompressionBufferEntries(
							parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-shadow-mem-cache-size");
		if(str_start) {
			config.setShadowMemCacheSizeInMB(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-min-level");
		if(str_start) {
			config.setMinProfiledLevel(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-max-level");
		if(str_start) {
			config.setMaxProfiledLevel(parseOptionInt(argv[i])+1);
			continue;
		}
		else {
			program_args.push_back(strdup(argv[i]));
		}
	}
}

