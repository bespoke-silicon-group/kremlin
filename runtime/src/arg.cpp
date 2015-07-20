// C++ headers
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>

// following two includes are needed for GNU GetOpt arg parsing
#include <getopt.h>
#include <unistd.h>

#include "arg.hpp" // includes vector
#include "config.hpp"

#include "debug.hpp"

#if 0
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
#endif

void parseKremlinOptions(KremlinConfiguration &config, 
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
			{"kremlin-shadow-mem-type", required_argument, nullptr, 'c'},
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

			case 'c':
				if (strcmp(optarg, "base") == 0)
					config.setShadowMemType(ShadowMemoryBase); 
				else if (strcmp(optarg, "stv") == 0)
					config.setShadowMemType(ShadowMemorySTV);
				else if (strcmp(optarg, "skadu") == 0)
					config.setShadowMemType(ShadowMemorySkadu);
				else if (strcmp(optarg, "dummy") == 0)
					config.setShadowMemType(ShadowMemoryDummy);
				else {
					std::cerr << "ERROR: Invalid shadow memory type: " << optarg << std::endl;
					std::cerr << "Valid options are: {skadu, stv, base, dummy}" << std::endl;
					exit(1);
				}

				break;

			// TODO: The following cases are all very similar... factor these
			// into a higher-order function
			case 'd':
				try {
					int cache_size = std::stoi(optarg);
					if (cache_size < 0) throw std::domain_error("must be postive");
					config.setShadowMemCacheSizeInMB(cache_size);
				}
				catch (std::exception& e) {
					std::cerr << "kremlin: WARNING: invalid shadow memory cache size. "
						<< "(Reason: " << e.what() << "). Using default value.\n";
				}
				break;

			case 'e':
				try {
					int gc_period = std::stoi(optarg);
					if (gc_period < 0) throw std::domain_error("must be postive");
					config.setShadowMemGarbageCollectionPeriod(gc_period);
				}
				catch (std::exception& e) {
					std::cerr << "kremlin: WARNING: invalid garbage collection period. "
						<< "(Reason: " << e.what() << "). Using default value.\n";
				}
				break;

			case 'f':
				try {
					int cb_entries = std::stoi(optarg);
					if (cb_entries < 0) throw std::domain_error("must be postive");
					config.setNumCompressionBufferEntries(cb_entries);
				}
				catch (std::exception& e) {
					std::cerr << "kremlin: WARNING: invalid number of compression buffer entries. "
						<< "(Reason: " << e.what() << "). Using default value.\n";
				}
				break;

			case 'g':
				try {
					int min_lev = std::stoi(optarg);
					if (min_lev < 0) throw std::domain_error("must be postive");
					config.setMinProfiledLevel(min_lev);
				}
				catch (std::exception& e) {
					std::cerr << "kremlin: WARNING: invalid MIN profile level. " 
						<< "(Reason: " << e.what() << "). Using default value.\n";
				}
				break;

			case 'h':
				try {
					int max_lev = std::stoi(optarg);
					if (max_lev < 0) throw std::domain_error("must be postive");
					config.setMaxProfiledLevel(max_lev);
				}
				catch (std::exception& e) {
					std::cerr << "kremlin: WARNING: invalid MAX profile level. "
						<< "(Reason: " << e.what() << "). Using default value.\n";
				}
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
