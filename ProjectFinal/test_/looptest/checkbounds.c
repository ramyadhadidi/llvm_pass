#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

    // Index 64, Upper 64
    void __checkArrayBounds_max_i64_u64(int64_t idx, int64_t min, int64_t max ){

       
        if ( idx >= max ){
            fprintf(stderr, "Array index %lld exceeds max of %lld. Terminating.\n", (long long)idx, (long long)max );
            exit(1);
        }
    }

    // Index 32, Upper 64
    void __checkArrayBounds_max_i32_u64(int32_t idx, int64_t min, int64_t max ){

      

        if ( (int64_t)idx >= max ){
            fprintf(stderr, "Array index %lld exceeds max of %lld. Terminating.\n", (long long)idx, (long long)max );
            exit(1);
        }
    }

    // Index 64, Upper 32
    void __checkArrayBounds_max_i64_u32(int64_t idx, int64_t min, int32_t max ){

      
        if ( idx >= (int64_t)max ){
            fprintf(stderr, "Array index %lld exceeds max of %lld. Terminating.\n", (long long)idx, (long long)max );
            exit(1);
        }
    }

    // Index 32, Upper 32
    void __checkArrayBounds_max_i32_u32(int32_t idx, int64_t min, int32_t max ){

        if ( (int64_t)idx >= (int64_t)max ){
            fprintf(stderr, "Array index %lld exceeds max of %lld. Terminating.\n", (long long)idx, (long long)max );
            exit(1);
        }
    }
////////////////////////////////////////////////////////////////////
    // Index 64, Upper 64
    void __checkArrayBounds_min_i64_u64(int64_t idx, int64_t min, int64_t max ){

        if ( idx < min ) {
            fprintf(stderr, "Array index %lld smaller than min of %lld. Terminating.\n", (long long)idx, (long long)min );
            exit(1);
        }


    }

    // Index 32, Upper 64
    void __checkArrayBounds_min_i32_u64(int32_t idx, int64_t min, int64_t max ){

        if ((int64_t)idx < min ) {
            fprintf(stderr, "Array index %lld smaller than min of %lld. Terminating.\n", (long long)idx, (long long)min );
            exit(1);
        }


    }

    // Index 64, Upper 32
    void __checkArrayBounds_min_i64_u32(int64_t idx, int64_t min, int32_t max ){

        if ( idx < min ) {
            fprintf(stderr, "Array index %lld smaller than min of %lld. Terminating.\n", (long long)idx, (long long)min );
            exit(1);
        }


    }

    // Index 32, Upper 32
    void __checkArrayBounds_min_i32_u32(int32_t idx, int64_t min, int32_t max ){

        if ((int64_t)idx < min ) {
            fprintf(stderr, "Array index %lld smaller than min of %lld. Terminating.\n", (long long)idx, (long long)min );
            exit(1);
        }

    }
