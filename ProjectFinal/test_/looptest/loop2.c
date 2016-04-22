
int main() {

    int foo[10];
    int a=2;
    int b = a;

    for(int i=0;i<10;i++) {
        //int b = 2;
        //b=3;
        //int a = i;
        foo[i] = 23;
        foo[b+1] = i;
    }


}
