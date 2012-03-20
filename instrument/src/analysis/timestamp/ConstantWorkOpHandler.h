#ifndef CONSTANT_WORK_OP_HANDLER_H
#define CONSTANT_WORK_OP_HANDLER_H

#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include "analysis/timestamp/TimestampHandler.h"
#include "analysis/timestamp/ValueClassifier.h"
#include "analysis/ControlDependence.h"
#include "analysis/InductionVariables.h"
#include "TimestampPlacer.h"

class TimestampAnalysis;

class ConstantWorkOpHandler : public TimestampHandler
{
    public:
    ConstantWorkOpHandler(TimestampAnalysis& timestampAnalysis, TimestampPlacer& ts_placer, InductionVariables& induc_vars);
    virtual ~ConstantWorkOpHandler();

    virtual ValueClassifier::Class getTargetClass() const;
    virtual Timestamp& getTimestamp(llvm::Value* val, Timestamp& ts);
    unsigned int getWork(llvm::Instruction* inst) const;
    void parseFromFile(const std::string& filename);

    private:
	/**
	 * The cost of an integer add operation.
	 */
	unsigned int int_add;

	/**
	 * The cost of an integer subtract operation.
	 */
	unsigned int int_sub;

	/**
	 * The cost of an integer multiplication operation.
	 */
	unsigned int int_mul;

	/**
	 * The cost of an integer division operation.
	 */
	unsigned int int_div;
	
	/**
	 * The cost of an integer modulus operation.
	 */
	unsigned int int_mod;

	/**
	 * The cost of an integer compare operation.
	 */
	unsigned int int_cmp;

	/**
	 * The cost of an floating point add operation.
	 */
	unsigned int fp_add;

	/**
	 * The cost of an floating point subtract operation.
	 */
	unsigned int fp_sub;

	/**
	 * The cost of an floating point multiplication operation.
	 */
	unsigned int fp_mul;

	/**
	 * The cost of an floating point division operation.
	 */
	unsigned int fp_div;
	
	/**
	 * The cost of an floating point modulus operation.
	 */
	unsigned int fp_mod;

	/**
	 * The cost of an floating point compare operation.
	 */
	unsigned int fp_cmp;

	/**
	 * The cost of logic operations.
	 */
	unsigned int logic;
    unsigned int mem_load;
    unsigned int mem_store;

    ControlDependence& cd;
    InductionVariables& induc_vars;
    llvm::LoopInfo& li;
    TimestampAnalysis& timestampAnalysis;
    TimestampPlacer& ts_placer;

    bool isLiveInRegion(llvm::BasicBlock& bb, llvm::Value& incoming_val);

    friend llvm::raw_ostream& operator<<(llvm::raw_ostream&, const ConstantWorkOpHandler&);
    friend std::ostream& operator<<(std::ostream&, const ConstantWorkOpHandler&);
};

std::ostream& operator<<(std::ostream& os, const ConstantWorkOpHandler& costs);
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ConstantWorkOpHandler& costs);

#endif // CONSTANT_WORK_OP_HANDLER_H
