
int main(int argc, char ** argv) {
  int arr[5] = {1, 2,3, 4, 5};
  int x = argc;
  int k = 0;
  for (int i = argc; i >= 0; i--) {
    for (int j = 0; j < argc; j++) {
      k += arr[x];
      k += arr[i];
      k += arr[j];
    }
  } 
  return 0; 
}


