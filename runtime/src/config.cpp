#include <iostream>
#include <cassert>
#include "config.h"

void KremlinConfiguration::print() {
	std::cerr << "Kremlin Configuration:\n";
	std::cerr << "\tMin profiled level: " << min_profiled_level << "\n";
	std::cerr << "\tMax profiled level: " << max_profiled_level << "\n";
	std::cerr << "\tShadow Memory Type: ";
	switch (shadow_mem_type) {
		case ShadowMemoryBase: {
			std::cerr << "Base" << "\n";
			break;
		}
		case ShadowMemorySTV: {
			std::cerr << "STV" << "\n";
			break;
		}
		case ShadowMemorySkadu: {
			std::cerr << "Skadu" << "\n";
			if (shadow_mem_cache_size_in_mb > 0) {
				std::cerr << "\t\tCache size: " 
					<< shadow_mem_cache_size_in_mb << "MB\n";
			}

			if (compress_shadow_mem) {
				std::cerr << "\t\tCompression enabled: ";
				std::cerr << num_compression_buffer_entries
					<< " compression buffer entries\n";
			}
			else
				std::cerr << "\t\tCompression disabled\n";

			if (garbage_collection_period > 0) {
				std::cerr << "\t\tGarbage collection enabled, period = "
					<< garbage_collection_period << "\n";
			}
			else
				std::cerr << "\t\tGarbage collection disabled.\n";

			break;
		}
		case ShadowMemoryDummy: {
			std::cerr << "Dummy" << "\n";
			break;
		}
		default: assert(0);
	}

	std::cerr << "\tSummarize recursive regions? "
		<< (summarize_recursive_regions ? "YES" : "NO") << "\n";

	std::cerr << "\tProfile output file: " << profile_output_filename << "\n";
	std::cerr << "\tDebug output file: " << debug_output_filename << "\n";
}
