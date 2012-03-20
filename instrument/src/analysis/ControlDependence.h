#ifndef CONTROL_DEPENDENCE_H
#define CONTROL_DEPENDENCE_H

#include <map>
#include <set>
#include <boost/ptr_container/ptr_map.hpp>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/BasicBlock.h>
#include <llvm/Function.h>
#include <llvm/Instructions.h>
#include "TimestampBlockHandler.h"
#include "PassLog.h"

class ControlDependence
{
    public:
    typedef std::set<llvm::BasicBlock*> ControllingBlocks;

    ControlDependence(llvm::Function& func, llvm::DominatorTree& dt, llvm::PostDominanceFrontier& pdf);
    virtual ~ControlDependence() {}

    ControllingBlocks& getPhiControllingBlocks(llvm::PHINode& phi, ControllingBlocks& result);
    llvm::BasicBlock* getControllingBlock(llvm::BasicBlock* blk);
    llvm::BasicBlock* getControllingBlock(llvm::BasicBlock* blk, bool consider_self);
    llvm::Value* getControllingCondition(llvm::BasicBlock* blk);

    private:
    typedef boost::ptr_map<llvm::PHINode*, ControllingBlocks> PhiToControllers;
    typedef std::map<llvm::BasicBlock*, ControllingBlocks> BbToControllers;

    PhiToControllers phi_to_controllers;
    BbToControllers bb_to_controllers;
    llvm::DominatorTree& dt;
    std::map<llvm::BasicBlock*,llvm::BasicBlock*> idom;
    PassLog& log;
    llvm::PostDominanceFrontier& pdf;

    void createIDomMap(llvm::Function& func);
    llvm::BasicBlock* getImmediateDominator(llvm::BasicBlock* blk);
    llvm::BasicBlock* findNearestCommonDominator(const std::set<llvm::BasicBlock*>& blocks);

    ControllingBlocks& getPhiControllingBlocks(llvm::BasicBlock* target, llvm::BasicBlock* limit, llvm::DominatorTree& dt, ControllingBlocks& result);
};

#endif // CONTROL_DEPENDENCE_H
