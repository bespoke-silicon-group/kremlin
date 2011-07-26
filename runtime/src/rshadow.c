#include "table.h"
#include "shadow.h"


/*
 * Register Shadow Memory 
 */

UInt regShadowInit() {

}

UInt regShadowFinalize() {

}


Timestamp regGetTimestamp(Reg reg, Level level) {
	Version version = getVersion(level);
	TEntry* entry = getLTEntry(reg);
	return getTimestamp(entry, level, version);
}

void regSetTimestamp(Timestamp time, Reg reg, Level level) {
	Version version = getVersion(level);
	TEntry* entry = getLTEntry(reg);
	updateTimestamp(entry, level, version, time);
}
