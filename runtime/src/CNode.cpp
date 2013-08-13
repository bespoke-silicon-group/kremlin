#include <sstream>

#include "CNode.h"
#include "CStat.h"
#include "MemMapAllocator.h"
#include "debug.h"

static UInt64 lastId = 0; // FIXME: change to member variable?
UInt64 CNode::allocId() { return ++lastId; }

void* CNode::operator new(size_t size) {
	return (CNode*)MemPoolAllocSmall(sizeof(CNode));
}

void CNode::operator delete(void* ptr) {
	MemPoolFreeSmall(ptr, sizeof(CNode));
}

CNode::CNode(SID static_id, CID callsite_id, RegionType type) : parent(NULL),
	node_type(NORMAL), region_type(type), static_id(static_id), 
	id(CNode::allocId()), callsite_id(callsite_id), recursion(NULL),
	num_instances(0), is_doall(1), curr_stat_index(-1) {

	new(&this->children) std::vector<CNode*, MPoolLib::PoolAllocator<CNode*> >();
	new(&this->stats) std::vector<CStat*, MPoolLib::PoolAllocator<CStat*> >();
}

CNode::~CNode() {
	this->children.~vector<CNode*, MPoolLib::PoolAllocator<CNode*> >();
	this->stats.~vector<CStat*, MPoolLib::PoolAllocator<CStat*> >();
}

CNode* CNode::getChild(UInt64 static_id, UInt64 callsite_id) {
	for (unsigned i = 0; i < this->children.size(); ++i) {
		CNode* child = this->children[i];
		if ( child->static_id == static_id
			&& (child->getRegionType() != RegionFunc || child->callsite_id == callsite_id)
		   ) {
			return child;
		}
	}

	return NULL;
}

void CNode::addChild(CNode *child) {
	assert(child != NULL);
	// TODO: add pre-condition to make sure child isn't already in list?
	this->children.push_back(child);
	child->parent = this;
	assert(!children.empty());
	assert(child->parent == this);
}

void CNode::addStats(RegionStats *new_stats) {
	assert(new_stats != NULL);

	MSG(DEBUG_CREGION, "CRegionUpdate: callsite_id(0x%lx), work(0x%lx), cp(%lx), spWork(%lx)\n", 
			new_stats->callSite, new_stats->work, new_stats->cp, new_stats->spWork);
	MSG(DEBUG_CREGION, "current region: id(0x%lx), static_id(0x%lx), callsite_id(0x%lx)\n", 
			this->id, this->static_id, this->callsite_id);

	// @TRICKY: if new stats aren't doall, then this node isn't doall
	// (converse isn't true)
	if (new_stats->is_doall == 0) { this->is_doall = 0; }

	this->num_instances++;
	this->updateCurrentCStat(new_stats);
	assert(this->num_instances > 0);
}

/*!
 * Update the current CStat with a new set of stats.
 *
 * @param new_stats The new set of stats to use when updating.
 * @pre new_stats is non-NULL
 * @pre The current stat index is non-negative.
 * @pre There is at least one CStat associated with this node.
 * @post There is at least one instance of the current CStat.
 */
void CNode::updateCurrentCStat(RegionStats *new_stats) {
	assert(new_stats != NULL);
	assert(this->curr_stat_index >= 0);
	assert(!this->stats.empty());

	MSG(DEBUG_CREGION, "CStatUpdate: work = %d, spWork = %d\n", new_stats->work, new_stats->spWork);

	CStat *stat = this->stats[this->curr_stat_index];
	stat->num_instances++;
	
	double new_self_par = (double)new_stats->work / (double)new_stats->spWork;
	if (stat->minSP > new_self_par) stat->minSP = new_self_par;
	if (stat->maxSP < new_self_par) stat->maxSP = new_self_par;
	stat->totalWork += new_stats->work;
	stat->tpWork += new_stats->cp;
	stat->spWork += new_stats->spWork;

	stat->totalIterCount += new_stats->childCnt;
	if (stat->minIterCount > new_stats->childCnt) 
		stat->minIterCount = new_stats->childCnt;
	if (stat->maxIterCount < new_stats->childCnt) 
		stat->maxIterCount = new_stats->childCnt;
#ifdef EXTRA_STATS
	stat->readCnt += new_stats->readCnt;
	stat->writeCnt += new_stats->writeCnt;
	stat->loadCnt += new_stats->loadCnt;
	stat->storeCnt += new_stats->storeCnt;
#endif

	assert(stat->num_instances > 0);
}

const char* CNode::toString() {
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

void CNode::moveToNextCStat() {
	assert(!this->stats.empty() || this->curr_stat_index == -1);
	int stat_index = ++(this->curr_stat_index);

	MSG(DEBUG_CREGION, "CStatForward id %d to page %d\n", this->id, stat_index);

	if (stat_index >= this->stats.size()) {
		CStat *new_stat = new CStat(); // FIXME: memory leak
		this->stats.push_back(new_stat);
	}

	assert(this->curr_stat_index >= 0);
}

void CNode::moveToPrevCStat() {
	assert(this->curr_stat_index >= 0);
	MSG(DEBUG_CREGION, "CStatBackward id %d from page %d\n", this->id, this->curr_stat_index);
	--(this->curr_stat_index);
}

CNode* CNode::getAncestorWithSameStaticID() {
	assert(this->parent != NULL);

	MSG(DEBUG_CREGION, "findAncestor: static_id: 0x%llx....", this->static_id);

	CNode *ancestor = this->parent;
	
	while (ancestor != NULL) {
		if (ancestor->static_id == this->static_id) {
			assert(ancestor->parent != NULL);
			return ancestor;
		}

		ancestor = ancestor->parent;
	}
	return NULL;
}


void CNode::handleRecursion() {
	/*
	 * We will detect recursion by looking for an ancestor with the same
	 * static ID. If no such ancestor exists, the current node isn't a
	 * recursive call. If we find such an ancestor, we: change the ancestor's
	 * node_type to R_INIT, set the this node's node_type to R_SINK, and set this 
	 * node's recursion field to point to the ancestral node.
	 */

	CNode* ancestor = getAncestorWithSameStaticID();

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
