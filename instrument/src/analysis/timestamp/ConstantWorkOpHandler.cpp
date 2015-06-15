#define DEBUG_TYPE __FILE__

#include <llvm/Support/Debug.h>
#include <foreach.h>
#include <fstream>
#include <sstream>
#include "analysis/timestamp/ConstantWorkOpHandler.h"
#include "analysis/timestamp/TimestampAnalysis.h"
#include "analysis/timestamp/UnknownOpCodeException.h"
#include "analysis/timestamp/TimestampCandidate.h"
#include "UnsupportedOperationException.h"

// Default work
#define INT_ADD 1
#define INT_SUB 1
#define INT_MUL 2
#define INT_DIV 10
#define INT_MOD 10

#define FP_ADD 2
#define FP_SUB 2
#define FP_MUL 5
#define FP_DIV 20
#define FP_MOD 20

#define LOGIC 1
#define INT_CMP 1
#define FP_CMP 2 

#define STORE 1
#define LOAD 4

#define FILE_READ 10
#define FILE_WRITE 10

using namespace llvm;

/**
 * Constructs the handler for constant work ops with their default costs.
 */
ConstantWorkOpHandler::ConstantWorkOpHandler(TimestampAnalysis& timestampAnalysis, TimestampPlacer& ts_placer, InductionVariables& induc_vars) :
	int_add_cost(INT_ADD),
	int_sub_cost(INT_SUB),
	int_mul_cost(INT_MUL),
	int_div_cost(INT_DIV),
	int_mod_cost(INT_MOD),
	int_cmp_cost(INT_CMP),
	fp_add_cost(FP_ADD),
	fp_sub_cost(FP_SUB),
	fp_mul_cost(FP_MUL),
	fp_div_cost(FP_DIV),
	fp_mod_cost(FP_MOD),
	fp_cmp_cost(FP_CMP),
	logic_op_cost(LOGIC),
    mem_load_cost(LOAD),
    mem_store_cost(STORE),
    _controlDep(ts_placer.getAnalyses().cd),
    _inductionVars(induc_vars),
    _loopInfo(ts_placer.getAnalyses().li),
    _timestampAnalysis(timestampAnalysis),
    _timestampPlacer(ts_placer)
{
}

ConstantWorkOpHandler::~ConstantWorkOpHandler()
{
}

/**
 * @copydoc
 */
ValueClassifier::Class ConstantWorkOpHandler::getTargetClass() const
{
    return ValueClassifier::CONSTANT_WORK_OP;
}

/**
 * @return true if incoming_val is contained within the loop that bb is a part
 * of.
 */
// XXX: Design hack...is this already deprecated?
// Only return true for incoming values that are live ins for the region
// specified by bb. Return true for any incoming that comes before the portion
// of loops bb.
bool ConstantWorkOpHandler::isLiveInRegion(llvm::BasicBlock& bb, llvm::Value& incoming_val)
{
    //LOG_DEBUG() << "isLive for bb: " << bb.getName() << " val: " << incoming_val << "\n";
    Loop* loop_associated_with_bb = _loopInfo.getLoopFor(&bb);

    // Can't come before a loop that doesn't exist.
    if(loop_associated_with_bb == NULL) return false;

    Instruction* incoming_inst = dyn_cast<Instruction>(&incoming_val);
    if(!incoming_inst) return false;

    BasicBlock& bb_of_incoming_inst = *incoming_inst->getParent();

    // Case when incoming is outside the loop.
    Loop* loop_associated_with_incoming_inst = _loopInfo.getLoopFor(&bb_of_incoming_inst);
    if(loop_associated_with_incoming_inst == NULL) return true;

    // Case when incoming is in the loop header and bb is not.
    if(
		loop_associated_with_incoming_inst->getHeader() == &bb_of_incoming_inst 
		&& loop_associated_with_incoming_inst == loop_associated_with_bb 
		&& loop_associated_with_bb->getHeader() != &bb
	  )
        return true;

    //LOG_DEBUG() << "bb_of_incoming_inst " << bb_of_incoming_inst.getName()
	//<< " is header: " << (loop_associated_with_incoming_inst->getHeader() == &bb_of_incoming_inst) << "\n";
    //LOG_DEBUG() << "!bb is header: " << (loop_associated_with_bb->getHeader() != &bb) << "\n";
    //LOG_DEBUG() << "loops ==?: "  << (loop_associated_with_incoming_inst ==
	//loop_associated_with_bb) << "\n";
    //LOG_DEBUG() << "incoming was in not (in the loop header of the same loop and not of the block)\n";

    // Case when incoming is not in our loop
    if(loop_associated_with_incoming_inst != loop_associated_with_bb)
        return true;

    return false;
}

/**
 * @copydoc
 */
Timestamp& ConstantWorkOpHandler::getTimestamp(llvm::Value* val, Timestamp& ts)
{
    Instruction& inst = *cast<Instruction>(val);
    BasicBlock& bb_of_inst = *inst.getParent();

    DEBUG(LOG_DEBUG() << "Getting timestamp of " << *val << " operands: \n");

	/*
	 * We are going to check each operand of the instruction to see if it is
	 * live-in to the instruction's region. If it is, we make sure that
	 * operand's timestamp is available before instruction and add a candidate
	 * based off the operand and the work for the instruction. If it's not
	 * live-in, we add candidate timestamps  based off the candidate timestamps
	 * associated with the operand.
	 */
    unsigned int work_of_inst = getWork(&inst);
    for(size_t op_idx = 0; op_idx < inst.getNumOperands(); ++op_idx)
    {
        Value& inst_operand = *inst.getOperand(op_idx);
        DEBUG(LOG_DEBUG() << inst_operand << "\n");
		// TODO: factor out common functionality (see TODO below)
        if(isLiveInRegion(bb_of_inst, inst_operand))
        {
            _timestampPlacer.requireValTimestampBeforeUser(inst_operand, inst);
            ts.addCandidate(&inst_operand, work_of_inst);
        }
        else
        {
            const Timestamp& inst_operand_timestamp = _timestampAnalysis.getTimestamp(&inst_operand);
            foreach(const TimestampCandidate& cand, inst_operand_timestamp)
			{
                ts.addCandidate(cand.getBase(), cand.getOffset() + work_of_inst);
			}
        }
    }

	/*
	 * For the controlling condition of the instruction's basic block, we
	 * follow the same procedure of checking if it's in the region and
	 * creating candidate timestamps accordingly.
	 */
    BasicBlock* controlling_bb = _controlDep.getControllingBlock(&bb_of_inst, false);
    if(controlling_bb)
    {
        Value& controlling_cond = *_controlDep.getControllingCondition(controlling_bb);

		// TODO: factor out common functionality (see TODO above)
        if(isLiveInRegion(bb_of_inst, controlling_cond))
        {
            _timestampPlacer.requireValTimestampBeforeUser(controlling_cond, inst);
            ts.addCandidate(&controlling_cond, work_of_inst);
        }
        else
        {
            const Timestamp& controlling_cond_timestamp = _timestampAnalysis.getTimestamp(&controlling_cond);
            foreach(const TimestampCandidate& cand, controlling_cond_timestamp)
			{
                ts.addCandidate(cand.getBase(), cand.getOffset() + work_of_inst);
			}
        }
    }

    return ts;
}

/**
 * @return The work for an instruction.
 */
unsigned int ConstantWorkOpHandler::getWork(Instruction* inst) const
{
    BinaryOperator* binary_op;
    PHINode* phi;
    if(
        // No constant binary ops.
        (binary_op = dyn_cast<BinaryOperator>(inst))
		&& isa<Constant>(binary_op->getOperand(0))
		&& isa<Constant>(binary_op->getOperand(1))
		
		||
        
        // No induction variables.
        (phi = dyn_cast<PHINode>(inst)) 
		&& _inductionVars.isInductionVariable(*phi)
		
		|| _inductionVars.isInductionIncrement(*inst)
	   )
    {
        return 0;
    }

    switch(inst->getOpcode()) {
        case Instruction::Add:
            return int_add_cost;
        case Instruction::FAdd:
            return fp_add_cost;
        case Instruction::Sub:
            return int_sub_cost;
        case Instruction::FSub:
            return fp_sub_cost;
        case Instruction::Mul:
            return int_mul_cost;
        case Instruction::FMul:
            return fp_mul_cost;
        case Instruction::UDiv:
        case Instruction::SDiv:
        case Instruction::URem:
        case Instruction::SRem:
            return int_div_cost;
        case Instruction::FDiv:
        case Instruction::FRem:
            return fp_div_cost;
        case Instruction::Shl:
        case Instruction::LShr:
        case Instruction::AShr:
        case Instruction::And:
        case Instruction::Or:
        case Instruction::Xor:
            return logic_op_cost;
        case Instruction::ICmp:
            return int_cmp_cost;
        case Instruction::FCmp:
            return fp_cmp_cost;
        case Instruction::Store:
            return mem_store_cost;
        case Instruction::Load:
            return mem_load_cost;
		// FIXME: more accurate cost of atomicrmw based on op
		case Instruction::AtomicRMW:
			return mem_load_cost + mem_store_cost + int_add_cost;
        case Instruction::Alloca:
        case Instruction::BitCast:
        case Instruction::Br:
        case Instruction::Call:
        case Instruction::Invoke:
        case Instruction::LandingPad:
        case Instruction::Resume:
        case Instruction::FPExt:
        case Instruction::FPToSI:
        case Instruction::FPToUI:
        case Instruction::FPTrunc:
        case Instruction::GetElementPtr:
        case Instruction::IntToPtr:
        case Instruction::PHI:
        case Instruction::Switch:
        case Instruction::PtrToInt:
        case Instruction::Ret:
        case Instruction::SExt:
        case Instruction::SIToFP:
        case Instruction::Select:
        case Instruction::Trunc:
        case Instruction::UIToFP:
        case Instruction::Unreachable:
        case Instruction::ZExt:
		// XXX: not sure what the cost of these should be
        case Instruction::ExtractValue:
        case Instruction::InsertValue:
            return 0;
    }
    throw UnknownOpCodeException(inst);
}

/**
 * Parses op costs from a file.
 *
 * This will fill the struct with the values read from the file. The file is
 * expected to have the name of the operation, whitespace, and the cost of the 
 * operation as an integer. Each of these pairs is expected to be separated by 
 * new lines.
 *
 * @param filename The filename to read and parse values from.
 */
void ConstantWorkOpHandler::parseFromFile(const std::string& filename) {
    std::string line;
    std::ifstream opcosts_file;

    opcosts_file.open(filename.c_str());

    // make sure this file is actually open, otherwise print error and abort
    if(!opcosts_file.is_open()) {
        std::cerr << "ERROR: Specified op costs file (" << filename << ") could not be opened.\n";
        assert(0);
    }

    while(!opcosts_file.eof()) {
        getline(opcosts_file,line);

        // skip empty lines
        if(line == "") { continue; }

        std::cerr << "read in op cost line: " << line;

        // tokenize the whitespace separated fields of this line
        std::istringstream iss(line);

        std::vector<std::string> tokens;
        std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter<std::vector<std::string> >(tokens));

        // should be of format "OP_NAME cost"
        assert(tokens.size() == 2 && "Incorect format for op cost file");

        // convert cost to unsigned int
        std::istringstream cost_ss(tokens[1]);
        unsigned int cost;
        cost_ss >> cost;

        std::string cost_name = tokens[0];

        // assign cost to correct field
        if(cost_name == "INT_ADD") { int_add_cost = cost; }
        else if(cost_name == "INT_SUB") { int_sub_cost = cost; }
        else if(cost_name == "INT_MUL") { int_mul_cost = cost; }
        else if(cost_name == "INT_DIV") { int_div_cost = cost; }
        else if(cost_name == "INT_MOD") { int_mod_cost = cost; }
        else if(cost_name == "INT_CMP") { int_cmp_cost = cost; }
        else if(cost_name == "FP_ADD") { fp_add_cost = cost; }
        else if(cost_name == "FP_SUB") { fp_sub_cost = cost; }
        else if(cost_name == "FP_MUL") { fp_mul_cost = cost; }
        else if(cost_name == "FP_DIV") { fp_div_cost = cost; }
        else if(cost_name == "FP_MOD") { fp_mod_cost = cost; }
        else if(cost_name == "FP_CMP") { fp_cmp_cost = cost; }
        else if(cost_name == "LOGIC") { logic_op_cost = cost; }
        else if(cost_name == "MEM_LOAD") { mem_load_cost = cost; }
        else if(cost_name == "MEM_STORE") { mem_store_cost = cost; }
        else {
            std::cerr << "ERROR: unknown op name (" << cost_name << ") in op cost file.\n";
            assert(0);
        }
    }

    opcosts_file.close();
}

template <typename Ostream>
Ostream& operator<<(Ostream& os, const ConstantWorkOpHandler& costs)
{
    throw UnsupportedOperationException();
    return os;
}

/**
 * Prints this struct to the stream.
 *
 * @param os The stream to print to.
 * @param costs The struct to print.
 * @return os.
 */
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ConstantWorkOpHandler& costs)
{
    operator<< <llvm::raw_ostream>(os, costs);

    return os;
}

/**
 * Prints this struct to the stream.
 *
 * @param os The stream to print to.
 * @param costs The struct to print.
 * @return os.
 */
std::ostream& operator<<(std::ostream& os, const ConstantWorkOpHandler& costs)
{
    os << costs;

    return os;
}
