#include <stdio.h>

struct st {
	char arr[100];
	float float_arr[200][300];
};

int main() {
	int a[300];
	int n = 50;
	int b[n];
	n = 200;
	unsigned int i = 10;
	struct st strt;

	a[i] = 100;
	b[i+1] = 100;
	a[i] = b[i] = 0;
	strt.arr[i] = 0;
	strt.float_arr[i][i+1] = 0;	

	return 0;
}
