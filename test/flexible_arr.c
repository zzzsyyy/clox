#include <stdio.h>
#include <stdlib.h>

struct double_vector_st {
	size_t length;
	double array[];
};

struct double_vector_st *allocate_double_vector(size_t len) {
	struct double_vector_st *vec = malloc(sizeof *vec + len * sizeof vec->array[0]);

	if (!vec) {
		perror("malloc double_vector_st failed");
		exit(1);
	}

	vec->length = len;

	for (size_t ix = 0; ix < len; ix++) vec->array[ix] = 0.0;

	return vec;
}

int main() {
	struct double_vector_st *arr = allocate_double_vector(100);
	printf("%lu", sizeof(&arr));
}
