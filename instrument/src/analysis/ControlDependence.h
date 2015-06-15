#ifndef CONTROL_DEPENDENCE_H
#define CONTROL_DEPENDENCE_H

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "PostDominanceFrontier.h"

#include <boost/ptr_container/ptr_map.hpp>
#include <map>
#include <set>

#include "PassLog.h"
#include "TimestampBlockHandler.h"

class ControlDependence
{
    public:
    typedef std::set<llvm::BasicBlock*> ControllingBlocks;

    ControlDependence(llvm::Function& func, llvm::DominatorTree& dt, PostDominanceFrontier& pdf);
    virtual ~ControlDependence() {}

    ControllingBlocks& getPhiControllingBlocks(llvm::PHINode& phi, ControllingBlocks& result);
    llvm::BasicBlock* getControllingBlock(llvm::BasicBlock* blk);
    llvm::BasicBlock* getControllingBlock(llvm::BasicBlock* blk, bool consider_self);
    llvm::Value* getControllingCondition(llvm::BasicBlock* blk);

    private:
    typedef boost::ptr_map<llvm::PHINode*, ControllingBlocks> PhiToControllers;
    typedef std::map<llvm::BasicBlock*, ControllingBlocks> BbToControllers;

    PhiToControllers phi_to_controllers;
    llvm::DominatorTree& dt;
    std::map<llvm::BasicBlock*,llvm::BasicBlock*> idom;
    PassLog& log;
    PostDominanceFrontier& pdf;

    void createIDomMap(llvm::Function& func);
    llvm::BasicBlock* getImmediateDominator(llvm::BasicBlock* blk);
    llvm::BasicBlock* findNearestCommonDominator(const std::set<llvm::BasicBlock*>& blocks);

    ControllingBlocks& getPhiControllingBlocks(llvm::BasicBlock* target, llvm::BasicBlock* limit, llvm::DominatorTree& dt, ControllingBlocks& result);
};

#endif // CONTROL_DEPENDENCE_H
