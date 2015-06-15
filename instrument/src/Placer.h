#ifndef PLACER_H
#define PLACER_H

#include <boost/ptr_container/ptr_map.hpp>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <set>
#include "PassLog.h"

/**
 * Places instrumentation functions as late as possible.
 */
class Placer
{
    public:
    Placer(llvm::Function& func, llvm::DominatorTree& dt);
    virtual ~Placer();

    void placeInsts();
    void addInstForPlacement(llvm::Instruction& inst, const std::set<llvm::Instruction*>& users);

    private:
    struct Node
    {
        Node(llvm::Instruction& inst);
        virtual ~Node();

        void addUser(Node& user);

        llvm::Instruction& inst;
        std::set<Node*> users;
        std::set<Node*> dependencies;
        unsigned int unplaced_users;
        PassLog& log;
    };

    typedef boost::ptr_map<llvm::Instruction*, Node> Nodes;

    Node& getOrCreateNode(llvm::Instruction& inst);

    llvm::DominatorTree& dt;
    llvm::Function& func;
    PassLog& log;
    Nodes nodes;
};

#endif // PLACER_H
