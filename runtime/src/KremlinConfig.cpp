#include <iostream>
#include <cassert>
#include "KremlinConfig.hpp"

void KremlinConfig::print() {
	std::cerr << "Kremlin Configuration:\n";
	std::cerr << "\tMin profiled level: " << min_profiled_level << "\n";
	std::cerr << "\tMax profiled level: " << max_profiled_level << "\n";
	std::cerr << "\tSummarize recursive regions? "
		<< (summarize_recursive_regions ? "YES" : "NO") << "\n";
	std::cerr << "\tProfile output file: " << profile_output_filename << "\n";
	std::cerr << "\tDebug output file: " << debug_output_filename << "\n";
}
