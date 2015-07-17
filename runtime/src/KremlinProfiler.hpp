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
	static const unsigned LOAD_COST = 4;
	static const unsigned STORE_COST = 1;

	bool enabled; // true if profiling is on (i.e. enabled), false otherwise
	bool initialized; // true iff init was called without corresponding deinit

	Time curr_time; // the current time of the profiler (virtual)
	Level curr_level; // current level 
	Level min_level; // minimum level to instrument
	Level max_level; // maximum level to instrument
	Level max_active_level; // max level we have seen thusfar

	Index curr_num_instrumented_levels; // number of regions currently instrumented
	bool instrument_curr_level; // whether we should instrument the current level

	// program region management
	std::vector<ProgramRegion*, MPoolLib::PoolAllocator<ProgramRegion*> > program_regions;
	Version* level_versions;
	Time* level_times;
	static const unsigned int arraySize = 512;
	Version nextVersion;

	// A vector used to represent the call stack.
	std::vector<FunctionRegion*, MPoolLib::PoolAllocator<FunctionRegion*> > callstack;

	CID last_callsite_id;

	static const unsigned int FUNC_ARG_QUEUE_SIZE = 64;
	std::vector<Reg> function_arg_queue;
	unsigned int arg_queue_read_index;
	unsigned int arg_queue_write_index;

	bool waiting_for_register_table_init;
	uint64_t num_function_regions_entered;
	uint64_t num_register_tables_setup;;

	Table* control_dependence_table;
	int cdt_read_ptr;
	Time* cdt_current_base;

	unsigned int doall_threshold;

	// Width and height of control dependence table.
	static const unsigned CDEP_ROW = 256;
	static const unsigned CDEP_COL = 64;

	static const unsigned INIT_NUM_REGIONS = 64;
	ProgramRegion* getRegionAtLevel(Level l);
	void increaseNumRegions(unsigned num_new);

	unsigned getNumRegions() { return program_regions.size(); }
	void doubleNumRegions() {
		increaseNumRegions(program_regions.size());
	}

	void initProgramRegions(unsigned num_regions) {
		assert(program_regions.empty());
		increaseNumRegions(num_regions);

		initVersionArray();
		initTimeArray();
	}

	void deinitProgramRegions();

	void initVersionArray() {
		level_versions = new Version[arraySize];
		for (unsigned i = 0; i < arraySize; ++i) level_versions[i] = 0;
	}

	void initTimeArray() {
		level_times = new Time[arraySize];
		for (unsigned i = 0; i < arraySize; ++i) level_times[i] = 0;
	}

	Time* getLevelTimes() { return level_times; }
	Version* getVersionAtLevel(Level level) { return &level_versions[level]; }

	void issueVersionToLevel(Level level) {
		level_versions[level] = nextVersion++;	
	}

	/*!
	 * Initializes the control dependence table to size of CDEP_ROW by
	 * CDEP_COL.
	 * @pre Control dependence table either hasn't been initialized or it's been
	 * deinitialized after initialization.
	 * @post control_dependence_table points to a valid Table in memory
	 */
	void initControlDependences();

	/*!
	 * @pre Control dependence table hasn't been deinitialized since the last time
	 * it was initialized.
	 * @post control_dependence_table is nullptr
	 */
	void deinitControlDependences();

	/*!
	 * Clears current control dependence at specified depth (index). Also updates
	 * cdt_current_base to point to entry in control dependence table
	 * corresponding to the current control dependence.
	 *
	 * @param The index (depth) of current control dependence to set to zero.
	 *
	 * @pre The control dependence table has been initialized.
	 * @post Time of current control dependence will be zero at the given index.
	 */
	void initRegionControlDependences(Index index);

	static Table *shadow_reg_file;
	MShadow *shadow_mem;

	/*!
	 * @brief Returns number of shadow registers in the current function.
	 *
	 * @return Current number of shadow registers.
	 */
	unsigned getCurrNumShadowRegisters();

	/*!
	 * @brief Returns "depth" of shadow register file in current function.
	 *
	 * @return Number of levels in the current shadow register file.
	 */
	unsigned getShadowRegisterFileDepth();

	/*!
	 * @brief Sets to zero the timestamp in all registers at the given index 
	 * (i.e. depth).
	 *
	 * @param index The level at which to set all register timestamps to 0.
	 * @pre shadow_reg_file is non-nullptr
	 * @pre index is not larger than the shadow register's depth
	 */
	void zeroRegistersAtIndex(Index index);

	void updateCurrLevelInstrumentableStatus() {
		if (curr_level >= min_level && curr_level <= max_level)
			instrument_curr_level = true;
		else 
			instrument_curr_level = false;
	}

	/*!
	 * Calculates the maximum of the specified current time and the value
	 * stored in a specified shadow register at a given depth plus an
	 * additional offset. The additional offset is optional, being used only
	 * when specified by the template parameter.
	 *
	 * @tparam num_data_deps The number of data dependencies.
	 * @tparam data_dep The index of the current data dependency.
	 * @tparam ignore_offset Should we add the offset to the data dependence
	 * time?
	 * @param curr_time The current time.
	 * @param reg The shadow register number.
	 * @param offset The additional time added to value stored in reg.
	 * @param l The level (depth) of the shadow register.
	 * @return The calculated maximum (see description).
	 * @pre shadow_reg_file is non-nullptr.
	 * @pre reg is less than the current number of shadow registers.
	 * @pre l is less than the shadow register's depth.
	 */
	template <unsigned num_data_deps, unsigned data_dep, bool ignore_offset>
	Time calcMaxTime(Time curr_time, uint32_t reg, uint32_t offset, Level l);

	/*!
	 * @brief Updates the timestamp of the destination register based on a
	 * number of data and/or control dependences.
	 *
	 * Up to five shadow register files can be specified as data dependences
	 * when calculating the updated timestamp. Each of these shadow registers
	 * can also have an associated offset, which will be added to their
	 * current timestamp when calculating the destination's timestamp. A
	 * template parameter specifies whether we should use the most recent
	 * control dependence and whether this timestamp update should trigger an
	 * update of the critical path.
	 *
	 * @tparam use_ctrl_dependence Whether the current control dependence
	 * should be used when calculating the new timestamp.
	 * @tparam update_cp Whether we should update the critical path based on
	 * the calculated timestamp.
	 * @tparam num_data_deps The number of data dependencies specified as
	 * inputs that are to be used.
	 * @tparam use_shadow_mem_dependence Whether we should include shadow
	 * memory in the timestamp calculation.
	 *
	 * @param dest_reg The shadow register that will be updated.
	 * @param src0_reg, src1_reg, src2_reg, src3_reg, src4_reg The shadow
	 * registers that will be used as dependences.
	 * @param src0_offset, src1_offset, src2_offset, src3_offset, src4_offset
	 * The additional offsets added to the timestamps in the source shadow
	 * registers.
	 * @param addr The address of the shadow memory dependence.
	 * @param mem_access_size The memory access size.
	 *
	 * @pre shadow_reg_file is non-nullptr.
	 * @pre shadow_mem is non-nullptr.
	 * @pre dest_reg is less than the current number of shadow registers.
	 * @pre Any used src_reg is less than the current number of shadow registers.
	 * @pre Any unused src_reg and offset will be 0.
	 * @pre If not using shadow mem, addr should be nullptr and mem_access_size
	 * should be 0.
	 * @pre If we're using shadow mem, the memory access size is between 1 and
	 * 8.
	 */
	template <bool use_ctrl_dependence, 
				bool update_cp, 
				unsigned num_data_deps, 
				bool use_shadow_mem_dependence>
	void timestampUpdater(uint32_t dest_reg, 
							uint32_t src0_reg=0, uint32_t src0_offset=0,
							uint32_t src1_reg=0, uint32_t src1_offset=0,
							uint32_t src2_reg=0, uint32_t src2_offset=0,
							uint32_t src3_reg=0, uint32_t src3_offset=0,
							uint32_t src4_reg=0, uint32_t src4_offset=0,
							Addr src_addr=nullptr,
							uint32_t mem_access_size=0);

	/*
	 * @brief Handles timestamp update when we have an unspecified number of
	 * data dependencies.
	 *
	 * A wrapper for timestampUpdater that reads a vararg list of data
	 * dependencies (and optional offsets). Bundles of five data dependencies
	 * are sent to timestampUpdater.
	 *
	 * @tparam use_ctrl_dependence Whether the current control dependence
	 * should be used when calculating the new timestamp.
	 * @tparam update_cp Whether we should update the critical path based on
	 * the calculated timestamp.
	 * @tparam use_src_reg Should we use src_reg as an additional data
	 * dependency?
	 * @tparam use_offsets Whether we should expect offsets to be part of the
	 * vararg list and should therefore include them when calculating
	 * timestamps.
	 * @tparam use_shadow_mem_dependence Whether we should include shadow
	 * memory in the timestamp calculation.
	 *
	 * @param dest_reg The shadow register that will be updated.
	 * @param src_reg Shadow register to be used as an additional dependency
	 * (assuming use_src_reg is true).
	 * @param mem_dependency addr The memory address used for data dependency
	 * when use_shadow_mem_dependence is specified.
	 * @param mem_access_size The memory access size; used only when
	 * use_shadow_mem_dependence is set.
	 * @param num_var_args The number of shadow registers (and possibly 
	 * offsets) to read from the vararg list.
	 * @param arg_list The vararg list from which to read.
	 *
	 * @pre dest_reg is less than the current number of shadow registers.
	 * @pre If use_src_reg is false, src_reg should be 0.
	 * @pre All shadow registers specified in the vararg list are less 
	 * than the current number of shadow registers.
	 * @pre Any unused src_reg and offset will be 0.
	 * @pre If not using shadow mem, addr should be nullptr and mem_access_size
	 * should be 0.
	 * @pre If we're using shadow mem, the memory access size is between 1 and
	 * 8.
	 */
	template <bool use_ctrl_dependence, 
				bool update_cp, 
				bool use_src_reg,
				bool use_offsets,
				bool use_shadow_mem_dependence>
	void handleVariableNumArgs(uint32_t dest_reg, uint32_t src_reg, 
							Addr mem_dependency_addr, uint32_t mem_access_size,
							unsigned num_var_args, va_list arg_list);

	template <bool store_const>
	void timestampUpdaterStore(Addr dest_addr, uint32_t mem_access_size, Reg src_reg);

	/*!
	 * Pushes new function region  onto function call stack.
	 *
	 * @post Function call stack will not be empty.
	 */
	void addFunctionToStack(CID callsite_id);

	/*!
	 * Pops function region from callstack
	 * @pre Callstack is not empty.
	 * @pre All function regions have had their tables setup.
	 */
	void callstackPop();

	Table* getRegisterFileTable() { return shadow_reg_file; }

	void setRegisterFileTable(Table* table) { 
		assert(table != nullptr);
		shadow_reg_file = table;
	}


public:
	KremlinProfiler(Level min, Level max) :
		enabled(false),
		initialized(false),
		curr_time(0),
		curr_level(-1),
		min_level(min),
		max_level(max),
		max_active_level(0),
		curr_num_instrumented_levels(0),
		instrument_curr_level(false),
		waiting_for_register_table_init(false),
		num_function_regions_entered(0),
		num_register_tables_setup(0),
		control_dependence_table(nullptr),
		cdt_read_ptr(0),
		cdt_current_base(nullptr),
		doall_threshold(5),
		shadow_mem(nullptr) {}

	~KremlinProfiler() {}

	void init();
	void deinit();
	void cleanup();

	/*!
	 * @brief Gets the timestamp in the specified register at the given index
	 * (i.e. depth).
	 *
	 * @param reg The shadow register number.
	 * @param index The depth at which to get the timestamp.
	 * @return The value in the specified shadow register.
	 * @pre shadow_reg_file is non-nullptr
	 * @pre reg is less than the current number of shadow registers.
	 * @pre index is not larger than the shadow register's depth
	 */
	Time getRegisterTimeAtIndex(Reg reg, Index index);

	/*!
	 * @brief Sets the timestamp in the specified register at the given index
	 * (i.e. depth) to a given value.
	 *
	 * @param time The time value to which to set the register value.
	 * @param reg The shadow register number.
	 * @param index The depth at which to get the timestamp.
	 * @pre shadow_reg_file is non-nullptr
	 * @pre reg is less than the current number of shadow registers.
	 * @pre index is not larger than the shadow register's depth
	 */
	void setRegisterTimeAtIndex(Time time, Reg reg, Index index);

	// TODO: is the following function necessary?
	bool waitingForRegisterTableSetup() {return this->waiting_for_register_table_init; }
	void waitForRegisterTableSetup() { this->waiting_for_register_table_init = true; }
	void finishRegisterTableSetup() { this->waiting_for_register_table_init = false; }

	void enable() { this->enabled = true; }
	void disable() { this->enabled = false; }
	bool isEnabled() { return this->enabled; }

	Time getCurrentTime() { return this->curr_time; }
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
	void increaseTime(uint32_t amount) { curr_time += amount; } // XXX: uint32_t -> Time?

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
		if (curr_level < min_level) {
			curr_num_instrumented_levels = 0;
		}

		else {
			curr_num_instrumented_levels = MIN(max_level, curr_level) - min_level + 1;	
		}
	}

	bool callstackIsEmpty() { return callstack.empty(); }

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

	/*!
	 * Returns time for the current control dependence at the specified region
	 * depth.
	 *
	 * @param index The depth in the region tree.
	 * @return The time of control dep at specified index.
	 *
	 * @pre Control dependence table has been initialized.
	 * @pre index doesn't exceed maximum depth of control dependence table.
	 */
	Time getControlDependenceAtIndex(Index index);

	void initShadowMemory();
	void deinitShadowMemory();

	void handleRegionEntry(SID regionId, RegionType regionType);
	void handleRegionExit(SID regionId, RegionType regionType);
	void handleFunctionExit();
	void handleLandingPad(SID regionId, RegionType regionType);
	void handleAssignConst(uint32_t dest_reg);
	void handleInduction(uint32_t dest_reg);
	void handleReduction(uint32_t op_cost, Reg dest_reg);
	void handleTimestamp(uint32_t dest_reg, uint32_t num_srcs, va_list args);
	void handleTimestamp0(uint32_t dest_reg);
	void handleTimestamp1(uint32_t dest_reg, uint32_t src_reg, uint32_t src_offset);
	void handleTimestamp2(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t src2_reg, uint32_t src2_offset);
	void handleTimestamp3(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset);
	void handleTimestamp4(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t src4_reg, uint32_t src4_offset);
	void handleTimestamp5(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t
				src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t
				src4_reg, uint32_t src4_offset, uint32_t src5_reg, uint32_t src5_offset);
	void handleTimestamp6(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t
				src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t
				src4_reg, uint32_t src4_offset, uint32_t src5_reg, uint32_t src5_offset, uint32_t
				src6_reg, uint32_t src6_offset);
	void handleTimestamp7(uint32_t dest_reg, uint32_t src1_reg, uint32_t src1_offset, uint32_t
				src2_reg, uint32_t src2_offset, uint32_t src3_reg, uint32_t src3_offset, uint32_t
				src4_reg, uint32_t src4_offset, uint32_t src5_reg, uint32_t src5_offset, uint32_t
				src6_reg, uint32_t src6_offset, uint32_t src7_reg, uint32_t src7_offset);
	void handleLoad(Addr src_addr, Reg dest_reg, uint32_t mem_access_size, uint32_t num_srcs, va_list args);
	void handleLoad0(Addr src_addr, Reg dest_reg, uint32_t mem_access_size);
	void handleLoad1(Addr src_addr, Reg dest_reg, Reg src_reg, uint32_t mem_access_size);
	void handleStore(Reg src_reg, Addr dest_addr, uint32_t mem_access_size);
	void handleStoreConst(Addr dest_addr, uint32_t mem_access_size);
	void handlePhi(Reg dest_reg, Reg src_reg, uint32_t num_ctrls, va_list args);
	void handlePhi1To1(Reg dest_reg, Reg src_reg, Reg ctrl_reg);
	void handlePhi2To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg);
	void handlePhi3To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg);
	void handlePhi4To1(Reg dest_reg, Reg src_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg);
	void handlePhiCond4To1(Reg dest_reg, Reg ctrl1_reg, Reg ctrl2_reg, Reg ctrl3_reg, Reg ctrl4_reg);
	void handlePhiAddCond(Reg dest_reg, Reg src_reg);

	void handlePopCDep();
	void handlePushCDep(Reg cond);

	void handlePrepCall(CID callSiteId, uint64_t calledRegionId);
	void handleEnqueueArgument(Reg src);
	void handleEnqueueConstArgument();
	void handleDequeueArgument(Reg dest);
	void handlePrepRTable(uint32_t num_virt_regs, uint32_t nested_depth);
	void handleLinkReturn(Reg dest);
	void handleReturn(Reg src);
	void handleReturnConst();

	void checkTimestamp(int index, ProgramRegion* region, Timestamp value);

};

#endif // KREMLIN_PROFILER_HPP
