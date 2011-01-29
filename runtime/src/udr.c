#include "udr.h"
#include "log.h"
#include "hash_map.h"
#include "vector.h"

#include <stdlib.h>

typedef vector* URegions;
typedef UInt64 StaticId;
HASH_MAP_DEFINE_PROTOTYPES(sid_uregions, StaticId, URegions);
HASH_MAP_DEFINE_FUNCTIONS(sid_uregions, StaticId, URegions);

UInt64 idHash(StaticId id) { return id; }
int idCompare(StaticId id1, StaticId id2) { return id1 == id2; }

/**
 * Delete a vector without having to take the address of it.
 */
void vectorDelete(vector* v)
{
    vector_delete(&v);
}

int isEquivalent(RegionField field0, RegionField field1) {
    if (field0.work != field1.work)
        return 0;
    if (field0.cp != field1.cp)
        return 0;
    if (field0.callSite != field1.callSite)
        return 0;

#ifdef EXTRA_STATS
    if (field0.readCnt != field1.readCnt)
        return 0;
    if (field0.writeCnt != field1.writeCnt)
        return 0;
        
    if (field0.readLineCnt != field1.readLineCnt) {
        return 0;
    }
    if (field0.writeLineCnt != field1.writeLineCnt) {
        return 0;
    }
#endif

    return 1;
}

DRegion* allocateDRegion(UInt64 sid, UInt64 did, UInt64 pSid,
    UInt64 pDid, RegionField field) {
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

static hash_map_sid_uregions* _sidToURegions;
int _uidPtr;
long long _uregionCnt = 0;

URegion* createURegion(UInt64 uid, UInt64 sid, RegionField field, UInt64 pSid, ChildInfo* head) {
    URegion* ret = (URegion*)malloc(sizeof(URegion));
    ret->uid = uid;
    ret->sid = sid;
    ret->field = field;
    ret->cnt = 1;
//  ret->pSid = pSid;
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
    URegions uRegions;
    URegions* uRegionsPtr = hash_map_sid_uregions_get(_sidToURegions,region->sid);
    if(uRegionsPtr)
        uRegions = *uRegionsPtr;
    else 
    { 
        vector_create(&uRegions, NULL, (void(*)(void*))freeURegion);
        hash_map_sid_uregions_put(_sidToURegions, region->sid, uRegions, TRUE);
    }

    URegion** it;
    URegion** end;
    for(it = (URegion**)vector_begin(uRegions), end = (URegion**)vector_end(uRegions); it < end; it++) 
    {
        URegion* uRegion = *it;
        if (uRegion->sid == region->sid &&
            //uRegion->pSid == region->pSid &&
            isEquivalent(uRegion->field, region->field) &&
            isChildrenSame(uRegion->cHeader, head)) 
        {
            uRegion->cnt++;
            return uRegion; 
        }
    }

    URegion* ret = createURegion(_uidPtr++, region->sid, region->field,
                    region->pSid, head);

    vector_push(uRegions, ret);

    return ret;
}

void processUdr(UInt64 sid, UInt64 did, UInt64 pSid, 
    UInt64 pDid, RegionField field) {
    
    DRegion* region = allocateDRegion(sid, did, pSid, pDid, field); // XXX: Memory leak
    
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

void initializeUdr() {

    // Initialize a hash map from static ids to URegion vectors. This map owns
    // all vectors it points to and will automatically delete them.
    hash_map_sid_uregions_create(&_sidToURegions, idHash, idCompare, NULL, vectorDelete);
}

void finalizeUdr() {
    File* fp = log_open("cpURegion.bin");
    hash_map_sid_uregions_iterator it;
    StaticId sid;
    for(sid = hash_map_sid_uregions_it_init(&it, _sidToURegions); sid; sid = hash_map_sid_uregions_it_next(&it))
    {
        URegions uRegions = *hash_map_sid_uregions_it_value(&it);
        URegion** it;
        URegion** end;
        for(it = (URegion**)vector_begin(uRegions), end = (URegion**)vector_end(uRegions); it < end; it++)
        {
            URegion* uRegion = *it;
            writeURegion(fp, uRegion);  
        }
    }
    log_close(fp);
    fprintf(stderr, "%d entries emitted to cpURegion.bin\n", _uidPtr);

    // Delete the hash map and the delete function will clean up the vectors
    // stored inside. The delete function for the vectors will handle cleaning
    // up the URegions.
    hash_map_sid_uregions_delete(&_sidToURegions);
}

