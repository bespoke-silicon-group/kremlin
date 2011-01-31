#include <stdio.h>
#include <stdlib.h>
#include "cregion.h"


/*** file local global variables ***/
static CNode* root;
static CNode* current;

/***  local functions ***/
static void updateCRegion(CRegion* region, RegionField* info);
static CNode* findChildNode(CNode* node, UInt64 sid, UInt64 callSite); 
static CNode* createCNode(CNode* parent, CRegion* region);
static void deleteCNode(); 
static CRegion* createCRegion(UInt64 sid, UInt64 callSite);
static void deleteCRegion(CRegion* region);
static void emit(char* file);
static void emitRegion(FILE* fp, CNode* node);


/*** public functions ***/


// initialize CRegion system
void cregionInit() {
	fprintf(stderr, "cregionInit..");
	CRegion* region = createCRegion(0, 0);
	root = createCNode(NULL, region);

	current = root;
	fprintf(stderr, "done!..\n");
}

void cregionFinish(char* file) {
	emit(file);
}

// move the pointer to the right place 
void cregionPutContext(UInt64 sid, UInt64 callSite) {
	CNode* child = findChildNode(current, sid, callSite);
	if (child == NULL) {
		CRegion* region = createCRegion(sid, callSite);
		child = createCNode(current, region);
	}
	current = child;
	//fprintf(stderr, "put context sid: 0x%llx, callSite: 0x%llx\n", sid, callSite);
}

// at the end of a region execution,
// pass the region exec info.
// update the passed info and 
// set current pointer to one level higher
void cregionRemoveContext(RegionField* info) {
	updateCRegion(current->region, info);
	current = current->parent;	
	//fprintf(stderr, "removing context \n");
}

/*** Local Functions */
static int numEntries = 0;
static int numEntriesLeaf = 0;

static void emit(char* file) {
	FILE* fp = fopen(file, "w");
	emitRegion(fp, root);
	fclose(fp);
	fprintf(stderr, "[Emit] total = %d, leaves = %d\n", numEntries, numEntriesLeaf);
}

// recursive call
static void emitRegion(FILE* fp, CNode* node) {
	CRegion* region = node->region;
	numEntries++;
	//fprintf(stderr, "emitting region 0x%llx\n", node->region->id);
    assert(fp != NULL);
    assert(node != NULL);
    assert(region != NULL);
    fwrite(&region->id, sizeof(Int64), 1, fp);
    fwrite(&region->sid, sizeof(Int64), 1, fp);
    fwrite(&region->callSite, sizeof(Int64), 1, fp);
    fwrite(&region->numInstance, sizeof(Int64), 1, fp);
    fwrite(&region->totalWork, sizeof(Int64), 1, fp);
    fwrite(&region->totalCP, sizeof(Int64), 1, fp);

#ifdef EXTRA_STATS
    fwrite(&region->field.readCnt, sizeof(Int64), 1, fp);
    fwrite(&region->field.writeCnt, sizeof(Int64), 1, fp);
    fwrite(&region->field.readLineCnt, sizeof(Int64), 1, fp);
    fwrite(&region->field.writeLineCnt, sizeof(Int64), 1, fp);
#else
    UInt64 tmp = 0;
    fwrite(&tmp, sizeof(Int64), 1, fp);
    fwrite(&tmp, sizeof(Int64), 1, fp);
    fwrite(&tmp, sizeof(Int64), 1, fp);
    fwrite(&tmp, sizeof(Int64), 1, fp);
#endif

	UInt64 size = node->childrenSize;
	if (size == 0)
		numEntriesLeaf++;
	int i;
    fwrite(&node->childrenSize, sizeof(Int64), 1, fp);
    CNode* current = node->firstChild;
	for (i=0; i<size; i++) {
		assert(current != NULL);
        fwrite(&current->region->id, sizeof(Int64), 1, fp);    
        current = current->next;
    }           
	assert(current == NULL);

	current = node->firstChild;
	for (i=0; i<size; i++) {
		emitRegion(fp, current);
        current = current->next;
	}
	assert(current == NULL);
}

static void updateCRegion(CRegion* region, RegionField* info) {
	//fprintf(stderr, "update current region with info: callSite(0x%llx), work(0x%llx), CP(0x%llx)\n", 
	//		info->callSite, info->work, info->cp);
	//fprintf(stderr, "current region: id(0x%llx), sid(0x%llx), callSite(0x%llx)\n", 
	//		region->id, region->sid, region->callSite);
	assert(region->callSite == info->callSite);
	region->totalWork += info->work;
	region->totalCP += info->cp;
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
	
	return ret;	
}

// remove a CNode
static void deleteCNode(CRegion* region) {
	free(region);
}

static UInt64 lastId = 0;
static UInt64 allocateCRegionId() {
	return ++lastId;
}


static CRegion* createCRegion(UInt64 sid, UInt64 callSite) {
	CRegion* ret = (CRegion*)malloc(sizeof(CRegion));
	ret->id = allocateCRegionId();
	ret->sid = sid;
	ret->callSite = callSite;
	ret->totalWork = 0;
	ret->numInstance = 0;
	return ret;
}

static void deleteCRegion(CRegion* region) {
	free(region);
}

