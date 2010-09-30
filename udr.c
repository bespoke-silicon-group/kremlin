#include "udr.h"
#include "log.h"

#include <stdlib.h>

int isEquivalent(URegionField field0, URegionField field1) {
	if (field0.work != field1.work)
		return 0;
	if (field0.cp != field1.cp)
		return 0;
	if (field0.readCnt != field1.readCnt)
		return 0;
	if (field0.writeCnt != field1.writeCnt)
		return 0;
		
	return 1;
}

DRegion* allocateDRegion(UInt64 sid, UInt64 did, UInt64 pSid,
	UInt64 pDid, URegionField field) {
	DRegion* ret = (DRegion*)malloc(sizeof(DRegion));
	ret->sid = sid;
	ret->did = did;
	ret->pSid = pSid;
	ret->pDid = pDid;
	ret->field = field;
	return ret;	
}

void freeDRegion(DRegion* target) {
	free(target);
}


DRegion* stackTop = NULL;

ChildInfo* createChild(UInt64 uid) {
	ChildInfo* ret = (ChildInfo*)malloc(sizeof(ChildInfo));
	ret->uid = uid;
	ret->cnt = 1;
	ret->next = NULL;
	return ret;
}

int isChildrenSame(ChildInfo* srcHead, ChildInfo* destHead) {
	ChildInfo* srcCurrent = srcHead;
	ChildInfo* destCurrent = destHead;
	
	while (srcCurrent != NULL && destCurrent!= NULL) {
		if (srcCurrent->uid != destCurrent->uid)
			return 0;
		if (srcCurrent->cnt != destCurrent->cnt)
			return 0;

		srcCurrent = srcCurrent->next;
		destCurrent = destCurrent->next;
	}

	if (srcCurrent != NULL || destCurrent != NULL)
		return 0;

	return 1;
}

// add a child to children
ChildInfo* addChild(DRegion* region, ChildInfo* head) {
	ChildInfo* current = head;
	int found = 0;
	while(current != NULL) {
		if (region->uregion->uid == current->uid) {
			current->cnt++;
			found = 1;
			return head;
		}	
		current = current->next;
	}	
	
	assert(found == 0);
	ChildInfo* info = createChild(region->uregion->uid);
	ChildInfo* last = NULL;
	current = head;
	// head case
	if (head == NULL || info->uid < head->uid) {
		info->next = head;
		return info;
	}

	// body case
	while (current != NULL) {
		assert(info->uid != current->uid);
		
		if (info->uid < current->uid) {
			assert(last != NULL);
			assert(last->uid < info->uid);
			info->next = current;
			last->next = info;
			return head;
		}	
		last = current;	
		current = current->next;	
	}
	
	// tail case
	last->next = info;
	return head;
	
}

void freeChildren(ChildInfo* head) {
	ChildInfo* current = head;
	while (current != NULL) {
		ChildInfo* next = current->next;
		free(current);
		current = next;
	}
}

int getChildrenSize(ChildInfo* head) {
	int size = 0;
	ChildInfo* current = head;
	while(current != NULL) {
		size++;
		current = current->next;
	}
	return size;
}

ChildInfo* copyChildren(ChildInfo* head) {
	if (head == NULL) {
		return NULL;
	}
	
	ChildInfo* retHead = createChild(head->uid);
	retHead->cnt = head->cnt;

	if (head->next == NULL) {
		return retHead;
	}
	
	ChildInfo* currSrc = head->next;
	ChildInfo* currDest = retHead;
	while (currSrc != NULL) {
		ChildInfo* toAdd = createChild(currSrc->uid);
		toAdd->cnt = currSrc->cnt;

		currDest->next = toAdd;
		currDest = toAdd;
		currSrc = currSrc->next;	
	}
	return retHead;
}

void printChildren(ChildInfo* head) {
	ChildInfo* current = head;
	while (current != NULL) {
		fprintf(stderr, "\t[%lld:%lld]", current->uid, current->cnt);
		current = current->next;
	}
}

void checkChildren(ChildInfo* head) {
	if (head == NULL)
		return;

	ChildInfo* current = head->next;
	ChildInfo* last = head;

	while (current != NULL) {
		if (last->uid >= current->uid) {
			printChildren(head);
			assert(0);
		}
		last = current;
		current = current->next;
	}
}

#define MAPSIZE	4096
#define ENTRYSIZE (1024 * 16)
URegion* _uregionMap[MAPSIZE][ENTRYSIZE];
int _uregionMapPtr[MAPSIZE];
int _uidPtr;
long long _uregionCnt = 0;

URegion* createURegion(UInt64 uid, UInt64 sid, URegionField field, UInt64 pSid, ChildInfo* head) {
	URegion* ret = (URegion*)malloc(sizeof(URegion));
	ret->uid = uid;
	ret->sid = sid;
	ret->field = field;
	ret->cnt = 1;
//	ret->pSid = pSid;
	ret->childrenSize = getChildrenSize(head);
	ret->cHeader = copyChildren(head);
	//checkChildren(ret->cHeader);
	_uregionCnt++;
	return ret;
}

void freeURegion(URegion* region) {
	freeChildren(region->cHeader);
	free(region);
	_uregionCnt--;
}

void printURegion(URegion* region) {
#if 0
	fprintf(stderr, "uid: %lld sid: %lld psid: %lld, work: %lld cp: %lld cnt: %lld\n",
			region->uid, region->sid, region->pSid, region->work, region->cp, region->cnt);
	printChildren(region->cHeader);
	fprintf(stderr, "\n");
#endif
}

// find a compatible URegion, or create one
// if a new URegion is created, Children info must be
// deep-copied
URegion* updateURegion(DRegion* region, ChildInfo* head) {
	int i = 0;

	for (i = 0; i < _uregionMapPtr[region->sid]; i++) {
		URegion* current = _uregionMap[region->sid][i];
		if (current->sid == region->sid &&
			//current->pSid == region->pSid &&
			isEquivalent(current->field, region->field) &&
			isChildrenSame(current->cHeader, head)) {
			current->cnt++;
			return current;	
		}
	}	
	
	URegion* ret = createURegion(_uidPtr++, region->sid, region->field,
					region->pSid, head);
	assert(_uregionMapPtr[region->sid] <= ENTRYSIZE);

	_uregionMap[region->sid][_uregionMapPtr[region->sid]++] = ret;

	return ret;
}

void processUdr(UInt64 sid, UInt64 did, UInt64 pSid, 
	UInt64 pDid, URegionField field) {
	
	DRegion* region = allocateDRegion(sid, did, pSid, pDid, field);
	
	int exit = 0;
	ChildInfo* headChild = NULL;

	while(stackTop != NULL && exit == 0) {
		DRegion* top = stackTop;
		exit = 1;
		if (top->pSid == region->sid && top->pDid == region->did) {
		// parent
			headChild = addChild(top, headChild);
			stackTop = stackTop->next;
			freeDRegion(top);
			exit = 0;
		}
	}
	
	// update the associated URegion or create a new one
	URegion* uregion = updateURegion(region, headChild);
	region->uregion = uregion;
	region->next = stackTop;
	stackTop = region;
	freeChildren(headChild);
	printURegion(uregion);
}


void finalizeUdr() {
	int i, j;
	File* fp = log_open("cpURegion.bin");
	for (i = 0; i < MAPSIZE; i++) {
		for (j = 0; j < _uregionMapPtr[i]; j++) {
			URegion* current = _uregionMap[i][j];
			writeURegion(fp, current);	
			freeURegion(current);
		}
	}
	log_close(fp);
	fprintf(stderr, "%d entries emitted to cpURegion.bin\n", _uidPtr);
}

