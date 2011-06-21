#include <stdio.h>

typedef signed long long    Int64;

int main(int argc, char* argv[]) {
	Int64 id, sid, callSite, numInstance, totalWork, tpWork, spWork;
	Int64 minSPInt, maxSPInt, readCnt, writeCnt, loadCnt, storeCnt;

	if(argc < 2) {
		fprintf(stderr,"need to specify the bin file to read\n");
		return 1;
	}

	//FILE* fp = fopen("kremlin.bin", "r");
	printf("using file: %s\n",argv[1]);
	FILE* fp = fopen(argv[1], "r");
	if(!fp) {
		printf("couldn't open %s\n",argv[1]);
		return 1;
	}

	while(!feof(fp)) {
		fread(&id, sizeof(Int64), 1, fp);
		printf("id = %llu, ",id);
		fread(&sid, sizeof(Int64), 1, fp);
		printf("sid = %llu, ",sid);
		fread(&callSite, sizeof(Int64), 1, fp);
		printf("callSite = %llu\n",callSite);
		fread(&numInstance, sizeof(Int64), 1, fp);
		printf("\tnumInstance = %lld, ",numInstance);
		fread(&totalWork, sizeof(Int64), 1, fp);
		printf("totalWork = %lld, ",totalWork);
		fread(&tpWork, sizeof(Int64), 1, fp);
		printf("tpWork = %lld, ",tpWork);
		fread(&spWork, sizeof(Int64), 1, fp);
		printf("spWork = %lld\n",spWork);

		fread(&minSPInt, sizeof(Int64), 1, fp);
		printf("\tminSP = %lld, ",minSPInt);
		fread(&maxSPInt, sizeof(Int64), 1, fp);
		printf("maxSP = %lld, ",maxSPInt);
		fread(&readCnt, sizeof(Int64), 1, fp);
		printf("readCnt = %lld, ",readCnt);
		fread(&writeCnt, sizeof(Int64), 1, fp);
		printf("writeCnt = %lld, ",writeCnt);
		fread(&loadCnt, sizeof(Int64), 1, fp);
		printf("loadCnt = %lld, ",loadCnt);
		fread(&storeCnt, sizeof(Int64), 1, fp);
		printf("storeCnt = %lld\n",storeCnt);

		Int64 num_children, child_id;

		fread(&num_children, sizeof(Int64), 1, fp);
		printf("\tnum_children = %lld\n",num_children);

		int i;
		for (i=0; i<num_children; ++i) {
			fread(&child_id, sizeof(Int64), 1, fp);    
			printf("\t\tchild_id[%d] = %lld, ",i,child_id);
		}

		if(num_children != 0) {
			printf("\n");
		}
	}

	fclose(fp);
}
