#include "MergeSort.h"

void merge_sort(int numbers[], int temp[], int left, int right)
{
	int mid;

	if(right > left)
	{
		mid = (right + left) / 2;
		merge_sort(numbers, temp, left, mid);
		merge_sort(numbers, temp, mid + 1, right);

		merge(numbers, temp, left, mid + 1, right);
	}
}
