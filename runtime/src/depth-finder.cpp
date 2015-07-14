#include "kremlin.h"
static unsigned int curr_level, max_level;
Level __kremlin_min_level = 0;
Level __kremlin_max_level = 21;



void* logBinaryOp(uint32_t opCost, uint32_t src0, uint32_t src1, uint32_t dest) {}
void* logBinaryOpConst(uint32_t opCost, uint32_t src, uint32_t dest) {}

void* logAssignment(uint32_t src, uint32_t dest) {}
void* logAssignmentConst(uint32_t dest) {}

void* logInsertValue(uint32_t src, uint32_t dest) {}
void* logInsertValueConst(uint32_t dest) {} 

void* logLoadInst(Addr src_addr, uint32_t dest, uint32_t size) {} 
void* logLoadInst1Src(Addr src_addr, uint32_t src1, uint32_t dest, uint32_t size) {}
void* logLoadInst2Src(Addr src_addr, uint32_t src1, uint32_t src2, uint32_t dest, uint32_t size) {}
void* logLoadInst3Src(Addr src_addr, uint32_t src1, uint32_t src2, uint32_t src3, uint32_t dest, uint32_t size) {}
void* logLoadInst4Src(Addr src_addr, uint32_t src1, uint32_t src2, uint32_t src3, uint32_t src4, uint32_t dest, uint32_t size) {}
void* logStoreInst(uint32_t src, Addr dest_addr, uint32_t size) {} 
void* logStoreInstConst(Addr dest_addr, uint32_t size) {} 

void logMalloc(Addr addr, size_t size, uint32_t dest) {}
void logRealloc(Addr old_addr, Addr new_addr, size_t size, uint32_t dest) {}
void logFree(Addr addr) {}

void* logPhiNode1CD(uint32_t dest, uint32_t src, uint32_t cd) {} 
void* logPhiNode2CD(uint32_t dest, uint32_t src, uint32_t cd1, uint32_t cd2) {} 
void* logPhiNode3CD(uint32_t dest, uint32_t src, uint32_t cd1, uint32_t cd2, uint32_t cd3) {} 
void* logPhiNode4CD(uint32_t dest, uint32_t src, uint32_t cd1, uint32_t cd2, uint32_t cd3, uint32_t cd4) {} 

void* log4CDToPhiNode(uint32_t dest, uint32_t cd1, uint32_t cd2, uint32_t cd3, uint32_t cd4) {}

void* logPhiNodeAddCondition(uint32_t dest, uint32_t src) {}

void prepareInvoke(uint64_t x) {}
void invokeThrew(uint64_t x) {}
void invokeOkay(uint64_t x) {}

void addControlDep(uint32_t cond) {}
void removeControlDep() {}

void prepareCall(uint64_t x, uint64_t y) {}
void addReturnValueLink(uint32_t dest) {}
void logFuncReturn(uint32_t src) {} 
void logFuncReturnConst(void) {}

void linkArgToLocal(uint32_t src) {} 
void linkArgToConst(void) {}
void transferAndUnlinkArg(uint32_t dest) {} 

void* logLibraryCall(uint32_t cost, uint32_t dest, uint32_t num_in, ...) {} 

void logBBVisit(uint32_t bb_id) {} 

void* logInductionVar(uint32_t dest) {} 
void* logInductionVarDependence(uint32_t induct_var) {} 

void* logReductionVar(uint32_t opCost, uint32_t dest) {} 

void initProfiler() {
	curr_level = 0;
	max_level = 0;
}
void deinitProfiler() {
	FILE *fp = fopen("kremlin.depth.txt","w");

	fprintf(fp,"%u\n",max_level);
	fclose(fp);
}

void turnOnProfiler() {}
void turnOffProfiler() {}

void logRegionEntry(uint64_t region_id, uint32_t region_type) {
	++curr_level;

	max_level = (curr_level > max_level) ? curr_level : max_level;
}

void logRegionExit(uint64_t region_id, uint32_t region_type) {
	--curr_level;
}

void cppEntry() {}
void cppExit() {}

void setupLocalTable(uint32_t maxVregNum, uint32_t maxLoopDepth) {}

void printProfileData() {}
int main(int argc, char* argv[]) { __main(argc,argv); }
