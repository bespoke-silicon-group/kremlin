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
	int_add(INT_ADD),
	int_sub(INT_SUB),
	int_mul(INT_MUL),
	int_div(INT_DIV),
	int_mod(INT_MOD),
	int_cmp(INT_CMP),
	fp_add(FP_ADD),
	fp_sub(FP_SUB),
	fp_mul(FP_MUL),
	fp_div(FP_DIV),
	fp_mod(FP_MOD),
	fp_cmp(FP_CMP),
	logic(LOGIC),
    mem_load(LOAD),
    mem_store(STORE),
    cd(ts_placer.getAnalyses().cd),
    induc_vars(induc_vars),
    li(ts_placer.getAnalyses().li),
    timestampAnalysis(timestampAnalysis),
    ts_placer(ts_placer)
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
    Loop* loop = li.getLoopFor(&bb);

    // Can't come before a loop that doesn't exist.
    if(!loop)
        return false;

    //LOG_DEBUG() << "bb is a loop\n";

    Instruction* incoming_inst = dyn_cast<Instruction>(&incoming_val);
    if(!incoming_inst)
        return false;

    //LOG_DEBUG() << "val is an inst\n";

    BasicBlock& incoming_bb = *incoming_inst->getParent();

    // Case when incoming is outside the loop.
    Loop* incoming_inst_loop = li.getLoopFor(&incoming_bb);
    if(!incoming_inst_loop)
        return true;

    //LOG_DEBUG() << "incoming val is in a loop\n";

    // Case when incoming is in the loop header and bb is not.
    if(incoming_inst_loop->getHeader() == &incoming_bb && incoming_inst_loop == loop && loop->getHeader() != &bb)
        return true;

    //LOG_DEBUG() << "incoming_bb " << incoming_bb.getName() << " is header: " << (incoming_inst_loop->getHeader() == &incoming_bb) << "\n";
    //LOG_DEBUG() << "!bb is header: " << (loop->getHeader() != &bb) << "\n";
    //LOG_DEBUG() << "loops ==?: "  << (incoming_inst_loop == loop) << "\n";
    //LOG_DEBUG() << "incoming was in not (in the loop header of the same loop and not of the block)\n";

    // Case when incoming is not in our loop
    if(incoming_inst_loop != loop)
        return true;

    //LOG_DEBUG() << "incoming was in not in our loop\n";

    return false;
}

/**
 * @copydoc
 */
Timestamp& ConstantWorkOpHandler::getTimestamp(llvm::Value* val, Timestamp& ts)
{
    Instruction& inst = *cast<Instruction>(val);
    BasicBlock& bb = *inst.getParent();
    DEBUG(LOG_DEBUG() << "Getting timestamp of " << *val << " operands: \n");
    unsigned int work = getWork(&inst);
    for(size_t i = 0; i < inst.getNumOperands(); i++)
    {
        Value& operand = *inst.getOperand(i);
        DEBUG(LOG_DEBUG() << operand << "\n");
        if(isLiveInRegion(bb, operand))
        {
            ts_placer.requestTimestamp(operand, inst);
            ts.insert(&operand, work);
        }
        else
        {
            const Timestamp& op_ts = timestampAnalysis.getTimestamp(&operand);
            foreach(const TimestampCandidate& cand, op_ts)
                ts.insert(cand.getBase(), cand.getOffset() + work);
        }
    }
    BasicBlock* controller = cd.getControllingBlock(inst.getParent(), false);
    if(controller)
    {
        Value& ctrl_value = *cd.getControllingCondition(controller);

        if(isLiveInRegion(bb, ctrl_value))
        {
            ts_placer.requestTimestamp(ctrl_value, inst);
            ts.insert(&ctrl_value, work);
        }
        else
        {
            const Timestamp& ctrl_ts = timestampAnalysis.getTimestamp(&ctrl_value);
            foreach(const TimestampCandidate& cand, ctrl_ts)
                ts.insert(cand.getBase(), cand.getOffset() + work);
        }
    }

    return ts;
}

/**
 * @return The work for an instruction.
 */
unsigned int ConstantWorkOpHandler::getWork(Instruction* inst) const
{
    BinaryOperator* bo;
    PHINode* phi;
    if(
        // No constant binary ops.
        (bo = dyn_cast<BinaryOperator>(inst)) && 
        isa<Constant>(bo->getOperand(0)) &&
        isa<Constant>(bo->getOperand(1)) ||
        
        // No induction variables.
        (phi = dyn_cast<PHINode>(inst)) && induc_vars.isInductionVariable(*phi) || induc_vars.isInductionIncrement(*inst))
    {
        return 0;
    }

    switch(inst->getOpcode()) {
        case Instruction::Add:
            return int_add;
        case Instruction::FAdd:
            return fp_add;
        case Instruction::Sub:
            return int_sub;
        case Instruction::FSub:
            return fp_sub;
        case Instruction::Mul:
            return int_mul;
        case Instruction::FMul:
            return fp_mul;
        case Instruction::UDiv:
        case Instruction::SDiv:
        case Instruction::URem:
        case Instruction::SRem:
            return int_div;
        case Instruction::FDiv:
        case Instruction::FRem:
            return fp_div;
        case Instruction::Shl:
        case Instruction::LShr:
        case Instruction::AShr:
        case Instruction::And:
        case Instruction::Or:
        case Instruction::Xor:
            return logic;
        case Instruction::ICmp:
            return int_cmp;
        case Instruction::FCmp:
            return fp_cmp;
        case Instruction::Store:
            return mem_store;
        case Instruction::Load:
            return mem_load;
        case Instruction::Alloca:
        case Instruction::BitCast:
        case Instruction::Br:
        case Instruction::Call:
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
        if(cost_name == "INT_ADD") { int_add = cost; }
        else if(cost_name == "INT_SUB") { int_sub = cost; }
        else if(cost_name == "INT_MUL") { int_mul = cost; }
        else if(cost_name == "INT_DIV") { int_div = cost; }
        else if(cost_name == "INT_MOD") { int_mod = cost; }
        else if(cost_name == "INT_CMP") { int_cmp = cost; }
        else if(cost_name == "FP_ADD") { fp_add = cost; }
        else if(cost_name == "FP_SUB") { fp_sub = cost; }
        else if(cost_name == "FP_MUL") { fp_mul = cost; }
        else if(cost_name == "FP_DIV") { fp_div = cost; }
        else if(cost_name == "FP_MOD") { fp_mod = cost; }
        else if(cost_name == "FP_CMP") { fp_cmp = cost; }
        else if(cost_name == "LOGIC") { logic = cost; }
        else if(cost_name == "MEM_LOAD") { mem_load = cost; }
        else if(cost_name == "MEM_STORE") { mem_store = cost; }
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
//    os << "op costs:" << "\n";
//    os << "\tINT_ADD: " << costs.int_add << "\n";
//    os << "\tINT_SUB: " << costs.int_sub << "\n";
//    os << "\tINT_MUL: " << costs.int_mul << "\n";
//    os << "\tINT_DIV: " << costs.int_div << "\n";
//    os << "\tINT_MOD: " << costs.int_mod << "\n";
//    os << "\tINT_CMP: " << costs.int_cmp << "\n";
//    os << "\tFP_ADD: " << costs.fp_add << "\n";
//    os << "\tFP_SUB: " << costs.fp_sub << "\n";
//    os << "\tFP_MUL: " << costs.fp_mul << "\n";
//    os << "\tFP_DIV: " << costs.fp_div << "\n";
//    os << "\tFP_MOD: " << costs.fp_mod << "\n";
//    os << "\tFP_CMP: " << costs.fp_cmp << "\n";
//    os << "\tLOGIC: " << costs.logic << "\n";

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
    operator<< <std::ostream>(os, costs);

    return os;
}
