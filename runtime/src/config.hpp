#ifndef _KREMLINCONFIG_HPP_
#define _KREMLINCONFIG_HPP_

#include <string>
#include "ktypes.h"

class KremlinConfiguration {
private:
	Level min_profiled_level;
	Level max_profiled_level;

	uint32_t shadow_mem_cache_size_in_mb;

	uint32_t garbage_collection_period;

	bool compress_shadow_mem;
	uint32_t num_compression_buffer_entries;

	bool summarize_recursive_regions;

	std::string profile_output_filename;
	std::string debug_output_filename;
	
	// TODO: allow "doall_threshold" to be a config option

public:

	KremlinConfiguration() : 
							min_profiled_level(0),
							max_profiled_level(32), 
							shadow_mem_cache_size_in_mb(4), 
							garbage_collection_period(1024), 
							compress_shadow_mem(false),
							num_compression_buffer_entries(4096),
							summarize_recursive_regions(true), 
							profile_output_filename("kremlin.bin"),
							debug_output_filename("kremlin.debug.log") {}

	~KremlinConfiguration() {}

	void print();

	/* Getters for all private member variables. */
	Level getMinProfiledLevel() { return min_profiled_level; }
	Level getMaxProfiledLevel() { return max_profiled_level; }
	Level getNumProfiledLevels() { return max_profiled_level - min_profiled_level + 1; }
	uint32_t getShadowMemCacheSizeInMB() { return shadow_mem_cache_size_in_mb; }
	uint32_t getShadowMemGarbageCollectionPeriod() { 
		return garbage_collection_period;
	}
	bool compressShadowMem() { return compress_shadow_mem; }
	uint32_t getNumCompressionBufferEntries() { 
		return num_compression_buffer_entries;
	}
	bool summarizeRecursiveRegions() { return summarize_recursive_regions; }
	const char* getProfileOutputFilename() { 
		return profile_output_filename.c_str();
	}
	const char* getDebugOutputFilename() {
		return debug_output_filename.c_str();
	}

	/* Setters for all private member variables. */
	void setMinProfiledLevel(Level l) { min_profiled_level = l; }
	void setMaxProfiledLevel(Level l) { max_profiled_level = l; }
	void setShadowMemCacheSizeInMB(uint32_t s) {
		shadow_mem_cache_size_in_mb = s;
	}
	void setShadowMemGarbageCollectionPeriod(uint32_t p) { 
		garbage_collection_period = p;
	}
	void enableShadowMemCompression() { compress_shadow_mem = true; }
	void setNumCompressionBufferEntries(uint32_t n) { 
		num_compression_buffer_entries = n;
	}
	void disableRecursiveRegionSummarization() { 
		summarize_recursive_regions = false;
	}
	void setProfileOutputFilename(const char* name) { 
		profile_output_filename.clear();
		profile_output_filename.append(name);
	}
	void setDebugOutputFilename(const char* name) { 
		debug_output_filename.clear();
		debug_output_filename.append(name);
	}
};

extern KremlinConfiguration kremlin_config;

#endif // _KREMLINCONFIG_HPP_
