

/*
/opt/local/bin/ispc -O3 --target-os=linux --target=neon-i16x16 --arch=aarch64 --device=cortex-a53 --PIC -h ./MapPixelsByDepth16.h  -o MapPixelsByDepth16.o_ispc ./MapPixelsByDepth.ispc
/opt/local/bin/ispc -O3 --target-os=linux --target=neon-i16x8 --arch=aarch64 --device=cortex-a53 --PIC -h ./MapPixelsByDepth8.h  -o MapPixelsByDepth8.o_ispc ./MapPixelsByDepth.ispc
*/

// neon-i16x16 will do 16 at once and thus needs to output as uint16.  This REQUIRES the total width to be divisible by 16, otherwise
// we neen the i16x8 version which is a bit slower but avoids the overrun into the next row

// Currently, this will only do up to 12 bit, but trivial change to go to up to 16bit

#if TARGET_WIDTH == 16
#define RESTYPE uint16
#define OPERAIONNAME MapPixelsByDepth16
#else
#define RESTYPE uint8
#define OPERAIONNAME MapPixelsByDepth8
#endif

export void OPERAIONNAME(const uniform uint16 data[], uniform uint32 start, uniform uint32 end, uniform uint32 bits,
        uniform RESTYPE res0[], uniform RESTYPE res1[], uniform RESTYPE res2[], uniform RESTYPE res3[],
        uniform RESTYPE res4[], uniform RESTYPE res5[], uniform RESTYPE res6[], uniform RESTYPE res7[],
        uniform RESTYPE res8[], uniform RESTYPE res9[], uniform RESTYPE res10[], uniform RESTYPE res11[]) {

    for (uniform int x = start; x < end; x += programCount) {
        uniform uint outIdx = ((x - start) / programCount);
        uint16 d = data[x + programIndex];
        uint16 b1 = d & 0x01;
        b1 <<= programIndex;
        uniform uint16 b1u = reduce_add(b1);
        res0[outIdx] = b1u;

        d >>= 1;
        b1 = d & 0x01;
        b1 <<= programIndex;
        b1u = reduce_add(b1);
        res1[outIdx] = b1u;

        d >>= 1;
        b1 = d & 0x01;
        b1 <<= programIndex;
        b1u = reduce_add(b1);
        res2[outIdx] = b1u;

        d >>= 1;
        b1 = d & 0x01;
        b1 <<= programIndex;
        b1u = reduce_add(b1);
        res3[outIdx] = b1u;
        
        d >>= 1;
        b1 = d & 0x01;
        b1 <<= programIndex;
        b1u = reduce_add(b1);
        res4[outIdx] = b1u;

        d >>= 1;
        b1 = d & 0x01;
        b1 <<= programIndex;
        b1u = reduce_add(b1);
        res5[outIdx] = b1u;

        if (bits > 6) {
            d >>= 1;
            b1 = d & 0x01;
            b1 <<= programIndex;
            b1u = reduce_add(b1);
            res6[outIdx] = b1u;
        }
        if (bits > 7) {
            d >>= 1;
            b1 = d & 0x01;
            b1 <<= programIndex;
            b1u = reduce_add(b1);
            res7[outIdx] = b1u;

        }
        if (bits > 8) {
            d >>= 1;
            b1 = d & 0x01;
            b1 <<= programIndex;
            b1u = reduce_add(b1);
            res8[outIdx] = b1u;
        }
        if (bits > 9) {
            d >>= 1;
            b1 = d & 0x01;
            b1 <<= programIndex;
            b1u = reduce_add(b1);
            res9[outIdx] = b1u;
        }
        if (bits > 10) {
            d >>= 1;
            b1 = d & 0x01;
            b1 <<= programIndex;
            b1u = reduce_add(b1);
            res10[outIdx] = b1u;
        }
        if (bits > 11) {
            d >>= 1;
            b1 = d & 0x01;
            b1 <<= programIndex;
            b1u = reduce_add(b1);
            res11[outIdx] = b1u;
        }
    }
}
