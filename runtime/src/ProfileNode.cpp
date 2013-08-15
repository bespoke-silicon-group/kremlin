#include <sstream>

#include "ProfileNode.hpp"
#include "ProfileNodeStats.hpp"
#include "MemMapAllocator.h"
#include "debug.h"

static UInt64 lastId = 0; // FIXME: change to member variable?
UInt64 ProfileNode::allocId() { return ++lastId; }

void* ProfileNode::operator new(size_t size) {
	return MemPoolAllocSmall(sizeof(ProfileNode));
}

void ProfileNode::operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(ProfileNode));
}

ProfileNode::ProfileNode(SID static_id, CID callsite_id, RegionType type) : parent(NULL),
	node_type(NORMAL), region_type(type), static_id(static_id), 
	id(ProfileNode::allocId()), callsite_id(callsite_id), recursion(NULL),
	num_instances(0), is_doall(1), curr_stat_index(-1) {

	new(&this->children) std::vector<ProfileNode*, MPoolLib::PoolAllocator<ProfileNode*> >();
	new(&this->stats) std::vector<ProfileNodeStats*, MPoolLib::PoolAllocator<ProfileNodeStats*> >();
}

ProfileNode::~ProfileNode() {
	for(unsigned i = 0; i < children.size(); ++i) {
		delete children[i];
	}
	for(unsigned i = 0; i < stats.size(); ++i) {
		delete stats[i];
	}
	children.clear();
	stats.clear();
	/*
	this->children.~vector<ProfileNode*, MPoolLib::PoolAllocator<ProfileNode*> >();
	this->stats.~vector<ProfileNodeStats*, MPoolLib::PoolAllocator<ProfileNodeStats*> >();
	*/
}

ProfileNode* ProfileNode::getChild(UInt64 static_id, UInt64 callsite_id) {
	for (unsigned i = 0; i < this->children.size(); ++i) {
		ProfileNode* child = this->children[i];
		if ( child->static_id == static_id
			&& (child->getRegionType() != RegionFunc || child->callsite_id == callsite_id)
		   ) {
			return child;
		}
	}

	return NULL;
}

void ProfileNode::addChild(ProfileNode *child) {
	assert(child != NULL);
	// TODO: add pre-condition to make sure child isn't already in list?
	this->children.push_back(child);
	child->parent = this;
	assert(!children.empty());
	assert(child->parent == this);
}

void ProfileNode::addStats(RegionStats *new_stats) {
	assert(new_stats != NULL);

	MSG(DEBUG_CREGION, "CRegionUpdate: callsite_id(0x%lx), work(0x%lx), cp(%lx), spWork(%lx)\n", 
			new_stats->callSite, new_stats->work, new_stats->cp, new_stats->spWork);
	MSG(DEBUG_CREGION, "current region: id(0x%lx), static_id(0x%lx), callsite_id(0x%lx)\n", 
			this->id, this->static_id, this->callsite_id);

	// @TRICKY: if new stats aren't doall, then this node isn't doall
	// (converse isn't true)
	if (new_stats->is_doall == 0) { this->is_doall = 0; }

	this->num_instances++;
	this->updateCurrentStats(new_stats);
	assert(this->num_instances > 0);
}

/*!
 * Update the current ProfileNodeStats with a new set of stats.
 *
 * @param new_stats The new set of stats to use when updating.
 * @pre new_stats is non-NULL
 * @pre The current stat index is non-negative.
 * @pre There is at least one ProfileNodeStats associated with this node.
 * @post There is at least one instance of the current ProfileNodeStats.
 */
void ProfileNode::updateCurrentStats(RegionStats *new_stats) {
	assert(new_stats != NULL);
	assert(this->curr_stat_index >= 0);
	assert(!this->stats.empty());

	MSG(DEBUG_CREGION, "ProfileNodeStatsUpdate: work = %d, spWork = %d\n", new_stats->work, new_stats->spWork);

	ProfileNodeStats *stat = this->stats[this->curr_stat_index];
	stat->num_instances++;
	
	double new_self_par = (double)new_stats->work / (double)new_stats->spWork;
	if (stat->min_self_par > new_self_par) stat->min_self_par = new_self_par;
	if (stat->max_self_par < new_self_par) stat->max_self_par = new_self_par;
	stat->total_work += new_stats->work;
	stat->total_par_per_work += new_stats->cp; // XXX: this seems wrong!
	stat->self_par_per_work += new_stats->spWork;

	stat->num_dynamic_child_regions += new_stats->childCnt;
	if (stat->min_dynamic_child_regions > new_stats->childCnt) 
		stat->min_dynamic_child_regions = new_stats->childCnt;
	if (stat->max_dynamic_child_regions < new_stats->childCnt) 
		stat->max_dynamic_child_regions = new_stats->childCnt;
#ifdef EXTRA_STATS
	stat->readCnt += new_stats->readCnt;
	stat->writeCnt += new_stats->writeCnt;
	stat->loadCnt += new_stats->loadCnt;
	stat->storeCnt += new_stats->storeCnt;
#endif

	assert(stat->num_instances > 0);
}

const char* ProfileNode::toString() {
	char _buf[256]; // FIXME: C++ string?
	const char* _strType[] = {"NORM", "RINIT", "RSINK"};

	UInt64 parentId = (this->parent == NULL) ? 0 : this->parent->id;
	UInt64 childId = (this->children.empty()) ? 0 : this->children[0]->id;
	std::stringstream ss;
	ss << "id: " << this->id << "node_type: " << _strType[this->node_type] 
		<< ", parent: " << parentId << ", firstChild: " << childId 
		<< ", static_id: " << this->static_id;
	return ss.str().c_str();
}

void ProfileNode::moveToNextStats() {
	assert(!this->stats.empty() || this->curr_stat_index == -1);
	int stat_index = ++(this->curr_stat_index);

	MSG(DEBUG_CREGION, "ProfileNodeStatsForward id %d to page %d\n", this->id, stat_index);

	if (stat_index >= this->stats.size()) {
		ProfileNodeStats *new_stat = new ProfileNodeStats(); // FIXME: memory leak
		this->stats.push_back(new_stat);
	}

	assert(this->curr_stat_index >= 0);
}

void ProfileNode::moveToPrevStats() {
	assert(this->curr_stat_index >= 0);
	MSG(DEBUG_CREGION, "ProfileNodeStatsBackward id %d from page %d\n", this->id, this->curr_stat_index);
	--(this->curr_stat_index);
}

ProfileNode* ProfileNode::getAncestorWithSameStaticID() {
	assert(this->parent != NULL);

	MSG(DEBUG_CREGION, "findAncestor: static_id: 0x%llx....", this->static_id);

	ProfileNode *ancestor = this->parent;
	
	while (ancestor != NULL) {
		if (ancestor->static_id == this->static_id) {
			assert(ancestor->parent != NULL);
			return ancestor;
		}

		ancestor = ancestor->parent;
	}
	return NULL;
}


void ProfileNode::handleRecursion() {
	/*
	 * We will detect recursion by looking for an ancestor with the same
	 * static ID. If no such ancestor exists, the current node isn't a
	 * recursive call. If we find such an ancestor, we: change the ancestor's
	 * node_type to R_INIT, set the this node's node_type to R_SINK, and set this 
	 * node's recursion field to point to the ancestral node.
	 */

	ProfileNode *ancestor = getAncestorWithSameStaticID();

	if (ancestor == NULL) {
		return;
	}
	else {
		ancestor->node_type = R_INIT;
		this->node_type = R_SINK;
		this->recursion = ancestor;
		return;
	}
}
