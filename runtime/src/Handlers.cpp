#include "debug.h"
#include "config.h"
#include "KremlinProfiler.hpp"
#include "ProgramRegion.hpp"
#include "MShadow.h"
#include "Table.h"

// TODO: make these static const members of profiler
#define LOAD_COST           4
#define STORE_COST          1
#define MALLOC_COST         100
#define FREE_COST           10

void KremlinProfiler::checkTimestamp(int index, ProgramRegion* region, Timestamp value) {
#ifndef NDEBUG
	if (value > getCurrentTime() - region->start) {
		fprintf(stderr, "index = %d, value = %lld, current time = %lld, region start = %lld\n", 
		index, value, getCurrentTime(), region->start);
		assert(0);
	}
#endif
}

void ProgramRegion::updateCriticalPathLength(Timestamp value) {
	this->cp = MAX(value, this->cp);
	MSG(3, "updateCriticalPathLength : value = %llu\n", this->cp);	
	this->sanityCheck();
#ifndef NDEBUG
	//assert(value <= profiler->getCurrentTime() - this->start);
	if (value > profiler->getCurrentTime() - this->start) {
		fprintf(stderr, "value = %lld, current time = %lld, region start = %lld\n", 
		value, profiler->getCurrentTime(), this->start);
		assert(0);
	}
#endif
}

Time KremlinProfiler::getControlDependenceAtIndex(Index index) {
	assert(control_dependence_table != NULL);
	assert(cdt_read_ptr >=  0);
	return *(cdt_current_base + index);
}

/*
 * Register Shadow Memory 
 */
static Table *shadow_reg_file; // TODO: static member of KremlinProfiler?

void KremlinProfiler::initShadowRegisterFile(Index depth) { 
	shadow_reg_file = NULL;
}

void KremlinProfiler::setRegisterFileTable(Table* table) { 
	shadow_reg_file = table;
}

Table* KremlinProfiler::getRegisterFileTable() { 
	return shadow_reg_file;
}


void KremlinProfiler::zeroRegistersAtIndex(Index index) {
	if (index >= shadow_reg_file->col)
		return;

	MSG(3, "zeroRegistersAtIndex col [%d] in table [%d, %d]\n",
		index, shadow_reg_file->row, shadow_reg_file->col);
	Reg i;
	assert(shadow_reg_file != NULL);
	for (i=0; i<shadow_reg_file->row; i++) {
		setRegisterTimeAtIndex(0ULL, i, index);
	}
}
Time KremlinProfiler::getRegisterTimeAtIndex(Reg reg, Index index) {
	MSG(3, "RShadowGet [%d, %d] in table [%d, %d]\n",
		reg, index, shadow_reg_file->row, shadow_reg_file->col);
	assert(reg < shadow_reg_file->row);	
	assert(index < shadow_reg_file->col);
	int offset = shadow_reg_file->getOffset(reg, index);
	Time ret = shadow_reg_file->array[offset];
	return ret;
}

void KremlinProfiler::setRegisterTimeAtIndex(Time time, Reg reg, Index index) {
	MSG(3, "RShadowSet [%d, %d] in table [%d, %d]\n",
		reg, index, shadow_reg_file->row, shadow_reg_file->col);
	assert(reg < shadow_reg_file->row);
	assert(index < shadow_reg_file->col);
	int offset = shadow_reg_file->getOffset(reg, index);
	MSG(3, "RShadowSet: dest = 0x%x value = %d reg = %d index = %d offset = %d\n", 
		&(shadow_reg_file->array[offset]), time, reg, index, offset);
	assert(shadow_reg_file != NULL);
	shadow_reg_file->array[offset] = time;
}


// BEGIN: move to iteractive debugger file
static inline void printTArray(Time* times, Index depth) {
	Index index;
	for (index = 0; index < depth; ++index) {
		MSG(0,"%u:%llu ",index,times[index]);
	}
}

static inline void printLoadDebugInfo(Addr addr, UInt dest, Time* times, Index depth) {
    MSG(0, "LOAD: ts[%u] = ts[0x%x] -- { ", dest, addr);
	printTArray(times,depth);
	MSG(0," }\n");
}

static inline void printStoreDebugInfo(UInt src, Addr addr, Time* times, Index depth) {
    MSG(0, "STORE: ts[0x%x] = ts[%u] -- { ", addr, src);
	printTArray(times,depth);
	MSG(0," }\n");
}

static inline void printStoreConstDebugInfo(Addr addr, Time* times, Index depth) {
    MSG(0, "STORE: ts[0x%x] = const -- { ", addr);
	printTArray(times,depth);
	MSG(0," }\n");
}
// END: move to iteractive debugger file

template <unsigned num_data_deps, unsigned cond, bool load_inst>
Time KremlinProfiler::calcNewDestTime(Time curr_dest_time, UInt32 src_reg, UInt32 src_offset, Index i) {
	if (num_data_deps > cond) {
		Time src_time = KremlinProfiler::getRegisterTimeAtIndex(src_reg, i);
		if (!load_inst) src_time += src_offset;
		return MAX(curr_dest_time, src_time);
	}
	else
		return curr_dest_time;
}

// TODO: once C++11 is widespread, give load_inst a default value of false
template <bool use_cdep, bool update_cp, unsigned num_data_deps, bool load_inst>
void KremlinProfiler::timestampUpdater(UInt32 dest_reg, 
										UInt32 src0_reg, UInt32 src0_offset,
										UInt32 src1_reg, UInt32 src1_offset,
										UInt32 src2_reg, UInt32 src2_offset,
										UInt32 src3_reg, UInt32 src3_offset,
										UInt32 src4_reg, UInt32 src4_offset,
										Addr src_addr,
										UInt32 mem_access_size
										) {

	Time* src_addr_times = NULL;
	Index end_index = getCurrNumInstrumentedLevels();

	if (load_inst) {
		assert(mem_access_size <= 8);
		Index region_depth = getCurrNumInstrumentedLevels();
		Level min_level = getLevelForIndex(0); // XXX: this doesn't seem right (-sat)
		src_addr_times = getShadowMemory()->get(src_addr, end_index, ProgramRegion::getVersionAtLevel(min_level), mem_access_size);

#ifdef KREMLIN_DEBUG
		printLoadDebugInfo(src_addr, dest_reg, src_addr_times, end_index);
#endif
	}

    for (Index index = 0; index < end_index; ++index) {
		Level i = getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		Time dest_time = 0;
		
		if (use_cdep) {
			Time cdep_time = dest_time = getControlDependenceAtIndex(index);
			assert(cdep_time <= getCurrentTime() - region->start);
		}

		if (load_inst) {
			// XXX: Why check timestamp here? Looks like this only occurs in KLoad
			// insts. If this is necessary, we might need to add checkTimestamp to
			// each src time (below).
			checkTimestamp(index, region, src_addr_times[index]);
			dest_time = MAX(dest_time, src_addr_times[index]);
		}

		dest_time = calcNewDestTime<num_data_deps, 0, load_inst>(dest_time, src0_reg, src0_offset, index);
		dest_time = calcNewDestTime<num_data_deps, 1, load_inst>(dest_time, src1_reg, src1_offset, index);
		dest_time = calcNewDestTime<num_data_deps, 2, load_inst>(dest_time, src2_reg, src2_offset, index);
		dest_time = calcNewDestTime<num_data_deps, 3, load_inst>(dest_time, src3_reg, src3_offset, index);
		dest_time = calcNewDestTime<num_data_deps, 4, load_inst>(dest_time, src4_reg, src4_offset, index);

		if (load_inst) dest_time += LOAD_COST;

		KremlinProfiler::setRegisterTimeAtIndex(dest_time, dest_reg, index);

		if (update_cp) {
        	region->updateCriticalPathLength(dest_time);
		}
    }
}

template <bool use_cdep, bool update_cp, bool phi_inst, bool load_inst>
void KremlinProfiler::handleVarArgDeps(UInt32 dest_reg, UInt32 src_reg, 
										Addr load_addr, UInt32 mem_access_size,
										unsigned num_srcs, va_list args) {
	UInt32 src_regs[5];
	UInt32 src_offsets[5];

	//UInt32 src0_reg, src1_reg, src2_reg, src3_reg, src4_reg;
	//UInt32 src0_offset, src1_offset, src2_offset, src3_offset, src4_offset;

	if (phi_inst || load_inst) {
		memset(&src_offsets, 0, sizeof(UInt32)*5);
		//src0_offset =  src1_offset = src2_offset =  src3_offset = src4_offset = 0;
	}

	unsigned arg_idx;
	for(arg_idx = 0; arg_idx < num_srcs; ++arg_idx) {
		unsigned place = arg_idx % 5;
		src_regs[place] = va_arg(args,UInt32);
		if (!phi_inst && !load_inst) {
			src_offsets[place] = va_arg(args,UInt32);
		}

		if (place == 0) {
			// TRICKY: once we start another round, we initialize all srcs
			// and src offsets to the first one (src0) to ensure that we
			// have a valid call to timestamp updater.
			// If we knew there were at least 5 sources, this would be
			// unnecessary (because it doesn't hurt to do use the same
			// source multiple times when calculating the timestamp.)
			src_regs[4] = src_regs[3] = src_regs[2] = src_regs[1] = src_regs[0];

			if (!phi_inst && !load_inst) {
				src_offsets[4] = src_offsets[3] = src_offsets[2] = src_offsets[1] = src_offsets[0];
			}
		}
		else if (place == 4) {
			timestampUpdater<true, true, 5, load_inst>(dest_reg, src_regs[0], src_offsets[0], 
														src_regs[1], src_offsets[1], 
														src_regs[2], src_offsets[2], 
														src_regs[3], src_offsets[3], 
														src_regs[4], src_offsets[4],
														load_addr, mem_access_size);
		}
	}

	if (phi_inst) {
		src_regs[arg_idx % 5] = src_reg;
	}

	// finish up any leftover args (beyond the last set of 5)
	if (arg_idx % 5 != 0 || phi_inst) {
		timestampUpdater<true, true, 5, load_inst>(dest_reg, src_regs[0], src_offsets[0], 
													src_regs[1], src_offsets[1], 
													src_regs[2], src_offsets[2], 
													src_regs[3], src_offsets[3], 
													src_regs[4], src_offsets[4],
													load_addr, mem_access_size);
	}
}

template <bool store_const>
void KremlinProfiler::timestampUpdaterStore(Addr dest_addr, UInt32 mem_access_size, Reg src_reg) {
	assert(mem_access_size <= 8);

	Time* dest_addr_times = ProgramRegion::getTimeArray();

	Index end_index = getCurrNumInstrumentedLevels();
    for (Index index = 0; index < end_index; ++index) {
		Level i = getLevelForIndex(index);
		ProgramRegion* region = ProgramRegion::getRegionAtLevel(i);

		Time control_dep_time = getControlDependenceAtIndex(index);
        Time dest_time = control_dep_time + STORE_COST;
		if (!store_const) {
			Time src_time = KremlinProfiler::getRegisterTimeAtIndex(src_reg, index);
        	dest_time = MAX(control_dep_time,src_time) + STORE_COST;
		}
		dest_addr_times[index] = dest_time;
        region->updateCriticalPathLength(dest_time);

#ifdef EXTRA_STATS
        region->storeCnt++;
#endif
    }

#ifdef KREMLIN_DEBUG
	if (store_const)
		printStoreConstDebugInfo(dest_addr, dest_addr_times, end_index);
	else
		printStoreDebugInfo(src_reg, dest_addr, dest_addr_times, end_index);
#endif

	Level min_level = getLevelForIndex(0); // XXX: see notes in KLoads
	getShadowMemory()->set(dest_addr, end_index, ProgramRegion::getVersionAtLevel(min_level), dest_addr_times, mem_access_size);
}


void KremlinProfiler::handleAssignConst(UInt dest_reg) {
    MSG(1, "_KAssignConst ts[%u]\n", dest_reg);
	idbgAction(KREM_ASSIGN_CONST,"## _KAssignConst(dest_reg=%u)\n",dest_reg);

    if (!enabled) return;

	timestampUpdater<true, true, 0, false>(dest_reg, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

// This function is mainly to help identify induction variables in the source
// code.
void KremlinProfiler::handleInduction(UInt dest_reg) {
    MSG(1, "KInduction to %u\n", dest_reg);
	idbgAction(KREM_INDUCTION,"## _KInduction(dest_reg=%u)\n",dest_reg);

    if (!enabled) return;

	timestampUpdater<true, true, 0, false>(dest_reg, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void KremlinProfiler::handleReduction(UInt op_cost, Reg dest_reg) {
    MSG(3, "KReduction ts[%u] with cost = %d\n", dest_reg, op_cost);
	idbgAction(KREM_REDUCTION, "## KReduction(op_cost=%u,dest_reg=%u)\n",op_cost,dest_reg);

    if (!enabled || !shouldInstrumentCurrLevel()) return;

	// XXX: do nothing??? (-sat)
}

void KremlinProfiler::handleTimestamp(UInt32 dest_reg, UInt32 num_srcs, va_list args) {
    MSG(1, "KTimestamp ts[%u] = (0..%u) \n", dest_reg,num_srcs);
	idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,num_srcs=%u,...)\n",dest_reg,num_srcs);

    if (!enabled) return;

	handleVarArgDeps<true, true, false, false>(dest_reg, 0, NULL, 0, num_srcs, args);
}

// XXX: not 100% sure this is the correct functionality
void KremlinProfiler::handleTimestamp0(UInt32 dest_reg) {
    MSG(1, "KTimestamp0 to %u\n", dest_reg);
	idbgAction(KREM_TS,"## _KTimestamp0(dest_reg=%u)\n",dest_reg);
    if (!enabled) return;

	timestampUpdater<true, true, 0, false>(dest_reg, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void KremlinProfiler::handleTimestamp1(UInt32 dest_reg, UInt32 src_reg, UInt32 src_offset) {
    MSG(3, "KTimestamp1 ts[%u] = ts[%u] + %u\n", dest_reg, src_reg, src_offset);
	idbgAction(KREM_TS,"## _KTimestamp1(dest_reg=%u,src_reg=%u,src_offset=%u)\n",dest_reg,src_reg,src_offset);

    if (!enabled) return;

	timestampUpdater<true, true, 1, false>(dest_reg, src_reg, src_offset, 0, 0, 0, 0, 0, 0, 0, 0);
}

void KremlinProfiler::handleTimestamp2(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset) {
    MSG(3, "KTimestamp2 ts[%u] = max(ts[%u] + %u,ts[%u] + %u)\n", dest_reg, src1_reg, src1_offset, src2_reg, src2_offset);
	idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!enabled) return;

	timestampUpdater<true, true, 2, false>(dest_reg, src1_reg, src1_offset, 
									src2_reg, src2_offset, 0, 0, 0, 0, 0, 0);
}

void KremlinProfiler::handleTimestamp3(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset) {
    MSG(3, "KTimestamp3 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!enabled) return;

	timestampUpdater<true, true, 3, false>(dest_reg, src1_reg, src1_offset, 
										src2_reg, src2_offset, 
										src3_reg, src3_offset, 0, 0, 0, 0);
}

void KremlinProfiler::handleTimestamp4(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32 src4_reg, UInt32 src4_offset) {
    MSG(3, "KTimestamp4 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset,src4_reg,src4_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!enabled) return;

	timestampUpdater<true, true, 4, false>(dest_reg, src1_reg, src1_offset, 
										src2_reg, src2_offset, 
										src3_reg, src3_offset, 
										src4_reg, src4_offset, 0, 0);
}

void KremlinProfiler::handleTimestamp5(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset) {
    MSG(3, "KTimestamp5 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset,src4_reg,src4_offset,src5_reg,src5_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!enabled) return;

	timestampUpdater<true, true, 5, false>(dest_reg, src1_reg, src1_offset, 
										src2_reg, src2_offset, 
										src3_reg, src3_offset, 
										src4_reg, src4_offset, 
										src5_reg, src5_offset);
}

void KremlinProfiler::handleTimestamp6(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
src6_reg, UInt32 src6_offset) {
    MSG(3, "KTimestamp6 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u, ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset,src4_reg,src4_offset,src5_reg,src5_offset,src6_reg,src6_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!enabled) return;

	timestampUpdater<true, false, 5, false>(dest_reg, src1_reg, src1_offset, 
										src2_reg, src2_offset, 
										src3_reg, src3_offset, 
										src4_reg, src4_offset, 
										src5_reg, src5_offset);
	timestampUpdater<false, true, 2, false>(dest_reg, dest_reg, 0, 
										src6_reg, src6_offset, 0, 0, 0, 0, 0, 0);
}

void KremlinProfiler::handleTimestamp7(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
src6_reg, UInt32 src6_offset, UInt32 src7_reg, UInt32 src7_offset) {
    MSG(3, "KTimestamp7 ts[%u] = max(ts[%u] + %u,ts[%u] + %u, ts[%u] + %u,"
	"ts[%u] + %u, ts[%u] + %u, ts[%u] + %u, ts[%u] + %u)\n",
	  dest_reg, src1_reg, src1_offset, src2_reg, src2_offset, src3_reg,
	  src3_offset, src4_reg, src4_offset, src5_reg, src5_offset,
	  src6_reg, src6_offset, src7_reg, src7_offset);
	// TODO: fix next line
	//idbgAction(KREM_TS,"## _KTimestamp(dest_reg=%u,src1_reg=%u,src1_offset=%u,src2_reg=%u,src2_offset=%u)\n",dest_reg,src1_reg,src1_offset,src2_reg,src2_offset);

    if (!enabled) return;

	timestampUpdater<true, false, 5, false>(dest_reg, src1_reg, src1_offset, 
										src2_reg, src2_offset, 
										src3_reg, src3_offset, 
										src4_reg, src4_offset, 
										src5_reg, src5_offset);
	timestampUpdater<false, true, 3, false>(dest_reg, dest_reg, 0, 
										src6_reg, src6_offset, 
										src7_reg, src7_offset, 0, 0, 0, 0);
}

void KremlinProfiler::handleLoad(Addr src_addr, Reg dest_reg, UInt32 mem_access_size, UInt32 num_srcs, va_list args) {
    MSG(1, "KLoad ts[%u] = max(ts[0x%x],...,ts_src%u[...]) + %u (access size: %u)\n", dest_reg,src_addr,num_srcs,LOAD_COST,mem_access_size);
	idbgAction(KREM_LOAD,"## _KLoad(src_addr=0x%x,dest_reg=%u,mem_access_size=%u,num_srcs=%u,...)\n",src_addr,dest_reg,mem_access_size,num_srcs);

    if (!enabled) return;

	handleVarArgDeps<true, true, false, true>(dest_reg, 0, src_addr, mem_access_size, num_srcs, args);
}

void KremlinProfiler::handleLoad0(Addr src_addr, Reg dest_reg, UInt32 mem_access_size) {
    MSG(1, "load size %d ts[%u] = ts[0x%x] + %u\n", mem_access_size, dest_reg, src_addr, LOAD_COST);
	idbgAction(KREM_LOAD, "## KLoad0(Addr=0x%x,dest_reg=%u,mem_access_size=%u)\n",
		src_addr, dest_reg, mem_access_size);

    if (!enabled) return;

	timestampUpdater<true, true, 0, true>(dest_reg, 
											0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
											src_addr, mem_access_size);
    MSG(3, "load ts[%u] completed\n\n",dest_reg);
}

void KremlinProfiler::handleLoad1(Addr src_addr, Reg dest_reg, Reg src_reg, UInt32 mem_access_size) {
    MSG(1, "load1 ts[%u] = max(ts[0x%x],ts[%u]) + %u\n", dest_reg, src_addr, src_reg, LOAD_COST);
	idbgAction(KREM_LOAD,"## KLoad1(Addr=0x%x,src_reg=%u,dest_reg=%u,mem_access_size=%u)\n",src_addr,src_reg,dest_reg,mem_access_size);

    if (!enabled) return;

	timestampUpdater<true, true, 1, true>(dest_reg, 
											src_reg, 0, 0, 0, 0, 0, 0, 0, 0, 0,
											src_addr, mem_access_size);
}

void KremlinProfiler::handleStore(Reg src_reg, Addr dest_addr, UInt32 mem_access_size) {
    MSG(1, "store size %d ts[0x%x] = ts[%u] + %u\n", mem_access_size, dest_addr, src_reg, STORE_COST);
	idbgAction(KREM_STORE,"## KStore(src_reg=%u,dest_addr=0x%x,mem_access_size=%u)\n",src_reg,dest_addr,mem_access_size);

    if (!enabled) return;

	timestampUpdaterStore<false>(dest_addr, mem_access_size, src_reg);

    MSG(1, "store mem[0x%x] completed\n", dest_addr);
}


void KremlinProfiler::handleStoreConst(Addr dest_addr, UInt32 mem_access_size) {
    MSG(1, "KStoreConst ts[0x%x] = %u\n", dest_addr, STORE_COST);
	idbgAction(KREM_STORE,"## _KStoreConst(dest_addr=0x%x,mem_access_size=%u)\n",dest_addr,mem_access_size);

    if (!enabled) return;

	timestampUpdaterStore<true>(dest_addr, mem_access_size, 0);

    MSG(1, "store const mem[0x%x] completed\n", dest_addr);
}

void KremlinProfiler::handlePhi(Reg dest_reg, Reg src_reg, UInt32 num_ctrls, va_list args) {
    MSG(1, "KPhi ts[%u] = max(ts[%u],ts[ctrl0]...ts[ctrl%u])\n", dest_reg, src_reg,num_ctrls);
	idbgAction(KREM_PHI,"## KPhi (dest_reg=%u,src_reg=%u,num_ctrls=%u)\n",dest_reg,src_reg,num_ctrls);

    if (!enabled) return;

	handleVarArgDeps<false, false, true, false>(dest_reg, src_reg, NULL, 0, num_ctrls, args);
}

void KremlinProfiler::handlePhi1To1(Reg dest_reg, Reg src_reg, Reg ctrl_reg) {
    MSG(1, "KPhi1To1 ts[%u] = max(ts[%u], ts[%u])\n", dest_reg, src_reg, ctrl_reg);
	idbgAction(KREM_PHI,"## KPhi1To1 (dest_reg=%u,src_reg=%u,ctrl_reg=%u)\n",dest_reg,src_reg,ctrl_reg);

    if (!enabled) return;

	timestampUpdater<false, false, 2, false>(dest_reg, src_reg, 0, 
										ctrl_reg, 0, 0, 0, 0, 0, 0, 0);
}

void KremlinProfiler::handlePhi2To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg) {
    MSG(1, "KPhi2To1 ts[%u] = max(ts[%u], ts[%u], ts[%u])\n", dest_reg, src_reg, ctrl1_reg, ctrl2_reg);
	idbgAction(KREM_PHI,"## KPhi2To1 (dest_reg=%u,src_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u)\n",dest_reg,src_reg,ctrl1_reg,ctrl2_reg);

    if (!enabled) return;

	timestampUpdater<false, false, 3, false>(dest_reg, src_reg, 0, 
										ctrl1_reg, 0, 
										ctrl2_reg, 0, 0, 0, 0, 0);
}

void KremlinProfiler::handlePhi3To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg) {
    MSG(1, "KPhi3To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u])\n", dest_reg, src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg);
	idbgAction(KREM_PHI,"## KPhi3To1 (dest_reg=%u,src_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u,ctrl3_reg=%u)\n",dest_reg,src_reg,ctrl1_reg,ctrl2_reg,ctrl3_reg);

    if (!enabled) return;

	timestampUpdater<false, false, 4, false>(dest_reg, src_reg, 0, 
										ctrl1_reg, 0, 
										ctrl2_reg, 0, 
										ctrl3_reg, 0, 0, 0);
}

void KremlinProfiler::handlePhi4To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg) {
    MSG(1, "KPhi4To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest_reg, src_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg);
	idbgAction(KREM_PHI,"## KPhi4To1 (dest_reg=%u,src_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u,ctrl3_reg=%u,ctrl4_reg=%u)\n", dest_reg,src_reg,ctrl1_reg,ctrl2_reg,ctrl3_reg,ctrl4_reg);

    if (!enabled) return;

	timestampUpdater<false, false, 5, false>(dest_reg, src_reg, 0, 
										ctrl1_reg, 0, 
										ctrl2_reg, 0, 
										ctrl3_reg, 0, 
										ctrl4_reg, 0);
}

void KremlinProfiler::handlePhiCond4To1(Reg dest_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg) {
    MSG(1, "KPhi4To1 ts[%u] = max(ts[%u], ts[%u], ts[%u], ts[%u], ts[%u])\n", 
		dest_reg, dest_reg, ctrl1_reg, ctrl2_reg, ctrl3_reg, ctrl4_reg);
	idbgAction(KREM_CD_TO_PHI,"## KPhi4To1 (dest_reg=%u,ctrl1_reg=%u,ctrl2_reg=%u,ctrl3_reg=%u,ctrl4_reg=%u)\n",
		dest_reg,ctrl1_reg,ctrl2_reg,ctrl3_reg,ctrl4_reg);

    if (!enabled) return;

	// XXX: either 2nd template should be true or 2nd template in KPhiAddCond
	// should be false
	timestampUpdater<false, false, 5, false>(dest_reg, dest_reg, 0, 
										ctrl1_reg, 0, 
										ctrl2_reg, 0, 
										ctrl3_reg, 0, 
										ctrl4_reg, 0);
}

void KremlinProfiler::handlePhiAddCond(Reg dest_reg, Reg src_reg) {
    MSG(1, "KPhiAddCond ts[%u] = max(ts[%u], ts[%u])\n", dest_reg, src_reg, dest_reg);
	idbgAction(KREM_CD_TO_PHI,"## KPhiAddCond (dest_reg=%u,src_reg=%u)\n",dest_reg,src_reg);

    if (!enabled) return;

	timestampUpdater<false, true, 2, false>(dest_reg, dest_reg, 0, 
										src_reg, 0, 0, 0, 0, 0, 0, 0);
}
