#ifndef _KREMLINCONFIG_HPP_
#define _KREMLINCONFIG_HPP_

#include <string>
#include "ktypes.h"

enum ShadowMemoryType {
	ShadowMemoryBase = 0,
	ShadowMemorySTV = 1,
	ShadowMemorySkadu = 2,
	ShadowMemoryDummy = 3
};

class KremlinConfiguration {
private:
	Level min_profiled_level;
	Level max_profiled_level;

	ShadowMemoryType shadow_mem_type;

	UInt32 shadow_mem_cache_size_in_mb;

	UInt32 garbage_collection_period;

	bool compress_shadow_mem;
	UInt32 num_compression_buffer_entries;

	bool summarize_recursive_regions;

	std::string profile_output_filename;
	std::string debug_output_filename;
	
	// TODO: allow "doall_threshold" to be a config option

public:

	KremlinConfiguration() : compress_shadow_mem(false),
							min_profiled_level(0), max_profiled_level(32), 
							num_compression_buffer_entries(4096),
							shadow_mem_cache_size_in_mb(4), 
							shadow_mem_type(ShadowMemorySkadu),
							garbage_collection_period(1024), 
							summarize_recursive_regions(true), 
							profile_output_filename("kremlin.bin"),
							debug_output_filename("kremlin.debug.log") {}

	~KremlinConfiguration() {}

	void print();

	/* Getters for all private member variables. */
	Level getMinProfiledLevel() { return min_profiled_level; }
	Level getMaxProfiledLevel() { return max_profiled_level; }
	Level getNumProfiledLevels() { return max_profiled_level - min_profiled_level + 1; }
	ShadowMemoryType getShadowMemType() { return shadow_mem_type; }
	UInt32 getShadowMemCacheSizeInMB() { return shadow_mem_cache_size_in_mb; }
	UInt32 getShadowMemGarbageCollectionPeriod() { 
		return garbage_collection_period;
	}
	bool compressShadowMem() { return compress_shadow_mem; }
	UInt32 getNumCompressionBufferEntries() { 
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
	void setShadowMemType(ShadowMemoryType t) { shadow_mem_type = t; }
	void setShadowMemCacheSizeInMB(UInt32 s) {
		shadow_mem_cache_size_in_mb = s;
	}
	void setShadowMemGarbageCollectionPeriod(UInt32 p) { 
		garbage_collection_period = p;
	}
	void enableShadowMemCompression() { compress_shadow_mem = true; }
	void setNumCompressionBufferEntries(UInt32 n) { 
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
