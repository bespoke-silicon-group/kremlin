#include <assert.h>
#include "ATable.h"
#include "hash_map.h"
#include "MemMapAllocator.h"


#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

HASH_MAP_DEFINE_PROTOTYPES(mt, Addr, size_t);
HASH_MAP_DEFINE_FUNCTIONS(mt, Addr, size_t);


static 	hash_map_mt* mTable;

void 	dumpTableMemAlloc(void);
static 	UInt64 addrHash(Addr addr);
static 	int addrCompare(Addr a1, Addr a2);


long long _tEntryLocalCnt = 0;
long long _tEntryGlobalCnt = 0;


void initMTable() {
	//hash_map_mt_create(&mTable, addrHash, addrCompare, NULL, NULL);
}

void deinitMTable() {
	//hash_map_mt_delete(&mTable);
}

void createMEntry(Addr addr, size_t size) {
//    hash_map_mt_put(mTable, addr, size, TRUE);
}

void freeMEntry(Addr start_addr) {
 //   hash_map_mt_remove(mTable, start_addr);
}

//TODO: rename to something more accurate
size_t getMEntry(Addr addr) {
#if 0
    size_t* ret = hash_map_mt_get(mTable, addr);
    if(!ret)
    {
        fprintf(stderr, "Freeing %p which was never malloc'd!\n", addr);
        assert(0);
    }
    return *ret;
#endif
	return NULL;
}




void dumpTableMemAlloc() {
}

UInt64 addrHash(Addr addr)
{
    return addr;
}
int addrCompare(Addr a1, Addr a2)
{
    return a1 == a2;
}



