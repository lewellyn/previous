 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68881 emulation
  *
  * Conversion routines for hosts knowing floating point format.
  *
  * Copyright 1996 Herman ten Brugge
  * Modified 2005 Peter Keunecke
  */

#define	FPCR_ROUNDING_MODE	0x00000030
#define	FPCR_ROUND_NEAR		0x00000000
#define	FPCR_ROUND_ZERO		0x00000010
#define	FPCR_ROUND_MINF		0x00000020
#define	FPCR_ROUND_PINF		0x00000030

#define	FPCR_ROUNDING_PRECISION	0x000000c0
#define	FPCR_PRECISION_SINGLE	0x00000040
#define	FPCR_PRECISION_DOUBLE	0x00000080
#define FPCR_PRECISION_EXTENDED	0x00000000

/*STATIC_INLINE int big_endian(void) {
    union {
        uae_u32 l;
        char c[4];
    } test = {0x01020304};
    return test.c[0] == 1;
}*/

#define SINGLE_PRECISION    1
#define DOUBLE_PRECISION    2
#define EXTENDED_PRECISION  3
#define QUAD_PRECISION      4

#if USE_LONG_DOUBLE

STATIC_INLINE int LE_check_float_format(void) { // little endian
    long double testvalue = 18.4;
    
    union {
        long double ld;
        uae_u32 u[1];
    } single32;
    single32.u[0] = 0x41933333;
    
    union {
        long double ld;
        uae_u32 u[2];
    } double64;
    double64.u[0] = 0x66666666;
    double64.u[1] = 0x40326666;
    
    union {
        long double ld;
        uae_u32 u[3];
    } extended80;
    extended80.u[0] = 0x33333333;
    extended80.u[1] = 0x93333333;
    extended80.u[2] = 0x00004003;
    
    union {
        long double ld;
        uae_u32 u[4];
    } quad128;
    quad128.u[0] = 0x66666666;
    quad128.u[1] = 0x66666666;
    quad128.u[2] = 0x66666666;
    quad128.u[3] = 0x40032666;
    
    
    if (((single32.ld-testvalue) < 0.001) && ((single32.ld-testvalue) > -0.001)) {
        printf("single precision\n");
        return SINGLE_PRECISION;
    } else if (((double64.ld-testvalue) < 0.001) && ((double64.ld-testvalue) > -0.001)) {
        printf("double precision\n");
        return DOUBLE_PRECISION;
    } else if (((extended80.ld-testvalue) < 0.001) && ((extended80.ld-testvalue) > -0.001)) {
        printf("extended precision\n");
        return EXTENDED_PRECISION;
    } else if (((quad128.ld-testvalue) < 0.001) && ((quad128.ld-testvalue) > -0.001)) {
        printf("quad precision\n");
        return QUAD_PRECISION;
    } else {
        printf("unknown format\n");
        return 0;
    }
//    printf("extended precision %i: %Lf\n", (int)sizeof(long double), extended80.ld);
}

STATIC_INLINE int BE_check_float_format(void) { // big endian
    long double testvalue = 18.4;
    
    union {
        long double ld;
        uae_u32 u[1];
    } single32;
    single32.u[0] = 0x41933333;
    
    union {
        long double ld;
        uae_u32 u[2];
    } double64;
    double64.u[0] = 0x40326666;
    double64.u[1] = 0x66666666;
    
    union {
        long double ld;
        uae_u32 u[3];
    } extended80;
    extended80.u[0] = 0x40039333;
    extended80.u[1] = 0x33333333;
    extended80.u[2] = 0x33330000;
    
    union {
        long double ld;
        uae_u32 u[4];
    } quad128;
    quad128.u[0] = 0x40032666;
    quad128.u[1] = 0x66666666;
    quad128.u[2] = 0x66666666;
    quad128.u[3] = 0x66666666;
    
    
    if (((single32.ld-testvalue) < 0.001) && ((single32.ld-testvalue) > -0.001)) {
        printf("single precision\n");
        return SINGLE_PRECISION;
    } else if (((double64.ld-testvalue) < 0.001) && ((double64.ld-testvalue) > -0.001)) {
        printf("double precision\n");
        return DOUBLE_PRECISION;
    } else if (((extended80.ld-testvalue) < 0.001) && ((extended80.ld-testvalue) > -0.001)) {
        printf("extended precision\n");
        return EXTENDED_PRECISION;
    } else if (((quad128.ld-testvalue) < 0.001) && ((quad128.ld-testvalue) > -0.001)) {
        printf("quad precision\n");
        return QUAD_PRECISION;
    } else {
        printf("unknown format\n");
        return 0;
    }
    //    printf("extended precision %i: %Lf\n", (int)sizeof(long double), extended80.ld);
}

    
#endif