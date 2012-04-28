/*
 * This benchmark is based off a doubly nested loop in the nw benchmark from
 * the rodinia suite. Both loops should be DOALL loops.
 */

#include <stdio.h>
#include <stdlib.h>

// specify "long long int" when shadow mem version doesn't handle 32-bit arrays
//typedef long long int integer;
typedef int integer;

integer maxOf3(integer int1, integer int2, integer int3) {
	integer max;
	if(int1 <= int2) max = int2;
	else max = int1;

	if(max <= int3) return int3;
	else return max;
}

int main(int argc, char** argv) {
	int max_rows, max_cols, penalty;

	if (argc == 3)
	{
		max_rows = atoi(argv[1]);
		max_cols = atoi(argv[1]);
		penalty = atoi(argv[2]);
	}
    else{
		printf("need exactly 2 arguments (# rows/cols AND penalty)!\n");
    }

	max_rows = max_rows + 1;
	max_cols = max_cols + 1;

	integer *input_itemsets = calloc(max_rows * max_cols,sizeof(integer));
	integer *referrence = calloc(max_rows * max_cols,sizeof(integer));

	int i;
	for( i = 0 ; i < max_cols-2 ; i++){
		int idx;
		for( idx = 0 ; idx <= i ; idx++){
			int index = (idx + 1) * max_cols + (i + 1 - idx);

			integer match = input_itemsets[index-1-max_cols]+ referrence[index];
			integer drop = input_itemsets[index-1] - penalty;
			integer insert = input_itemsets[index-max_cols]  - penalty;

			integer max = maxOf3(match,drop,insert);

			input_itemsets[index] = max;
		}
	}

	return 0;
}
