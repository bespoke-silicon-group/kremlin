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

// TODO: use initializer list
CNode::CNode(SID static_id, CID callsite_id, RegionType type) {
	this->parent = NULL;
	new(&this->children) std::vector<CNode*, MPoolLib::PoolAllocator<CNode*> >();
	this->type = NORMAL;
	this->rType = type;
	this->id = CNode::allocId();
	this->sid = static_id;
	this->cid = callsite_id;
	this->recursion = NULL;
	this->numInstance = 0;
	this->isDoall = 1;

	// debug info
	this->code = 0xDEADBEEF;

	// stat
	new(&this->stats) std::vector<CStat*, MPoolLib::PoolAllocator<CStat*> >();
	this->curr_stat_index = -1;
}

CNode::~CNode() {
	this->children.~vector<CNode*, MPoolLib::PoolAllocator<CNode*> >();
	this->stats.~vector<CStat*, MPoolLib::PoolAllocator<CStat*> >();
}

// returns child with specified static and callsite IDs or NULL if no child
// matches
CNode* CNode::getChild(UInt64 sid, UInt64 callSite) {
	//fprintf(stderr, "looking for sid : 0x%llx, callSite: 0x%llx\n", sid, callSite);
	for (unsigned i = 0; i < this->children.size(); ++i) {
		CNode* child = this->children[i];
		//fprintf(stderr, "\tcandidate sid : 0x%llx, callSite: 0x%llx\n", child->sid, child->cid);
		if ( child->sid == sid
			&& (child->rType != RegionFunc || child->cid == callSite)
		   ) {
			return child;
		}
	}

	//fprintf(stderr, "\tnot found, creating one..\n");
	return NULL;
}

// adds a node to list of children
void CNode::addChild(CNode* child) {
	this->children.push_back(child);
	child->parent = this;
}

void CNode::updateStats(RegionField* info) {
	assert(info != NULL);
	MSG(DEBUG_CREGION, "CRegionUpdate: cid(0x%lx), work(0x%lx), cp(%lx), spWork(%lx)\n", 
			info->callSite, info->work, info->cp, info->spWork);
	MSG(DEBUG_CREGION, "current region: id(0x%lx), sid(0x%lx), cid(0x%lx)\n", 
			this->id, this->sid, this->cid);

	//assert(this->cid == info->callSite);
	assert(this->numInstance >= 0);

	// handle P bit for DOALL identification
	if (info->isDoall == 0)
		this->isDoall = 0;

	this->numInstance++;
	this->updateCurrentCStat(info);
}

void CNode::updateCurrentCStat(RegionField* info) {
	assert(this->curr_stat_index != -1);
	assert(info != NULL);

	CStat* stat = this->stats[this->curr_stat_index];

	MSG(DEBUG_CREGION, "CStatUpdate: work = %d, spWork = %d\n", info->work, info->spWork);
	
	double sp = (double)info->work / (double)info->spWork;
	stat->numInstance++;
	if (stat->minSP > sp) stat->minSP = sp;
	if (stat->maxSP < sp) stat->maxSP = sp;
	stat->totalWork += info->work;
	stat->tpWork += info->cp;
	stat->spWork += info->spWork;

	stat->totalIterCount += info->childCnt;
	if (stat->minIterCount > info->childCnt) 
		stat->minIterCount = info->childCnt;
	if (stat->maxIterCount < info->childCnt) 
		stat->maxIterCount = info->childCnt;


#ifdef EXTRA_STATS
	stat->readCnt += info->readCnt;
	stat->writeCnt += info->writeCnt;
	stat->loadCnt += info->loadCnt;
	stat->storeCnt += info->storeCnt;
#endif
}

// XXX: unused?
// side effect: oldRegion's parent is set to NULL
void CNode::replaceChild(CNode* old_child, CNode* new_child) {
	old_child->parent = NULL;

	for (unsigned i = 0; i < this->children.size(); ++i) {
		CNode* child = this->children[i];
		if (child == old_child) {
			this->children[i] = new_child;
			return;
		}
	}

	assert(0);
	return;
}

const char* CNode::toString() {
	char _buf[256]; // FIXME: C++ string?
	const char* _strType[] = {"NORM", "RINIT", "RSINK"};

	UInt64 parentId = (this->parent == NULL) ? 0 : this->parent->id;
	UInt64 childId = (this->children.empty()) ? 0 : this->children[0]->id;
	std::stringstream ss;
	ss << "id: " << this->id << "type: " << _strType[this->type] << ", parent: " << parentId << ", firstChild: " << childId << ", sid: " << this->sid;
	return ss.str().c_str();
}

void CNode::statForward() {
	int stat_index = ++(this->curr_stat_index);

	MSG(DEBUG_CREGION, "CStatForward id %d to page %d\n", this->id, stat_index);

	// FIXME: it appears as though if and else-if can be combined
	if (this->stats.size() == 0) {
		assert(stat_index == 0); // FIXME: is this correct assumption?
		CStat* new_stat = new CStat(); // FIXME: memory leak
		this->stats.push_back(new_stat);
	}

	else if (stat_index >= this->stats.size()) {
		CStat* new_stat = new CStat(); // FIXME: memory leak
		this->stats.push_back(new_stat);
	}
}

void CNode::statBackward() {
	assert(this->curr_stat_index != -1);
	MSG(DEBUG_CREGION, "CStatBackward id %d from page %d\n", this->id, this->curr_stat_index);
	--(this->curr_stat_index);
}

CNode* CNode::findAncestorBySid() {
	MSG(DEBUG_CREGION, "findAncestor: sid: 0x%llx....", this->sid);
	SID sid = this->sid;
	CNode* ancestor = this->parent;
	
	while (ancestor != NULL) {
		if (ancestor->sid == sid) {
			return ancestor;
		}

		ancestor = ancestor->parent;
	}
	return NULL;
}


void CNode::handleRecursion() {
	// detect a recursion with a new node
	// - find an ancestor where ancestor.sid == node.sid
	// - case a) no ancestor found - no recursion
	// - case b) recursion to the root node: self recursion
	//			 transform node to RNode
	// - case c) recursion to a non-root node: a new tree needed
	//	       - create a CTree from a subtree starting from the ancestor
	//         - set current tree and node appropriately

	CNode* ancestor = findAncestorBySid();

	if (ancestor == NULL) {
		return;
#if 0
	} else if (ancestor == this->root) {
		convertToSelfRNode();
		return;
#endif

	} else {
		assert(ancestor->parent != NULL);
#if 0 // XXX: don't use the following code!
		CTree* rTree = CTree::createFromSubTree(ancestor, this);
		CNode* rNode = CNode::createExtRNode(ancestor->sid, ancestor->cid, rTree);
		ancestor->parent->replaceChild(ancestor, rNode);
#endif
		ancestor->type = R_INIT;
		this->type = R_SINK;
		this->recursion = ancestor;
		return;
	}
}

// No idea what this stuff was supposed to do (-sat)
#if 0
CNode* CNode::createExtRNode(SID sid, CID cid, CTree* childTree) {
	CNode* ret = new CNode(sid, cid);
	ret->type = EXT_R;
	ret->tree = childTree;
	childTree->parent = ret;
	return ret;
}; 

void CNode::convertToSelfRNode(CTree* tree) {
	this->tree = tree;
	this->type = SELF_R;
}

bool CNode::isRNode() { return (this->tree != NULL); }

bool CNode::isSelfRNode(CTree* tree) {
	if (!this->isRNode()) return FALSE;
	return (this->tree == tree);
}
#endif

