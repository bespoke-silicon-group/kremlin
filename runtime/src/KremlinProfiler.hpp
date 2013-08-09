#ifndef KREMLIN_PROFILER_HPP
#define KREMLIN_PROFILER_HPP

#include <vector>
#include <stdarg.h> /* for variable length args */
#include "ktypes.h"
#include "PoolAllocator.hpp"

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))

class MShadow;
class ProgramRegion;
class FunctionRegion;
class Table;

class KremlinProfiler {
private:
	bool enabled; // true if profiling is on (i.e. enabled), false otherwise
	bool initialized; // true iff init was called without corresponding deinit

	Time curr_time; // the current time of the profiler (virtual)
	Level curr_level; // current level 
	Level min_level; // minimum level to instrument
	Level max_level; // maximum level to instrument
	Level max_active_level; // max level we have seen thusfar

	Index curr_num_instrumented_levels; // number of regions currently instrumented
	bool instrument_curr_level; // whether we should instrument the current level

	// A vector used to represent the call stack.
	std::vector<FunctionRegion*, MPoolLib::PoolAllocator<FunctionRegion*> > callstack;

	CID last_callsite_id;

	static const unsigned int FUNC_ARG_QUEUE_SIZE = 64;
	std::vector<Reg> function_arg_queue;
	unsigned int arg_queue_read_index;
	unsigned int arg_queue_write_index;

	bool waiting_for_register_table_init;
	UInt64 num_function_regions_entered;
	UInt64 num_register_tables_setup;;

	Table* control_dependence_table;
	int cdt_read_ptr;
	Time* cdt_current_base;

	void initControlDependences();
	void deinitControlDependences();
	void initRegionControlDependences(Index index);


	enum ShadowMemoryType {
		ShadowMemoryBase = 0,
		ShadowMemorySTV = 1,
		ShadowMemorySkadu = 2,
		ShadowMemoryDummy = 3
	};

	MShadow *shadow_mem;

	void updateCurrLevelInstrumentableStatus() {
		if (curr_level >= min_level && curr_level <= max_level)
			instrument_curr_level = true;
		else 
			instrument_curr_level = false;
	}

	template <unsigned num_data_deps, unsigned cond, bool load_inst>
	Time calcNewDestTime(Time curr_dest_time, UInt32 src_reg, UInt32 src_offset, Index i);

	template <bool use_cdep, bool update_cp, unsigned num_data_deps, bool load_inst>
	void timestampUpdater(UInt32 dest_reg, 
							UInt32 src0_reg, UInt32 src0_offset,
							UInt32 src1_reg, UInt32 src1_offset,
							UInt32 src2_reg, UInt32 src2_offset,
							UInt32 src3_reg, UInt32 src3_offset,
							UInt32 src4_reg, UInt32 src4_offset,
							Addr src_addr=NULL,
							UInt32 mem_access_size=0);

	template <bool use_cdep, bool update_cp, bool phi_inst, bool load_inst>
	void handleVarArgDeps(UInt32 dest_reg, UInt32 src_reg, 
							Addr load_addr, UInt32 mem_access_size,
							unsigned num_srcs, va_list args);

	template <bool store_const>
	void timestampUpdaterStore(Addr dest_addr, UInt32 mem_access_size, Reg src_reg);

public:
	KremlinProfiler(Level min, Level max) {
		this->enabled = false;
		this->initialized = false;
		this->curr_time = 0;
		this->curr_level = -1;
		this->min_level = min;
		this->max_level = max;
		this->max_active_level = 0;
		this->curr_num_instrumented_levels = 0;
		this->instrument_curr_level = false;
		this->waiting_for_register_table_init = false;
		this->num_function_regions_entered = 0;
		this->num_register_tables_setup = 0;
		this->control_dependence_table = NULL;
		this->cdt_read_ptr = 0;
		this->cdt_current_base = NULL;
		this->shadow_mem = NULL;
	}

	~KremlinProfiler() {}

	void init();
	void deinit();
	void cleanup();

	static void initShadowRegisterFile(Index depth);
	static void deinitShadowRegisterFile() {}

	static Time getRegisterTimeAtIndex(Reg reg, Index index);
	static void setRegisterTimeAtIndex(Time time, Reg reg, Index index);
	static void zeroRegistersAtIndex(Index index);

	static void setRegisterFileTable(Table* table);
	static Table* getRegisterFileTable();

	// TODO: is the following function necessary?
	bool waitingForRegisterTableSetup() {return this->waiting_for_register_table_init; }
	void waitForRegisterTableSetup() { this->waiting_for_register_table_init = true; }
	void finishRegisterTableSetup() { this->waiting_for_register_table_init = false; }
	void incrementFunctionRegionCount() { this->num_function_regions_entered++; }
	void incrementSetupTableCount() { this->num_register_tables_setup++; }

	void enable() { this->enabled = true; }
	void disable() { this->enabled = false; }
	bool isEnabled() { return this->enabled; }

	int getCurrentTime() { return this->curr_time; }
	int getCurrentLevel() { return this->curr_level; }
	Level getCurrentLevelIndex() { return curr_level - min_level; }
	int getMinLevel() { return this->min_level; }
	int getMaxLevel() { return this->max_level; }
	int getMaxActiveLevel() { return this->max_active_level; }
	CID getLastCallsiteID() { return this->last_callsite_id; }
	MShadow* getShadowMemory() { return this->shadow_mem; }
	bool shouldInstrumentCurrLevel() { return instrument_curr_level; }

	int getArraySize() { return max_level - min_level + 1; }
	Index getCurrNumInstrumentedLevels() { return curr_num_instrumented_levels; }

	Level getLevelForIndex(Index index) { return min_level + index; }

	void setLastCallsiteID(CID cs_id) { this->last_callsite_id = cs_id; }
	void increaseTime(UInt32 amount) { curr_time += amount; } // XXX: UInt32 -> Time?

	void incrementLevel() { 
		++curr_level;
		updateCurrLevelInstrumentableStatus();
		updateCurrNumInstrumentedLevels();

		if (curr_level > max_active_level)
			max_active_level = curr_level;
	}
	void decrementLevel() { 
		--curr_level;
		updateCurrLevelInstrumentableStatus();
		updateCurrNumInstrumentedLevels();
	}

	/*! \brief Update number of levels being instrumented based on our current
	 * level.
	 */
	void updateCurrNumInstrumentedLevels() {
		if (!KConfigLimitLevel()) {
			curr_num_instrumented_levels = curr_level + 1;
		}

		else if (curr_level < min_level) {
			curr_num_instrumented_levels = 0;
		}

		else {
			curr_num_instrumented_levels = MIN(max_level, curr_level) - min_level + 1;	
		}
	}

	bool callstackIsEmpty() { return callstack.empty(); }

	/**
	 * Pushes new context onto function context stack.
	 */
	void addFunctionToStack(CID cid);

	/**
	 * Removes context at the top of the function context stack.
	 */
	void removeFunctionFromStack();

	FunctionRegion* getCurrentFunction();
	FunctionRegion* getCallingFunction();

	void initFunctionArgQueue() {
		function_arg_queue.resize(KremlinProfiler::FUNC_ARG_QUEUE_SIZE, -1);
		clearFunctionArgQueue();
	}

	void deinitFunctionArgQueue() {}

	void functionArgQueuePushBack(Reg src) {
		function_arg_queue[arg_queue_write_index++] = src;
		assert(arg_queue_write_index < function_arg_queue.size());
	}

	Reg functionArgQueuePopFront() {
		assert(arg_queue_read_index < arg_queue_write_index);
		return function_arg_queue[arg_queue_read_index++];
	}

	void clearFunctionArgQueue() {
		arg_queue_read_index = 0;
		arg_queue_write_index = 0;
	}

	void initShadowMemory();
	void deinitShadowMemory();

	Time getControlDependenceAtIndex(Index index);

	void handleRegionEntry(SID regionId, RegionType regionType);
	void handleRegionExit(SID regionId, RegionType regionType);
	void handleFunctionExit();
	void handleLandingPad(SID regionId, RegionType regionType);
	void handleAssignConst(UInt dest_reg);
	void handleInduction(UInt dest_reg);
	void handleReduction(UInt op_cost, Reg dest_reg);
	void handleTimestamp(UInt32 dest_reg, UInt32 num_srcs, va_list args);
	void handleTimestamp0(UInt32 dest_reg);
	void handleTimestamp1(UInt32 dest_reg, UInt32 src_reg, UInt32 src_offset);
	void handleTimestamp2(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset);
	void handleTimestamp3(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset);
	void handleTimestamp4(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32 src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32 src4_reg, UInt32 src4_offset);
	void handleTimestamp5(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
				src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
				src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset);
	void handleTimestamp6(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
				src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
				src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
				src6_reg, UInt32 src6_offset);
	void handleTimestamp7(UInt32 dest_reg, UInt32 src1_reg, UInt32 src1_offset, UInt32
				src2_reg, UInt32 src2_offset, UInt32 src3_reg, UInt32 src3_offset, UInt32
				src4_reg, UInt32 src4_offset, UInt32 src5_reg, UInt32 src5_offset, UInt32
				src6_reg, UInt32 src6_offset, UInt32 src7_reg, UInt32 src7_offset);
	void handleLoad(Addr src_addr, Reg dest_reg, UInt32 mem_access_size, UInt32 num_srcs, va_list args);
	void handleLoad0(Addr src_addr, Reg dest_reg, UInt32 mem_access_size);
	void handleLoad1(Addr src_addr, Reg dest_reg, Reg src_reg, UInt32 mem_access_size);
	void handleStore(Reg src_reg, Addr dest_addr, UInt32 mem_access_size);
	void handleStoreConst(Addr dest_addr, UInt32 mem_access_size);
	void handlePhi(Reg dest_reg, Reg src_reg, UInt32 num_ctrls, va_list args);
	void handlePhi1To1(Reg dest_reg, Reg src_reg, Reg ctrl_reg);
	void handlePhi2To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg);
	void handlePhi3To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg);
	void handlePhi4To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg);
	void handlePhiCond4To1(Reg dest_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg);
	void handlePhiAddCond(Reg dest_reg, Reg src_reg);

	void handlePopCDep();
	void handlePushCDep(Reg cond);

	void handlePrepCall(CID callSiteId, UInt64 calledRegionId);
	void handleEnqueueArgument(Reg src);
	void handleEnqueueConstArgument();
	void handleDequeueArgument(Reg dest);
	void handlePrepRTable(UInt num_virt_regs, UInt nested_depth);
	void handleLinkReturn(Reg dest);
	void handleReturn(Reg src);
	void handleReturnConst();

	void checkTimestamp(int index, ProgramRegion* region, Timestamp value);

};

#endif // KREMLIN_PROFILER_HPP
