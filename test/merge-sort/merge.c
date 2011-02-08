#include "MergeSort.h"

void merge(int numbers[], int temp[], const int left, const int mid, const int right)
{
    int* next_left;
    int* next_right;
    int* next_tmp;

	for(next_left = numbers + left, next_right = numbers + mid + 1, next_tmp = temp + left; 
        next_left <= numbers + mid && next_right <= numbers + right; 
        next_tmp++)
	{
		if(*next_left < *next_right)
			*next_tmp = *next_left++;
		else
			*next_tmp = *next_right++;
	}
    
	while(next_right <= numbers + right)
        *next_tmp++ = *next_right++;

	while(next_left <= numbers + mid)
        *next_tmp++ = *next_left++;

    for(next_left = numbers + left, next_tmp = temp + left; next_tmp <= temp + right; next_left++, next_tmp++)
        *next_left = *next_tmp;
}
