#include "InstrumentationFuncManager.h"
#include <llvm/Instructions.h>
#include <llvm/Instruction.h>
#include <llvm/Constants.h>
#include <llvm/GlobalVariable.h>
#include <llvm/User.h>
#include <llvm/DerivedTypes.h>
#include <boost/bind.hpp>
#include <iostream>

#include "LLVMTypes.h"

using namespace llvm;

InstrumentationFuncManager::InstrumentationFuncManager(Module& module) : log(PassLog::get()), module(module)
{
}

InstrumentationFuncManager::~InstrumentationFuncManager()
{
}

void InstrumentationFuncManager::addFunc(const std::string& name, FunctionType* type)
{
	instrumentation_funcs[name] = cast<Function>(module.getOrInsertFunction(name, type));
}

const Type* InstrumentationFuncManager::getArgTypeFromFirstUser(Function* func, unsigned arg_no) {
	LOG_INFO() << "Grabbing arg number " << arg_no << " from first user of " << func->getName() << "\n";

	User* first_user = *(func->use_begin());

	assert(isa<CallInst>(first_user) && "currently don't support finding type in non call inst user");
	CallInst* ci = cast<CallInst>(first_user);

	assert(arg_no < ci->getNumArgOperands() && "first user does not have that many arguments");

	const Type* arg_type = ci->getArgOperand(arg_no)->getType();
	LOG_INFO() << "Argument type is: " << *arg_type << "\n";

	return arg_type;
}

void InstrumentationFuncManager::initializeDefaultValues()
{
	std::vector<const Type*> args;
	LLVMTypes types(module.getContext());

	FunctionType* no_args = FunctionType::get(types.voidTy(), args, false);

	// funcs with no args
	addFunc("_KPopCDep", no_args);
	addFunc("_KLinkArgConst", no_args);
	addFunc("setupUnwindPoint", no_args);
	addFunc("_KReturnConst", no_args);

	// funcs with 1 unsigned int arg
	args.push_back(types.i32()); 
	FunctionType* uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KAssignConst", uint);
	addFunc("_KInsertValConst", uint);
	addFunc("_KPushCDep", uint);
	addFunc("_KLinkReturn", uint);
	addFunc("_KReturn", uint);
	addFunc("_KLinkArg", uint);
	addFunc("_KUnlinkArg", uint);
	addFunc("_KInduction", uint);

	addFunc("_KInductionDep", uint);


	addFunc("_KPrepInvoke", uint);
	addFunc("_KInvokeThrew", uint);
	addFunc("_KInvokeOkay", uint);

	// funcs with 2 unsigned int args
	args.push_back(types.i32()); 
	FunctionType* uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KAssign", uint_uint);
	addFunc("_KInsertVal", uint_uint);
	addFunc("_KPhiAddCondition", uint_uint);
	addFunc("_KReduction", uint_uint);
	addFunc("_KPrepRTable", uint_uint);

	// funcs with 3 unsigned int args
	args.push_back(types.i32()); 
	FunctionType* uint_uint_uint = FunctionType::get(types.voidTy(), args, false);
	FunctionType* uint_uint_uint_varg = FunctionType::get(types.voidTy(), args, true);

	addFunc("_KBinaryConst", uint_uint_uint);
	addFunc("_KPhi1To1", uint_uint_uint);
	addFunc("_KCallLib", uint_uint_uint_varg);

	// funcs with 4 unsigned int args
	args.push_back(types.i32()); 
	FunctionType* uint_uint_uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KBinary", uint_uint_uint_uint);
	addFunc("_KPhi2To1", uint_uint_uint_uint);

	// funcs with 5 unsigned int args
	args.push_back(types.i32()); 
	FunctionType* uint_uint_uint_uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KPhi3To1", uint_uint_uint_uint_uint);
	addFunc("_KPhiCond4To1", uint_uint_uint_uint_uint);

	// funcs with 6 unsigned int args
	args.push_back(types.i32()); 
	FunctionType* uint_uint_uint_uint_uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KPhi4To1", uint_uint_uint_uint_uint_uint);

	args.clear();

    // functions taking 64-bit ints
	args.push_back(types.i64()); 
	args.push_back(types.i64()); 
	FunctionType* uint64_uint64 = FunctionType::get(types.voidTy(), args, false);
	addFunc("_KPrepCall", uint64_uint64);
    args.clear();

	// funcs with args other than just Int32Ty
	args.push_back(types.i32()); 
	args.push_back(types.pi8()); // void*
	args.push_back(types.i32());
	FunctionType* uint_pvoid = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KStore", uint_pvoid);

	args.clear();
	args.push_back(types.pi8()); // void*
	args.push_back(types.i32());
	FunctionType* pvoid = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KStoreConst", pvoid);
	args.clear();

	args.push_back(types.pi8()); // void*
	FunctionType* pvoidclean = FunctionType::get(types.voidTy(), args, false);

	// Look before declaration of free before adding logFree (if it doesn't exist, we shouldn't bother with it)
	if(Function* f = module.getFunction("free")) 
	{
		LOG_DEBUG() << "adding free because we located this function: " << *f;
		addFunc("_KFree", pvoidclean);
	}

	args.push_back(types.i32()); 

	args.push_back(types.i32()); 
	FunctionType* pvoid_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KLoad", pvoid_uint);

	args.push_back(types.i32());
	FunctionType* pvoid_uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KLoad1", pvoid_uint_uint);

	args.push_back(types.i32());
	FunctionType* pvoid_uint_uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KLoad2", pvoid_uint_uint_uint);

	args.push_back(types.i32());
	FunctionType* pvoid_uint_uint_uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KLoad3", pvoid_uint_uint_uint_uint);

	args.push_back(types.i32());
	FunctionType* pvoid_uint_uint_uint_uint_uint = FunctionType::get(types.voidTy(), args, false);

	addFunc("_KLoad4", pvoid_uint_uint_uint_uint_uint);

	args.clear();

	// Look for definition of malloc to find out if it takes 32 bit int or 64 bit.
	// If there isn't one in this module, then there won't be any calls to malloc so we won't bother adding logMalloc to inst_funcs

	Function* malloc_func;
	if((malloc_func = module.getFunction("malloc")) || (malloc_func = module.getFunction("calloc")))
	{

		LOG_DEBUG() << "adding malloc/calloc func because we found: " << *malloc_func;

		args.push_back(types.pi8()); // void*

		// need to make sure malloc is actually defined. It's surprisingly common for people to not include stdlib.h when using malloc
		const FunctionType* malloc_ft = malloc_func->getFunctionType();
		if(malloc_ft->getNumParams() == 0) {
			LOG_WARN() << "malloc not fully defined. Did you forget to include stdlib.h?\n";

			// let's be safe and find the type used by first user (should be same type for all users)
			const Type* size_arg_type = getArgTypeFromFirstUser(malloc_func,0);

			args.push_back(size_arg_type);
		}
		else {
			args.push_back(malloc_ft->getParamType(0)); // size is first and only param for malloc/calloc
		}

		args.push_back(types.i32()); // destination reg for malloc/calloc

		FunctionType* pvoid_sizet = FunctionType::get(types.voidTy(), args, false);

		addFunc("_KMalloc", pvoid_sizet);

		args.clear();
	}

	// Again, we look for realloc's declaration to see if it's 32 or 64 bit
	if(Function* realloc_func = module.getFunction("realloc")) 
	{
		LOG_DEBUG() << "adding realloc because we found: " << *realloc_func;

		args.push_back(types.pi8()); // void*
		args.push_back(types.pi8()); // void*

		// see above note about people forgetting to include stdlib.h when using malloc/realloc
		const FunctionType* realloc_ft = realloc_func->getFunctionType();
		if(realloc_ft->getNumParams() == 0) {
			LOG_WARN() << "realloc not fully defined. Did you forget to include stdlib.h?\n";

			// let's be safe and find the type used by first user (should be same type for all users)
			const Type* size_arg_type = getArgTypeFromFirstUser(realloc_func,1);

			args.push_back(size_arg_type);
		}
		else {
			args.push_back(realloc_ft->getParamType(1)); // size will be 2nd param for realloc
		}

		args.push_back(types.i32()); // destination reg for malloc/calloc

		FunctionType* pvoid_pvoid_sizet = FunctionType::get(types.voidTy(), args, false);

		addFunc("_KRealloc", pvoid_pvoid_sizet);
	}
}

Function* InstrumentationFuncManager::get(const std::string& name)
{
	return instrumentation_funcs[name];
}

