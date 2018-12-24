/**
 * Helpers for main.c
 */
#include "relative_value.h"

/**
 * Scale a percent (value which is 0-100) to a new min and max
 * @param  pct value to scale
 * @param  min new min of range
 * @param  max new max of range
 */
float scale(float pct, int min, int max) {
	return (pct / 100.0) * (max - min) + min;
}

/**
 * comparator for qsort
 */
int compare(const void *a, const void *b) {
  float fa = *(const float*) a;
  float fb = *(const float*) b;
  return (fa > fb) - (fa < fb);
}

/**
 * Produces the relative value for a data set given in an array of 'typical' and a 'current' to
 * compare against
 * float array[] = {62.5, 63.0, 65.0, 66.0, 66.5, 70.0};
 * printf("%.3f\n", relative_value(array, 64.0, 6, 0, 100));
 * exit(1); prints 33
 */
float relative_value(float *typical, float current, int size, int min, int max) {
	int i, j, k;
	k = i = 0;
	qsort(typical, size, sizeof(float), compare);
	for (; i < size; ++i) {
		if (typical[i] >= current) {
			j = i;
			// If the typical data contains lots of floats that are the same as current,
			// taking the first occurrence understates the relative value
			// This happens often with water meters that are usually 0 (so the typical has a lot of 0s) and the current reading is also 0
			while (j != (size - 1) && current == typical[++j]) {
				++k; // count how many values are the same 
			}
			break;
		}
	}
	float adjusted_i = i + (k/2); // move the index halfway between the flat-lined data
	float rv = (adjusted_i / (size)) * 100;
	return scale(rv, min, max);
}

