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
void Placer::placeInsts()
{
    // The algorithm:
    // We are given a DAG of nodes with dependencies representing edges and
    // users representing back-edges. We also define the number of unplaced
    // users (unplaced_users) to be the number of uses that have not been
    // placed yet.
    //
    // We initialize all the nodes' unplaced_users and identify the set of
    // nodes that have no users and add them to the placeable_nodes set.
    //
    // We then continue to loop through the placeable_nodes set while it has
    // elements. We pop off some node in the placeable_nodes list and iterate
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
    vector<Node*> placeable_nodes;
    foreach(Nodes::value_type inst_node_pair, nodes)
    {
        Node& node_to_init = *inst_node_pair->second;
        node_to_init.unplaced_users = node_to_init.users.size();
        if(node_to_init.unplaced_users == 0)
            placeable_nodes.push_back(&node_to_init);

        DEBUG(LOG_DEBUG() << "To place: " << node_to_init.inst << "\n");

        foreach(Node* user, node_to_init.users)
            DEBUG(LOG_DEBUG() << "user: " << user->inst << "\n");

        foreach(Node* dep, node_to_init.dependencies)
            DEBUG(LOG_DEBUG() << "dep: " << dep->inst << "\n");
    }

    // Loop until everything is placed.
    while(!placeable_nodes.empty())
    {
        Node& node_to_be_placed = *placeable_nodes.back();
        placeable_nodes.pop_back();

        // Decrement the unplaced_users of the dependencies.
        foreach(Node* dep, node_to_be_placed.dependencies)
		{
			--dep->unplaced_users;
            if(dep->unplaced_users == 0)
                placeable_nodes.push_back(dep);
		}

        DEBUG(LOG_DEBUG() << "Checking if placed: " << node_to_be_placed.inst
		<< " parent: " << node_to_be_placed.inst.getParent() << "\n");

        // if not placed.
        if(node_to_be_placed.inst.getParent() == NULL)
        {
            std::set<Instruction*> user_insts;
            foreach(Node* user, node_to_be_placed.users)
                user_insts.insert(&user->inst);

            LOG_DEBUG() << "Getting common dom\n";

            // Identify the nearest common dominator.
            assert(user_insts.begin() != user_insts.end());
            BasicBlock* common_dom = (*user_insts.begin())->getParent();
            foreach(Instruction* user, user_insts)
                common_dom = dt.findNearestCommonDominator(common_dom, user->getParent());

            LOG_DEBUG() << "Getting earliest inst\n";

            // Identify the earliest user in the BB or the terminator.
            Instruction* earliest_user_inst = common_dom->getTerminator();
            foreach(Instruction& inst, *common_dom)
			{
                if(user_insts.find(&inst) != user_insts.end())
                {
                    earliest_user_inst = &inst;
                    break;
                }
			}

            DEBUG(LOG_DEBUG() << "Placing " << node_to_be_placed.inst << 
			" before " << *earliest_user_inst << "\n");

            node_to_be_placed.inst.insertBefore(earliest_user_inst);
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
void Placer::addInstForPlacement(llvm::Instruction& call, const std::set<llvm::Instruction*>& users)
{
    // Make the node.
    Node& inst_node = getOrCreateNode(call);

    // Add the users.
    foreach(Instruction* user, users)
        inst_node.addUser(getOrCreateNode(*user));
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
