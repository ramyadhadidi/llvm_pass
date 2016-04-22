
#include <stdio.h>
#include <stdlib.h>




int local_elimination(){
    int foo[10];
    int bart[12];
    //This will be checked
    foo[5] = 4;
    //This won't be checked
    foo[1] = 4;
    //This will be
    foo[6] = 4;

    int * bar;
    bar = (int *) malloc(10*sizeof(int));

    //This will be checked
    *(bar + 4) = 5;
    //This will be checked
    *(bar + 1) = 5;
    //This wont be checked
    *(bar + 5) = 5;

    for(int i=0;i<10;i++) {
        //This will be checked
        foo[i] = 23;
        //This wont be checked
        foo[i] += 83;
        //This wont be checked
        bart[i] = 234;
    }

    for(int i=0;i<10;i++) {
        //This will be checked
        *(foo + i) = 3333;
        //This wont be checked
        *(foo + i) += 444;
        //This won't be checked 
        *(bart + i) = 43254;
    }


    return 0;
}


void global_elimination_1(void)
{
    int i, j, k, p, q;
    int w[100];
    int x[100];
    int y[100];
    int z[100];
/*
    printf("Before Initialization\n");
    for(i=0;i<100;i++) {
        x[i] = i*i;
        y[i] = x[i]+x[i];
        w[i] = x[i]+x[i];
    }
*/
    printf("Before Loop\n");
    for(i=1;i<100;i++) {
        for(j=1;j<100;j++) {
            for(k=0;k<100;k++) {
                w[k] = x[i+j] + y[i+j];
                z[k] = y[i+j];
		z[k]=x[i]+y[j];
                if(k%2==0)
                    z[k] = x[i-1]*y[j-1];
                else
                    z[k] = x[i]-y[j];
            }
        }
    }

}

int main()
{
//    dynamic_malloc();

    local_elimination();

 //   global_elimination_1();

    return 0;
}
