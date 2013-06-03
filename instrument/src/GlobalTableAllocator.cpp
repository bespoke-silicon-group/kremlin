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
    vector<Type*> args;

    // pointer, bytes allocated
    args += types.pi8(), types.i32();
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
    FunctionType* alloc_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
    alloc_func = cast<Function>(m.getOrInsertFunction("logAlloc", alloc_type));

    // setup free_func
    args.clear();
    args += types.pi8();
	aref = new ArrayRef<Type*>(args);
    FunctionType* free_type = FunctionType::get(types.voidTy(), *aref, false);
	delete aref;
    free_func = cast<Function>(m.getOrInsertFunction("logFree", free_type));
}

void GlobalTableAllocator::addAlloc(llvm::Value& ptr, llvm::Value& size, llvm::Instruction& use)
{
    vector<Value*> args;
    args += &ptr, &size;
	ArrayRef<Value*> *aref = new ArrayRef<Value*>(args);
    CallInst& ci = *CallInst::Create(alloc_func, *aref, "");
	delete aref;
    ts_placer.constrainInstPlacement(ci, use);
}

void GlobalTableAllocator::addFree(llvm::Value& ptr, llvm::Instruction& use)
{
    vector<Value*> args;
    args += &ptr;
	ArrayRef<Value*> *aref = new ArrayRef<Value*>(args);
    CallInst& ci = *CallInst::Create(free_func, *aref, "");
	delete aref;
    ts_placer.constrainInstPlacement(ci, use);
}
