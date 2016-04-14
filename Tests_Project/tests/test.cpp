
int count = 0;

int main(int argc, char ** argv) {
  int arr[5] = {1, 2, 3, 4, 5};
  int arr2[3] = {3, 2, 1};
  int i = 0;
  if (argc > 1) {
    int i = 2;
    arr[i-1] = arr2[i];
  } else {
    i = arr2[i+1];
  }
  arr[i] = 2;
  return 0; 
}


