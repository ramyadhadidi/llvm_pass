
int main(int argc, char ** argv) {
  int arr[5] = {1, 2,3, 4, 5};
  int x = argc;
  int j = 0;
  int i = 0;
  do {
    j += arr[x];
    j += arr[i];
    i++;
  } while (i < argc);
  return 0; 
}


