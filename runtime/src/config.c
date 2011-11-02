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
