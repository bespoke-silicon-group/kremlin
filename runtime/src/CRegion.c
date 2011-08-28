#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "CRegion.h"


/*** file local global variables ***/
static CNode* root;
static CNode* current;

/***  local functions ***/
static void updateCRegion(CRegion* region, RegionField* info);
static CNode* findChildNode(CNode* node, SID sid, CID callSite); 
static CNode* createCNode(CNode* parent, CRegion* region);
static void deleteCNode(); 
static CRegion* createCRegion(SID sid, CID callSite);
static void deleteCRegion(CRegion* region);
static void emit(char* file);
static void emitRegion(FILE* fp, CNode* node, UInt level);



/*** public functions ***/


// initialize CRegion system
void CRegionInit() {
	//fprintf(stderr, "cregionInit..");
	//CRegion* region = createCRegion(0, 0);
	//root = createCNode(NULL, region);

	//current = root;
	//fprintf(stderr, "done!..\n");
}

void CRegionDeinit(char* file) {
	emit(file);
}

// move the pointer to the right place 
void CRegionEnter(SID sid, CID callSite) {
	CNode* child = NULL;
	if (current != NULL)
		child = findChildNode(current, sid, callSite);
	if (child == NULL) {
		CRegion* region = createCRegion(sid, callSite);
		child = createCNode(current, region);
	}
	current = child;
	if (root == NULL)
		root = child;
	
	MSG(3, "CRegion: put context sid: 0x%llx, callSite: 0x%llx\n", sid, callSite);
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
		updateCRegion(current->region, info);
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
	return level >= getMinLevel() && level < getMaxLevel();
	//return TRUE;
}

// recursive call
static void emitRegion(FILE* fp, CNode* node, UInt level) {
	CRegion* region = node->region;
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

		fwrite(&region->id, sizeof(Int64), 1, fp);
		fwrite(&region->sid, sizeof(Int64), 1, fp);
		fwrite(&region->callSite, sizeof(Int64), 1, fp);
		fwrite(&region->numInstance, sizeof(Int64), 1, fp);
		fwrite(&region->totalWork, sizeof(Int64), 1, fp);
		fwrite(&region->tpWork, sizeof(Int64), 1, fp);
		fwrite(&region->spWork, sizeof(Int64), 1, fp);

		UInt64 minSPInt = (UInt64)(region->minSP * 100.0);
		UInt64 maxSPInt = (UInt64)(region->maxSP * 100.0);
		fwrite(&minSPInt, sizeof(Int64), 1, fp);
		fwrite(&maxSPInt, sizeof(Int64), 1, fp);
		fwrite(&region->readCnt, sizeof(Int64), 1, fp);
		fwrite(&region->writeCnt, sizeof(Int64), 1, fp);
		fwrite(&region->loadCnt, sizeof(Int64), 1, fp);
		fwrite(&region->storeCnt, sizeof(Int64), 1, fp);

		fwrite(&node->childrenSize, sizeof(Int64), 1, fp);

		CNode* current = node->firstChild;

		int i;
		for (i=0; i<size; i++) {
			assert(current != NULL);
			fwrite(&current->region->id, sizeof(Int64), 1, fp);    
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
	CRegion* region = node->region;
	fprintf(stderr,"DOT: visiting %llu\n",region->id);

	CNode* child_node = node->firstChild;

	UInt64 size = node->childrenSize;
	int i;
	for(i = 0; i < size; ++i) {
		CRegion* child_region = child_node->region;
		fprintf(fp,"\t%llu -> %llu;\n",region->id,child_region->id);
		emitDOT(fp,child_node);
		child_node = child_node->next;
	}
}

static void updateCRegion(CRegion* region, RegionField* info) {
	assert(region != NULL);
	assert(info != NULL);
	MSG(3, "CRegion: update current region with info: csid(0x%llx), callSite(0x%llx), work(0x%llx), cp(%llx), tpWork(%llx), spWork(%llx)\n", 
			region->sid, info->callSite, info->work, info->cp, info->tpWork, info->spWork);
	MSG(3, "current region: id(0x%llx), sid(0x%llx), callSite(0x%llx)\n", 
			region->id, region->sid, region->callSite);
	assert(region->callSite == info->callSite);
	double sp = (double)info->work / (double)info->spWork;
	if (region->minSP > sp) region->minSP = sp;
	if (region->maxSP < sp) region->maxSP = sp;
	region->totalWork += info->work;
	region->totalCP += info->cp;
	region->tpWork += info->tpWork;
	region->spWork += info->spWork;
	region->readCnt += info->readCnt;
	region->writeCnt += info->writeCnt;
	region->loadCnt += info->loadCnt;
	region->storeCnt += info->storeCnt;
	assert(region->numInstance >= 0);
	region->numInstance++;
}

static CNode* findChildNode(CNode* node, UInt64 sid, UInt64 callSite) {
	CNode* child = node->firstChild;
	//fprintf(stderr, "looking for sid : 0x%llx, callSite: 0x%llx\n", sid, callSite);
	while (child != NULL) {
		CRegion* region = child->region;
		//fprintf(stderr, "\tcandidate sid : 0x%llx, callSite: 0x%llx\n", region->sid, region->callSite);
		if (region->sid == sid && region->callSite == callSite) {
			return child;
		} else {
			child = child->next;
		}
	}
	return NULL;
}

// create a CNode
static CNode* createCNode(CNode* parent, CRegion* region) {
	CNode* ret = (CNode*)malloc(sizeof(CNode));
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

// remove a CNode
static void deleteCNode(CRegion* region) { free(region); }

static UInt64 lastId = 0;
static UInt64 allocateCRegionId() { return ++lastId; }


static CRegion* createCRegion(UInt64 sid, UInt64 callSite) {
	CRegion* ret = (CRegion*)malloc(sizeof(CRegion));
	ret->id = allocateCRegionId();
	ret->sid = sid;
	ret->callSite = callSite;
	ret->totalWork = 0;
	ret->tpWork = 0;
	ret->spWork = 0;
	ret->minSP = 0xFFFFFFFFFFFFFFFFULL;
	ret->maxSP = 0;
	ret->readCnt = 0;
	ret->writeCnt = 0;
	ret->loadCnt = 0;
	ret->storeCnt = 0;
	ret->numInstance = 0;
	return ret;
}

static void deleteCRegion(CRegion* region) { free(region); }

