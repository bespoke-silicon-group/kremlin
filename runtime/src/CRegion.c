#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "kremlin.h"
#include "MemMapAllocator.h"
#include "CRegion.h"


/*** file local global variables ***/
static CNode* root;
static CNode* current;
static Bool   inRecursion;

/***  local functions ***/
static void CNodeUpdate(CNode* node, RegionField* info);
static CNode* findChildNode(CNode* node, SID sid, CID callSite); 
static void deleteCNode(); 
static CStat* CStatCreate();
static void deleteCStatDelete(CStat* region);
static void emit(char* file);
static void emitRegion(FILE* fp, CNode* node, UInt level);
static CNode* findRecursiveAncestor(SID sid, CID cid);

static CNode* CNodeCreate(SID sid, CID cid); 
static CNode* CNodeFindChild(CNode* node, UInt64 sid, UInt64 callSite);
static void CNodeAttach(CNode* node, CStat* region);
static void CNodeLink(CNode* parent, CNode* child); 
/*** public functions ***/


// initialize CRegion system
void CRegionInit() {
	//fprintf(stderr, "cregionInit..");
	//CRegion* region = createCRegion(0, 0);
	//root = createCNode(NULL, region);

	//current = root;
	//fprintf(stderr, "done!..\n");
	inRecursion = 0;
}

void CRegionDeinit(char* file) {
	emit(file);
}

static CNode* findRecursiveAncestor(SID sid, CID cid) {
	CNode* node = current;
	
	while (node != NULL) {
		if (node->sid == sid) {
			return node;
		}
		node = node->parent;
	}
	return NULL;
}

static void replaceChild(CNode* parent, CNode* oldRegion, CNode* newRegion) {
	CNode* child = parent->firstChild;

	// case 1: first child
	if (child == oldRegion) {
		parent->firstChild = newRegion;
		newRegion->next = oldRegion->next;
		return;
	}

	// case 2: not the first child
	CNode* prev = child;
	child = child->next;
	while (child != NULL) {
		if (child == oldRegion) {
			prev->next = newRegion;
			newRegion->next = oldRegion->next;
			return;
		}
		prev = child;
		child = child->next;
	}
	assert(0);
}

#if 0
CNode* createRNode(CNode* root) {
	CRegion* region = createCRegion(root->region->sid, root->region->callSite);
	CNode* ret = createCNode(NULL, region);
	return ret;
}
#endif


void CRegionEnter(SID sid, CID callSite) {
	CNode* child = NULL;
	if (current != NULL)
		child = CNodeFindChild(current, sid, callSite);

	if (child == NULL) {
		CStat* stat = CStatCreate();
		child = CNodeCreate(sid, callSite);
		CNodeAttach(child, stat);
		if (current != NULL)
			CNodeLink(current, child);
	}
	current = child;

	if (root == NULL)
		root = child;
	
	assert(current != NULL);
	assert(current->region != NULL);
	MSG(3, "CRegion: put context sid: 0x%llx, callSite: 0x%llx\n", 
		sid, callSite);

#if 0
	// detect if it is a recursion
	CNode* ancestor = findRecursiveAncestor(sid, callsite);
	if (ancestor != NULL) {
		assert(ancestor->parent != NULL);
		CNode* rNode = createRNode(ancestor);	
		replaceChild(ancestor->parent, ancestor, rNode);
		inRecursion = 1;
	}

	// additional processing when in recursive region
	if (inRecursion) {
		return;
	} 
#endif
}

// at the end of a region execution,
// pass the region exec info.
// update the passed info and 
// set current pointer to one level higher
void CRegionLeave(RegionField* info) {
	// don't update if we didn't give it any info
	// this happens when we are out of range for logging
	MSG(3, "CRegion: remove context current CNode= 0x%x\n", current);
	if(info != NULL) {
		assert(current != NULL);
		assert(current->region != NULL);
		CNodeUpdate(current, info);
	}
	current = current->parent;	
	MSG(3, "CRegion: move to parent \n");
}

/*** Local Functions */
static int numEntries = 0;
static int numEntriesLeaf = 0;
static int numCreated = 0;

static void emit(char* file) {
	FILE* fp = fopen(file, "w");
	emitRegion(fp, root, 0);
	fclose(fp);
	fprintf(stderr, "[kremlin] Created %d Emitted (all %d leaves %d)\n", numCreated, numEntries, numEntriesLeaf);

	fp = fopen("kremlin_region_graph.dot","w");
	/*
	fprintf(fp,"digraph G {\n");
	emitDOT(fp,root);
	fprintf(fp,"}\n");
	fclose(fp);
	*/
}

static Bool isEmittable(Level level) {
	return level >= KConfigGetMinLevel() && level < KConfigGetMaxLevel();
	//return TRUE;
}

// recursive call
static void emitRegion(FILE* fp, CNode* node, UInt level) {
	CStat* region = node->region;
	//fprintf(stderr, "emitting region %llu at level %u\n", node->region->id,level);
    assert(fp != NULL);
    assert(node != NULL);
    assert(region != NULL);
	//assert(region->numInstance > 0);

	UInt64 size = node->childrenSize;

	if (isEmittable(level))
	{
		numEntries++;
		if(size == 0) { numEntriesLeaf++; }

		fwrite(&node->id, sizeof(Int64), 1, fp);
		fwrite(&node->sid, sizeof(Int64), 1, fp);
		fwrite(&node->callSite, sizeof(Int64), 1, fp);
		fwrite(&node->numInstance, sizeof(Int64), 1, fp);

		fwrite(&region->totalWork, sizeof(Int64), 1, fp);
		fwrite(&region->tpWork, sizeof(Int64), 1, fp);
		fwrite(&region->spWork, sizeof(Int64), 1, fp);

		UInt64 minSPInt = (UInt64)(region->minSP * 100.0);
		UInt64 maxSPInt = (UInt64)(region->maxSP * 100.0);
		fwrite(&minSPInt, sizeof(Int64), 1, fp);
		fwrite(&maxSPInt, sizeof(Int64), 1, fp);
		fwrite(&region->isDoall, sizeof(Int64), 1, fp);

		fwrite(&node->totalChildCount, sizeof(Int64), 1, fp);
		fwrite(&node->minChildCount, sizeof(Int64), 1, fp);
		fwrite(&node->maxChildCount, sizeof(Int64), 1, fp);
		fwrite(&node->childrenSize, sizeof(Int64), 1, fp);

		CNode* current = node->firstChild;

		int i;
		for (i=0; i<size; i++) {
			assert(current != NULL);
			fwrite(&current->id, sizeof(Int64), 1, fp);    
			current = current->next;
		}           
		assert(current == NULL);
	}

	CNode* current = node->firstChild;

	int i;
	for (i=0; i<size; i++) {
		emitRegion(fp, current, level+1);
        current = current->next;
	}
	assert(current == NULL);
}

void emitDOT(FILE* fp, CNode* node) {
	fprintf(stderr,"DOT: visiting %lu\n",node->id);
	CNode* child_node = node->firstChild;
	UInt64 size = node->childrenSize;
	int i;
	for(i = 0; i < size; ++i) {
		fprintf(fp,"\t%ul -> %ul;\n", node->id, child_node->id);
		emitDOT(fp,child_node);
		child_node = child_node->next;
	}
}

static void CNodeUpdate(CNode* node, RegionField* info) {
	CStat* region = node->region;
	assert(region != NULL);
	assert(info != NULL);
	MSG(3, "CRegion: update with: cid(0x%lx), work(0x%lx), cp(%lx), tpWork(%lx), spWork(%lx)\n", 
			info->callSite, info->work, info->cp, info->tpWork, info->spWork);
	MSG(3, "current region: id(0x%lx), sid(0x%lx), cid(0x%lx)\n", 
			node->id, node->sid, node->callSite);
	node->numInstance++;
	assert(node->callSite == info->callSite);

	double sp = (double)info->work / (double)info->spWork;
	if (region->minSP > sp) region->minSP = sp;
	if (region->maxSP < sp) region->maxSP = sp;
	region->totalWork += info->work;
	region->totalCP += info->cp;
	region->tpWork += info->tpWork;
	region->spWork += info->spWork;

	node->totalChildCount += info->childCnt;
	if (node->minChildCount > info->childCnt) 
		node->minChildCount = info->childCnt;
	if (node->maxChildCount < info->childCnt) 
		node->maxChildCount = info->childCnt;

#ifdef EXTRA_STATS
	region->readCnt += info->readCnt;
	region->writeCnt += info->writeCnt;
	region->loadCnt += info->loadCnt;
	region->storeCnt += info->storeCnt;
#endif
	assert(node->numInstance >= 0);

	// handle P bit 
	if (info->isDoall == 0)
		region->isDoall = 0;
}

static CNode* CNodeFindChild(CNode* node, UInt64 sid, UInt64 callSite) {
	CNode* child = node->firstChild;
	//fprintf(stderr, "looking for sid : 0x%llx, callSite: 0x%llx\n", sid, callSite);
	while (child != NULL) {
		//fprintf(stderr, "\tcandidate sid : 0x%llx, callSite: 0x%llx\n", region->sid, region->callSite);
		if (child->sid == sid && child->callSite == callSite) {
			return child;
		} else {
			child = child->next;
		}
	}
	return NULL;
}

static void CNodeLink(CNode* parent, CNode* child) {
	assert(parent != NULL);
	CNode* prevFirstChild = (parent == NULL) ? NULL : parent->firstChild;
	child->next = prevFirstChild;
	parent->firstChild = child;
	parent->childrenSize++;
}

static UInt64 lastId = 0;
static UInt64 CNodeAllocId() { return ++lastId; }


static CNode* CNodeCreate(SID sid, CID cid) {
	CNode* ret = (CNode*)MemPoolAllocSmall(sizeof(CNode));
	MSG(3, "CNode: created CNode at 0x%x\n", ret);

	// basic info
	ret->id = CNodeAllocId();
	ret->sid = sid;
	ret->callSite = cid;
	ret->childrenSize = 0;
	ret->firstChild = NULL;
	ret->numInstance = 0;

	// child info	
	ret->totalChildCount = 0;
	ret->minChildCount = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxChildCount = 0;

	// debug info
	ret->code = 0xDEADBEEF;
	return ret;
}

static void CNodeAttach(CNode* node, CStat* region) {
	node->region = region;
}

#if 0
static CNode* createCNode(CNode* parent, CRegion* region) {
	CNode* ret = (CNode*)MemPoolAllocSmall(sizeof(CNode));
	//fprintf(stderr, "CNode addr = 0x%llx\n", ret);
	assert(region != NULL);
	ret->region = region;
	ret->parent = parent;
	ret->childrenSize = 0;
	ret->firstChild = NULL;

	CNode* prevFirstChild = (parent == NULL) ? NULL : parent->firstChild;
	ret->next = prevFirstChild;
	if (parent != NULL) {
		parent->firstChild = ret;
		parent->childrenSize++;
	}
	
	MSG(3, "CNode: created CNode at 0x%x\n", ret);
	assert(ret->region != NULL);
	numCreated++;
	return ret;	
}
#endif

// remove a CNode
static void CNodeDelete(CNode* node) { 
	MemPoolFreeSmall(node, sizeof(CNode)); 
	//free(region);
}

static CStat* CStatCreate() {
	CStat* ret = (CStat*)MemPoolAllocSmall(sizeof(CStat));
	//fprintf(stderr, "CRegion addr = 0x%llx\n", ret);
	//ret->id = allocateCRegionId();
	//ret->sid = sid;
	//ret->callSite = callSite;
	ret->totalWork = 0;
	ret->tpWork = 0;
	ret->spWork = 0;
	ret->minSP = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxSP = 0;
	ret->readCnt = 0;
	ret->writeCnt = 0;
	ret->loadCnt = 0;
	ret->storeCnt = 0;
	//ret->numInstance = 0;
	ret->isDoall = 1;
	return ret;
}

static void deleteCStat(CStat* stat) {
	//free(region);
	MemPoolFreeSmall(stat, sizeof(CStat)); 
}

