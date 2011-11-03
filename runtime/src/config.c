#include <stdio.h>

static int useCompression = 0;

void setCompression(int compress) {
	useCompression = compress;
}

int getCompression() {
	return useCompression;	
}

static int cBufferSize = 4096;
void setCBufferSize(int size) {
	cBufferSize = size;
	fprintf(stderr, "setting CBufferSize to %d\n", size);
}

int getCBufferSize() {
	return cBufferSize;
}

static int _regionDepth = 20;

void setRegionDepth(int depth) {
	fprintf(stderr, "[kremlin] Setting region depth to %d\n", depth);
	_regionDepth = depth;
	setMaxLevel(depth);
}

int getRegionDepth() {
	return _regionDepth;
}

