int main() {
	int array[100];
	int i;

	i = 30;
	array[i] = 10;
	if (i > 10) {
		array[i] = 0;
	} else {
    array[i-1] = -1;
  }
	
	return 0;
}
