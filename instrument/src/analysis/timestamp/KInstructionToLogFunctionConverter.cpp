#define DEBUG_TYPE __FILE__
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <llvm/IR/Constants.h>
#include <llvm/Support/Debug.h>

#include "analysis/timestamp/KInstructionToLogFunctionConverter.h"
#include "analysis/timestamp/TimestampCandidate.h"
#include "LLVMTypes.h"
#include "foreach.h"

#define NUM_SPECIALIZED 8

using namespace llvm;
using namespace boost;
using namespace std;

InstructionToLogFunctionConverter::InstructionToLogFunctionConverter(llvm::Module& m, InstIds& inst_to_id) :
    inst_to_id(inst_to_id),
    log(PassLog::get()),
    log_func(NULL),
    m(m)
{
    std::vector<Type*> args;
    LLVMTypes types(m.getContext());

    args.push_back(types.i32()); // dest virtual reg num
    args.push_back(types.i32()); // num operands
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
    FunctionType* log_func_type = FunctionType::get(types.voidTy(), *aref, true);

    // if the cast fails, another func with the same name and different prototype exists.
    log_func = cast<Function>(m.getOrInsertFunction("_KTimestamp", log_func_type)); 

    // Custom functions
    args.pop_back();
    for(size_t i = 0; i < NUM_SPECIALIZED; i++)
    {
        FunctionType* func_type = FunctionType::get(types.voidTy(), args, false);
        func_map.insert(make_pair(i, cast<Function>(m.getOrInsertFunction("_KTimestamp" + lexical_cast<string>(i), func_type))));

        args.push_back(types.i32()); // src reg num
        args.push_back(types.i32()); // src timestamp
    }
}

InstructionToLogFunctionConverter::~InstructionToLogFunctionConverter()
{
}

llvm::CallInst* InstructionToLogFunctionConverter::operator()(const Value* inst, const Timestamp& ts) const
{
    LOG_DEBUG() << "Converting inst: " << *inst << "\n";

    std::vector<Value*> args;
    LLVMTypes types(m.getContext());
#if 0
    function<void(unsigned int)> push_int = bind(&vector<Value*>::push_back, 
        ref(args), bind<Constant*>(&ConstantInt::get, types.i32(), _1, false));
#endif

    args.push_back(ConstantInt::get(types.i32(), inst_to_id.getId(*inst), false)); // dest id.
#if 0
    push_int(inst_to_id.getId(*inst)); // dest id.
#endif

    // Look up the custom function
    FuncMap::const_iterator it = func_map.find(ts.size());
    Function* func;
    if(it == func_map.end())
    {
        func = log_func;
        args.push_back(ConstantInt::get(types.i32(), ts.size(), false)); // num args.
#if 0
        push_int(ts.size()); // num args.
#endif
    }
    else
        func = it->second;

    foreach(const TimestampCandidate& cand, ts)
    {
        args.push_back(ConstantInt::get(types.i32(), inst_to_id.getId(*cand.getBase()), false)); // vtable index
        args.push_back(ConstantInt::get(types.i32(), cand.getOffset(), false)); // constant work
#if 0
        push_int(inst_to_id.getId(*cand.getBase())); // vtable index
        push_int(cand.getOffset()); // constant work
#endif
    }

	ArrayRef<Value*> *aref = new ArrayRef<Value*>(args);
    CallInst* c = CallInst::Create(func, *aref, "");
	delete aref;
	return c;
}
