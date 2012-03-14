#include "kremlin.h"
static unsigned int curr_level, max_level;
Level __kremlin_min_level = 0;
Level __kremlin_max_level = 21;



void* logBinaryOp(UInt opCost, UInt src0, UInt src1, UInt dest) {}
void* logBinaryOpConst(UInt opCost, UInt src, UInt dest) {}

void* logAssignment(UInt src, UInt dest) {}
void* logAssignmentConst(UInt dest) {}

void* logInsertValue(UInt src, UInt dest) {}
void* logInsertValueConst(UInt dest) {} 

void* logLoadInst(Addr src_addr, UInt dest, UInt32 size) {} 
void* logLoadInst1Src(Addr src_addr, UInt src1, UInt dest, UInt32 size) {}
void* logLoadInst2Src(Addr src_addr, UInt src1, UInt src2, UInt dest, UInt32 size) {}
void* logLoadInst3Src(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt dest, UInt32 size) {}
void* logLoadInst4Src(Addr src_addr, UInt src1, UInt src2, UInt src3, UInt src4, UInt dest, UInt32 size) {}
void* logStoreInst(UInt src, Addr dest_addr, UInt32 size) {} 
void* logStoreInstConst(Addr dest_addr, UInt32 size) {} 

void logMalloc(Addr addr, size_t size, UInt dest) {}
void logRealloc(Addr old_addr, Addr new_addr, size_t size, UInt dest) {}
void logFree(Addr addr) {}

void* logPhiNode1CD(UInt dest, UInt src, UInt cd) {} 
void* logPhiNode2CD(UInt dest, UInt src, UInt cd1, UInt cd2) {} 
void* logPhiNode3CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3) {} 
void* logPhiNode4CD(UInt dest, UInt src, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {} 

void* log4CDToPhiNode(UInt dest, UInt cd1, UInt cd2, UInt cd3, UInt cd4) {}

void* logPhiNodeAddCondition(UInt dest, UInt src) {}

void prepareInvoke(UInt64 x) {}
void invokeThrew(UInt64 x) {}
void invokeOkay(UInt64 x) {}

void addControlDep(UInt cond) {}
void removeControlDep() {}

void prepareCall(UInt64 x, UInt64 y) {}
void addReturnValueLink(UInt dest) {}
void logFuncReturn(UInt src) {} 
void logFuncReturnConst(void) {}

void linkArgToLocal(UInt src) {} 
void linkArgToConst(void) {}
void transferAndUnlinkArg(UInt dest) {} 

void* logLibraryCall(UInt cost, UInt dest, UInt num_in, ...) {} 

void logBBVisit(UInt bb_id) {} 

void* logInductionVar(UInt dest) {} 
void* logInductionVarDependence(UInt induct_var) {} 

void* logReductionVar(UInt opCost, UInt dest) {} 

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

void logRegionEntry(UInt64 region_id, UInt region_type) {
	++curr_level;

	max_level = (curr_level > max_level) ? curr_level : max_level;
}

void logRegionExit(UInt64 region_id, UInt region_type) {
	--curr_level;
}

void cppEntry() {}
void cppExit() {}

void setupLocalTable(UInt maxVregNum, UInt maxLoopDepth) {}

void printProfileData() {}
int main(int argc, char* argv[]) { __main(argc,argv); }
