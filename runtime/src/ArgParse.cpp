// C++ headers
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstring>

// following two includes are needed for GNU GetOpt arg parsing
#include <getopt.h>
#include <unistd.h>

#include "ArgParse.hpp" // includes vector
#include "KremlinConfig.hpp"

#include "debug.hpp"

#if 0
static void getCustomOutputFilename(KremlinConfig &config, std::string& filename) {
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
#endif

static void setNonNegativeOption(std::function<void(uint32_t)> setter, 
									std::string name) {
	try {
		int val = std::stoi(optarg);
		if (val < 0) throw std::domain_error("must be postive");
		setter(val);
	}
	catch (std::exception& e) {
		std::cerr << "kremlin: WARNING: invalid " << name << ". "
			<< "(Reason: " << e.what() << "). Using default value.\n";
	}
}

void parseKremlinOptions(KremlinConfig &config, 
							int argc, char* argv[], 
							std::vector<char*>& native_args) {
	native_args.push_back(argv[0]); // program name is always a native arg

	opterr = 0; // unknown opt isn't an error: it's a native program opt

	int disable_rs = 0;
	int enable_sm_compress = 0;
#ifdef KREMLIN_DEBUG
	int enable_idbg;
#endif

	while (true)
	{
		static struct option long_options[] =
		{
			{"kremlin-disable-rsummary", no_argument, &disable_rs, 1},
			{"kremlin-compress-shadow-mem", no_argument, &enable_sm_compress, 1},
#ifdef KREMLIN_DEBUG
			{"kremlin-idbg", no_argument, &enable_idbg, 1},
#endif

			{"kremlin-output", required_argument, nullptr, 'a'},
			{"kremlin-log-output", required_argument, nullptr, 'b'},
			{"kremlin-shadow-mem-cache-size", required_argument, nullptr, 'd'},
			{"kremlin-shadow-mem-gc-period", required_argument, nullptr, 'e'},
			{"kremlin-cbuffer-size", required_argument, nullptr, 'f'},
			{"kremlin-min-level", required_argument, nullptr, 'g'},
			{"kremlin-max-level", required_argument, nullptr, 'h'},
			{nullptr, 0, nullptr, 0} // indicates end of options
		};

		int currind = optind; // need to cache this for native arg storage

		int option_index = 0;
		int c = getopt_long(argc, argv, "abc:d:f:", long_options, &option_index);

		// finished processing args
		if (c == -1) break;

		switch (c) {
			case 0:
				// 0 indicates a flag, so nothing to do here.
				// We'll handle flags after the switch.
				break;

			case 'a':
				config.setProfileOutputFilename(optarg);
				break;

			case 'b':
				config.setDebugOutputFilename(optarg);
				break;

			case 'd':
				setNonNegativeOption(std::bind(&KremlinConfig::setShadowMemCacheSizeInMB,
												&config, std::placeholders::_1), 
										"shadow memory cache size");
				break;

			case 'e':
				setNonNegativeOption(std::bind(&KremlinConfig::setShadowMemGarbageCollectionPeriod, 
												&config, std::placeholders::_1),
										"garbage collection period");
				break;

			case 'f':
				setNonNegativeOption(std::bind(&KremlinConfig::setNumCompressionBufferEntries, 
												&config, std::placeholders::_1),
										"number of compression buffer entries");
				break;

			case 'g':
				setNonNegativeOption(std::bind(&KremlinConfig::setMinProfiledLevel, 
												&config, std::placeholders::_1),
										"MIN profile level");
				break;

			case 'h':
				setNonNegativeOption(std::bind(&KremlinConfig::setMaxProfiledLevel, 
												&config, std::placeholders::_1),
										"MAX profile level");
				break;

			case '?':
				if (optopt) {
					native_args.push_back(strdup((char*)(&c)));
				}
				else {
					native_args.push_back(argv[currind]);
				}
				break;

			default:
				abort();
		}
	}

	// handle some kremlin-specific flags being set
	if (enable_sm_compress)
		config.enableShadowMemCompression();

	if (disable_rs)
		config.disableRecursiveRegionSummarization();

#ifdef KREMLIN_DEBUG
	if (enable_idbg) {
		__kremlin_idbg = 1;
		__kremlin_idbg_run_state = Waiting;
	}
#endif

	// any remaining command line args are considered native
	if (optind < argc)
	{
		while (optind < argc) {
			native_args.push_back(argv[optind++]);
		}
	}
}
