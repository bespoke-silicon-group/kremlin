#include <boost/assign/std/vector.hpp>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include "LLVMTypes.h"
#include "GlobalTableAllocator.h"

using namespace llvm;
using namespace boost;
using namespace boost::assign;
using namespace std;

GlobalTableAllocator::GlobalTableAllocator(TimestampPlacer& ts_placer) :
    ts_placer(ts_placer)
{
    // Setup the alloc_func
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<const Type*> args;

    // pointer, bytes allocated
    args += types.pi8(), types.i32();
    FunctionType* alloc_type = FunctionType::get(types.voidTy(), args, false);
    alloc_func = cast<Function>(m.getOrInsertFunction("logAlloc", alloc_type));

    // setup free_func
    args.clear();
    args += types.pi8();
    FunctionType* free_type = FunctionType::get(types.voidTy(), args, false);
    free_func = cast<Function>(m.getOrInsertFunction("logFree", free_type));
}

void GlobalTableAllocator::addAlloc(llvm::Value& ptr, llvm::Value& size, llvm::Instruction& use)
{
    vector<Value*> args;
    args += &ptr, &size;
    CallInst& ci = *CallInst::Create(alloc_func, args.begin(), args.end(), "");
    ts_placer.add(ci, use);
}

void GlobalTableAllocator::addFree(llvm::Value& ptr, llvm::Instruction& use)
{
    vector<Value*> args;
    args += &ptr;
    CallInst& ci = *CallInst::Create(free_func, args.begin(), args.end(), "");
    ts_placer.add(ci, use);
}
