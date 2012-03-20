//#include <string>
//
//static const std::string CPP_THROW_FUNC;
//static const std::string CPP_RETHROW_FUNC;
//static const std::string CPP_EH_EXCEPTION;
//static const std::string CPP_EH_TYPE_ID;
//static const std::string CPP_EH_SELECTOR;
//
//const std::string CriticalPath::CPP_THROW_FUNC = "__cxa_throw";
//const std::string CriticalPath::CPP_RETHROW_FUNC = "";
//const std::string CriticalPath::CPP_EH_EXCEPTION = "llvm.eh.exception";
//const std::string CriticalPath::CPP_EH_TYPE_ID = "llvm.eh.typeid.for";
//const std::string CriticalPath::CPP_EH_SELECTOR = "llvm.eh.selector";
//
//bool isCppThrowFunc(Instruction* i)
//{
//    CallInst* ci;
//    Function* func;
//    return (ci = dyn_cast<CallInst>(i)) && (func = ci->getCalledFunction()) && (
//        func->getName() == CPP_THROW_FUNC ||
//        func->getName() == CPP_RETHROW_FUNC ||
//        func->getName() == CPP_EH_EXCEPTION ||
//        func->getName() == CPP_EH_TYPE_ID); // ||
//        // func->getName() == CPP_EH_SELECTOR);
//}
//    
//bool isThrowCall(CallInst* ci) {
//    Function* func = ci->getCalledFunction();
//    if(func) {
//        return func->getName() == CPP_THROW_FUNC && ci->doesNotReturn();
//    }
//    else {
//        return false;
//    }
//}
//// Returns a set of landing pads for invoke instructions. Blocks that could have been joined together into larger basic blocks are
//// considered the same and the one that dominates the rest is returned.
//std::set<BasicBlock*>& getInvokeLandingPads(Function* func, std::set<BasicBlock*>& result) {
//
//    std::map<BasicBlock*, BasicBlock*> joined_blocks;
//    getJoinedBasicBlocks(func, joined_blocks);
//    /*
//       LOG_DEBUG() << "joined blocks found for getInvokeLandingPads:" << "\n";
//       for(std::map<BasicBlock*, BasicBlock*>::iterator it = joined_blocks.begin(), end = joined_blocks.end(); it != end; it++)
//       LOG_DEBUG() << it->first->getName() << " -> " << it->second->getName() << "\n";
//       */
//
//    foreach(BasicBlock& blk, *func)
//        foreach(Instruction& inst, blk)
//        if(InvokeInst* ii = dyn_cast<InvokeInst>(&inst))
//            result.insert(joined_blocks[ii->getUnwindDest()]);
//    return result;
//}
//
//std::set<BasicBlock*>& getInvokeDestinations(Function* func, std::set<BasicBlock*>& result) {
//
//    std::map<BasicBlock*, BasicBlock*> joined_blocks;
//    getJoinedBasicBlocks(func, joined_blocks);
//
//    foreach(BasicBlock& blk, *func)
//        foreach(Instruction& inst, blk) {
//            if(InvokeInst* ii = dyn_cast<InvokeInst>(&inst))
//                result.insert(joined_blocks[ii->getNormalDest()]);
//        }
//    return result;
//}
//
//            else if((ii = dyn_cast<InvokeInst>(i)))
//            {
//                static int invoke_id = 0;
//                ConstantInt* invoke_id_const = ConstantInt::get(types.i32(),invoke_id++);
//                args.push_back(invoke_id_const);
//                inst_calls_begin.addCallInst(i,"prepareInvoke",args);
//
//                //The exception value is pushed on to the stack, so we are not allowed to call any other function until the
//                //value has been grabbed. As a result, we must push the instrumentation call to be after these calls,
//                //but before any other instrumentation calls. The following gets the first instrumentation call found.
//                //We will insert before it. If we don't find one, the terminator will suffice.
//                {
//
//                    BasicBlock* blk = ii->getUnwindDest();
//
//                    std::vector<Instruction*> joined_blks;
//                    joinBasicBlocks(blk, joined_blks);
//
//                    LOG_DEBUG() << "joined blocks contents of " << blk->getName() << ":" << "\n";
//                    foreach(Instruction* inst, joined_blks)
//                        LOG_DEBUG() << *inst << "\n";
//                    LOG_DEBUG() << "\n";
//
//                    //TODO: Need to handle if we put something in the front. If we do this, ours needs to go into the front instead of the back.
//                    Instruction* insert_before = joined_blks.back();
//                    foreach(Instruction* inst, joined_blks) {
//
//                        if(requiresInstID(inst)) {
//                            insert_before = inst;
//                            break;
//                        }
//                    }
//
//                    LOG_DEBUG() << "inserting invokeThrew before " << PRINT_VALUE(*insert_before) << "\n";
//
//                    inst_calls_end.addCallInstBefore(insert_before, "invokeThrew", args);
//                }
//
//                // insert invokeOkay for this invoke instruction
//                {
//                    BasicBlock* blk = ii->getNormalDest();
//                    Instruction* insert_before = blk->getFirstNonPHI();
//                    inst_calls_begin.addCallInstBefore(insert_before, "invokeOkay", args);
//                }
//
//                args.clear();
//                instrumentCall(ii, inst_to_id, inst_calls_begin, bb_call_idx++);
//            } // end invokeinst
//
