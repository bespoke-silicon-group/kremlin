//=- llvm/Analysis/PostDominanceFrontier.h - Post Dominance Frontier Calculation-*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file exposes interfaces to post dominance information.
//
//===----------------------------------------------------------------------===//

#ifndef POST_DOMINANCE_FRONTIER_H
#define POST_DOMINANCE_FRONTIER_H

#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DominanceFrontier.h"

using namespace llvm;

class PostDominanceFrontier : public DominanceFrontierBase<BasicBlock>, public FunctionPass {
public:
	static char ID;

	//PostDominanceFrontier() : DominanceFrontierBase<BasicBlock>(true), FunctionPass(ID) {}
	PostDominanceFrontier() : FunctionPass(ID), DominanceFrontierBase<BasicBlock>(true) {}

	virtual bool runOnFunction(Function &F) {
		Frontiers.clear();
		PostDominatorTree &DT = getAnalysis<PostDominatorTree>();
		Roots = DT.getRoots();
		if (const DomTreeNode *Root = DT.getRootNode())
			calculate(DT, Root);
		return false;
	}

	const DomSetType &calculate(const PostDominatorTree &DT, const DomTreeNode *Node);

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<PostDominatorTree>();
	}
};

#endif
