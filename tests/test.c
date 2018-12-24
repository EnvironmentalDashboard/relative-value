#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "minunit.h"
#include "../relative_value.c"

int tests_run = 0;

int nearlyEqual(float a, float b, float epsilon) {
	float absA = fabsf(a);
	float absB = fabsf(b);
	float diff = fabsf(a - b);
	if (diff < epsilon) {
		return 1;
	}
	return 0;
}

int test_scale() {
	float res = scale(25.0, 0, 200);
	_assert(res == 50.0);
	res = scale(25.0, 0, 50);
	_assert(res == 12.5);
	return 0;
}

int test_relative_value() {
	float mock_typical[] = {62.5, 63.0, 65.0, 66.0, 66.5, 70.0};
	float mock_current = 64.0;
	float res = relative_value(mock_typical, mock_current, sizeof(mock_typical)/sizeof(float), 0, 100);
	_assert(nearlyEqual(res, 33.333336, 0.00001));
	return 0;
}

int all_tests() {
  _verify(test_scale);
  _verify(test_relative_value);
  return 0;
}

int main(int argc, char **argv) {
  int result = all_tests();
  if (result == 0) {
    printf("PASSED\n");
  }
  printf("Tests run: %d\n", tests_run);

  return result;
}

