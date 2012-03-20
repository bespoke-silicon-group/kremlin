#define DEBUG_TYPE __FILE__

#include <llvm/Support/Debug.h>
#include <foreach.h>
#include "Placer.h"

using namespace std;
using namespace boost;
using namespace llvm;

Placer::Node::Node(Instruction& inst) :
    inst(inst),
    log(PassLog::get())
{
}

Placer::Node::~Node()
{
}

void Placer::Node::addUser(Node& user)
{
    users.insert(&user);
    user.dependencies.insert(this);
}

/**
 * Constructs a new placer.
 *
 * @param func The function to place instructions in.
 * @param dt The dominator tree for the function.
 */
Placer::Placer(llvm::Function& func, llvm::DominatorTree& dt) :
    dt(dt),
    func(func),
    log(PassLog::get())
{
}

Placer::~Placer()
{
}

/**
 * Places all instructions that haven't been placed yet at the latest point
 * possible while respecting all dependencies.
 */
void Placer::place()
{
    // The algorithm:
    // We are given a DAG of nodes with dependencies representing edges and
    // users representing back-edges. We also define the number of unplaced
    // users (unplaced_users) to be the number of uses that have not been
    // placed yet.
    //
    // We initialize all the nodes' unplaced_users and identify the set of
    // nodes that have no users and add them to the placeable set.
    //
    // We then continue to loop through the placeable set while it has
    // elements. We pop off some node in the placeable list and iterate
    // through its uses and decrement their unplaced_users since this is about
    // to be placed. If any of these unplaced_users drop to zero, we add them
    // to the placable set.
    //
    // We then attempt to place the instruction. We first identify the basic
    // block that dominates all of the users. This guarantees that our
    // instruction will execute before any of the users. We then look in the
    // basic block for any of the users since basic blocks dominate
    // themselves. We then insert the instruction before the earliest user in
    // the basic block or the terminator if none are found. 
    //
    // Runtime: O(N + E)

    LOG_INFO() << "Placing begins\n";

    // Initialize the nodes and create an initial placable set.
    vector<Node*> placeable;
    foreach(Nodes::value_type p, nodes)
    {
        Node& node = *p->second;
        node.unplaced_users = node.users.size();
        if(!node.unplaced_users)
            placeable.push_back(&node);

        DEBUG(LOG_DEBUG() << "To place: " << node.inst << "\n");

        foreach(Node* user, node.users)
            DEBUG(LOG_DEBUG() << "user: " << user->inst << "\n");

        foreach(Node* dep, node.dependencies)
            DEBUG(LOG_DEBUG() << "dep: " << dep->inst << "\n");
    }

    // Loop until everything is placed.
    while(placeable.size())
    {
        Node& node = *placeable.back();
        placeable.pop_back();

        // Decrement the unplaced_users of the dependencies.
        foreach(Node* dep, node.dependencies)
            if(!--(dep->unplaced_users))
                placeable.push_back(dep);

        DEBUG(LOG_DEBUG() << "Checking if placed: " << node.inst << " parent: " << node.inst.getParent() << "\n");

        // if not placed.
        if(!node.inst.getParent())
        {
            std::set<Instruction*> users;
            foreach(Node* user, node.users)
                users.insert(&user->inst);

            LOG_DEBUG() << "Getting common dom\n";

            // Identify the nearest common dominator.
            assert(users.begin() != users.end());
            BasicBlock* common_dom = (*users.begin())->getParent();
            foreach(Instruction* user, users)
                common_dom = dt.findNearestCommonDominator(common_dom, user->getParent());

            LOG_DEBUG() << "Getting earliest inst\n";

            // Identify the earliest user in the BB or the terminator.
            Instruction* earliest_user = common_dom->getTerminator();
            foreach(Instruction& inst, *common_dom)
                if(users.find(&inst) != users.end())
                {
                    earliest_user = &inst;
                    break;
                }

            DEBUG(LOG_DEBUG() << "Placing " << node.inst << " before " << *earliest_user << "\n");

            node.inst.insertBefore(earliest_user);
        }
    }
    LOG_INFO() << "Placing finishes\n";
}

/**
 * Adds an instruction to place. The added instruction will be placed so that
 * it dominates all of the users.
 *
 * @param call The instruction to insert.
 * @param users The users of the instruction.
 */
void Placer::add(llvm::Instruction& call, const std::set<llvm::Instruction*>& users)
{
    // Make the node.
    Node& node = getOrCreateNode(call);

    // Add the users.
    foreach(Instruction* user, users)
        node.addUser(getOrCreateNode(*user));
}

Placer::Node& Placer::getOrCreateNode(llvm::Instruction& inst)
{
    Instruction* pinst = &inst;
    Nodes::iterator it = nodes.find(&inst);
    if(it == nodes.end())
    {
        DEBUG(LOG_DEBUG() << "Adding node: " << inst << "\n");
        return *nodes.insert(pinst, new Node(inst)).first->second;
    }
    return *it->second;
}
