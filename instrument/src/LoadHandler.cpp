#include <boost/lexical_cast.hpp>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include "analysis/InductionVariables.h"
#include "LoadHandler.h"
#include "LLVMTypes.h"
#include "MemoryInstHelper.h"

#define MAX_SPECIALIZED 5

using namespace llvm;
using namespace boost;
using namespace std;

/**
 * Constructs a new load handler.
 *
 * @param ts_placer The instruction placer this handler is associated with.
 */
LoadHandler::LoadHandler(TimestampPlacer& ts_placer) :
    induc_vars(ts_placer.getAnalyses().li),
    ts_placer(ts_placer)
{
    opcodes.push_back(Instruction::Load);

    // Setup the ret_const_func
    Module& m = *ts_placer.getFunc().getParent();
    LLVMTypes types(m.getContext());
    vector<Type*> args;

    args.push_back(types.pi8());
    args.push_back(types.i32());
    args.push_back(types.i32());
    args.push_back(types.i32());
	ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
    FunctionType* func_type = FunctionType::get(types.voidTy(), *aref, true);
	delete aref;
    log_func = cast<Function>(m.getOrInsertFunction("_KLoad", func_type));

    // Make specialized functions.
    args.clear();
    args.push_back(types.pi8());
    args.push_back(types.i32());
    args.push_back(types.i32());
    for(int i = 0; i < MAX_SPECIALIZED; i++)
    {
		ArrayRef<Type*> *aref = new ArrayRef<Type*>(args);
        FunctionType* func_type = FunctionType::get(types.voidTy(), *aref, false);
		delete aref;
        specialized_funcs.insert(make_pair(i, 
                cast<Function>(m.getOrInsertFunction(
                        "_KLoad" + lexical_cast<string>(i), 
                        func_type))));

        args.push_back(types.i32());
    }
}

const TimestampPlacerHandler::Opcodes& LoadHandler::getOpcodes()
{
    return opcodes;
}

void LoadHandler::handle(llvm::Instruction& inst)
{
    LoadInst& load = *cast<LoadInst>(&inst);
    LLVMTypes types(load.getContext());
    vector<Value*> args;

#if 0
    // Function that pushes an llvm int into args.
    function<void(unsigned int)> push_int = bind(&vector<Value*>::push_back,
        ref(args), bind<Constant*>(&ConstantInt::get, types.i32(), _1, false));
#endif

    // the mem loc is already in ptr form so we simply use that
    CastInst& ptr_cast = *CastInst::CreatePointerCast(load.getPointerOperand(),types.pi8(),"inst_arg_ptr");
    args.push_back(&ptr_cast);

    args.push_back(ConstantInt::get(types.i32(), ts_placer.getId(load), false)); // Dest ID
#if 0
    push_int(ts_placer.getId(load)); // Dest ID
#endif 

    // If this load uses a getelementptr inst for its address, we need
    // to check for any non-constant ops (other than the pointer index)
    // that are in the gep and add them as dependencies for the load
    GetElementPtrInst* gepi = dyn_cast<GetElementPtrInst>(load.getPointerOperand());

    size_t num_conds = 0;
    size_t num_conds_idx = args.size() + 1; // Placeholder for num_conds
    if(gepi)
    {
        for(User::op_iterator gepi_op = gepi->idx_begin(), gepi_ops_end = gepi->idx_end(); 
            gepi_op != gepi_ops_end; 
            gepi_op++) 
        {
            // We are only looking for non-consts that aren't induction variables.
            // We avoid induction variables because they add unnecessary dependencies
            // (i.e. we don't care that i is a dep for a[i])
            PHINode* gepi_op_phi = dyn_cast<PHINode>(gepi_op);
            Instruction* gepi_op_inst = dyn_cast<Instruction>(gepi_op);

            if(!isa<ConstantInt>(gepi_op) &&
                !(gepi_op_phi && induc_vars.isInductionVariable(*gepi_op_phi)) &&
                !(gepi_op_inst && induc_vars.isInductionIncrement(*gepi_op_inst))) 
            {
                //LOG_DEBUG() << "getelementptr " << gepi->getName() << " depends on " << gepi_op->get()->getName() << "\n";

#if 0
                push_int(ts_placer.getId(*gepi_op->get()));
#endif
    		args.push_back(ConstantInt::get(types.i32(), ts_placer.getId(*gepi_op->get()), false));
                num_conds++;
            }
        }
    }

    	args.push_back(ConstantInt::get(types.i32(), MemoryInstHelper::getTypeSizeInBytes(&load), false));
#if 0
	push_int(MemoryInstHelper::getTypeSizeInBytes(&load));
#endif

	// try to find a specialized version
    Function* log_func = this->log_func;
    SpecializedFuncs::iterator it = specialized_funcs.find(num_conds);
    if(it != specialized_funcs.end())
        log_func = it->second;

    // Insert num conditions
    else
        args.insert(args.begin() + num_conds_idx, ConstantInt::get(types.i32(), num_conds, false));

	ArrayRef<Value*> *aref = new ArrayRef<Value*>(args);
    CallInst& ci = *CallInst::Create(log_func, *aref, "");
	delete aref;
    ts_placer.constrainInstPlacement(ci, load);
    ts_placer.constrainInstPlacement(ptr_cast, ci);
}

