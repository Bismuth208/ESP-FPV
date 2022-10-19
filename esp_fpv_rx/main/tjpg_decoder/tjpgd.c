/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor R0.01c                     (C)ChaN, 2019
/-----------------------------------------------------------------------------/
/ The TJpgDec is a generic JPEG decompressor module for tiny embedded systems.
/ This is a free software that opened for education, research and commercial
/  developments under license policy of following terms.
/
/  Copyright (C) 2019, ChaN, all right reserved.
/
/ * The TJpgDec module is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-----------------------------------------------------------------------------/
/ Oct 04, 2011 R0.01  First release.
/ Feb 19, 2012 R0.01a Fixed decompression fails when scan starts with an escape seq.
/ Sep 03, 2012 R0.01b Added JD_TBLCLIP option.
/ Mar 16, 2019 R0.01c Supprted stdint.h.
/----------------------------------------------------------------------------/
/ May 2019 ～ July 2020  Tweak for ESP32 ( modify by lovyan03 )
/----------------------------------------------------------------------------/
/ September 2022 ~ October 2022 Further Tweak for ESP32 ( modify by Bismuth208 )
/----------------------------------------------------------------------------*/

#include "tjpgd.h"

#include <esp_attr.h>


static uint8_t jd_workbuf[768];
static jd_yuv_t jd_mcubuf[384];


/*-----------------------------------------------*/
/* Zigzag-order to raster-order conversion table */
/*-----------------------------------------------*/

#define ZIG(n) Zig[n]

// clang-format off
static const DRAM_ATTR uint8_t Zig[64] = {	/* Zigzag-order to raster-order conversion table */
	 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};
// clang-format on


/*-------------------------------------------------*/
/* Input scale factor of Arai algorithm            */
/* (scaled up 16 bits for fixed point operations)  */
/*-------------------------------------------------*/

#define IPSF(n) Ipsf[n]

// clang-format off
static const DRAM_ATTR uint16_t Ipsf[64] = {	/* See also aa_idct.png */
	(uint16_t)(1.00000*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.17588*8192), (uint16_t)(1.00000*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.27590*8192),
	(uint16_t)(1.38704*8192), (uint16_t)(1.92388*8192), (uint16_t)(1.81226*8192), (uint16_t)(1.63099*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.08979*8192), (uint16_t)(0.75066*8192), (uint16_t)(0.38268*8192),
	(uint16_t)(1.30656*8192), (uint16_t)(1.81226*8192), (uint16_t)(1.70711*8192), (uint16_t)(1.53636*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.02656*8192), (uint16_t)(0.70711*8192), (uint16_t)(0.36048*8192),
	(uint16_t)(1.17588*8192), (uint16_t)(1.63099*8192), (uint16_t)(1.53636*8192), (uint16_t)(1.38268*8192), (uint16_t)(1.17588*8192), (uint16_t)(0.92388*8192), (uint16_t)(0.63638*8192), (uint16_t)(0.32442*8192),
	(uint16_t)(1.00000*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.17588*8192), (uint16_t)(1.00000*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.27590*8192),
	(uint16_t)(0.78570*8192), (uint16_t)(1.08979*8192), (uint16_t)(1.02656*8192), (uint16_t)(0.92388*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.61732*8192), (uint16_t)(0.42522*8192), (uint16_t)(0.21677*8192),
	(uint16_t)(0.54120*8192), (uint16_t)(0.75066*8192), (uint16_t)(0.70711*8192), (uint16_t)(0.63638*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.42522*8192), (uint16_t)(0.29290*8192), (uint16_t)(0.14932*8192),
	(uint16_t)(0.27590*8192), (uint16_t)(0.38268*8192), (uint16_t)(0.36048*8192), (uint16_t)(0.32442*8192), (uint16_t)(0.27590*8192), (uint16_t)(0.21678*8192), (uint16_t)(0.14932*8192), (uint16_t)(0.07612*8192)
};
// clang-format on

/*---------------------------------------------*/
/* Output bayer pattern table                  */
/*---------------------------------------------*/

// clang-format off
static const int8_t DRAM_ATTR Bayer[8][32] = {
	{ 0, 4, 1, 5,  0, 4, 1, 5, -2, 2,-1, 3, -2, 2,-1, 3,  1, 5, 0, 4,  1, 5, 0, 4, -1, 3,-2, 2, -1, 3,-2, 2},
	{ 1, 5, 0, 4,  1, 5, 0, 4, -1, 3,-2, 2, -1, 3,-2, 2,  0, 4, 1, 5,  0, 4, 1, 5, -2, 2,-1, 3, -2, 2,-1, 3},
	{ 2,-1, 3,-2,  2,-1, 3,-2,  5, 0, 4, 1,  5, 0, 4, 1,  3,-2, 2,-1,  3,-2, 2,-1,  4, 1, 5, 0,  4, 1, 5, 0},
	{ 3,-2, 2,-1,  3,-2, 2,-1,  4, 1, 5, 0,  4, 1, 5, 0,  2,-1, 3,-2,  2,-1, 3,-2,  5, 0, 4, 1,  5, 0, 4, 1},
	{ 4, 1, 5, 0,  4, 1, 5, 0,  2,-1, 3,-2,  2,-1, 3,-2,  5, 0, 4, 1,  5, 0, 4, 1,  3,-2, 2,-1,  3,-2, 2,-1},
	{ 5, 0, 4, 1,  5, 0, 4, 1,  3,-2, 2,-1,  3,-2, 2,-1,  4, 1, 5, 0,  4, 1, 5, 0,  2,-1, 3,-2,  2,-1, 3,-2},
	{-2, 2,-1, 3, -2, 2,-1, 3,  1, 5, 0, 4,  1, 5, 0, 4, -1, 3,-2, 2, -1, 3,-2, 2,  0, 4, 1, 5,  0, 4, 1, 5},
	{-1, 3,-2, 2, -1, 3,-2, 2,  0, 4, 1, 5,  0, 4, 1, 5, -2, 2,-1, 3, -2, 2,-1, 3,  1, 5, 0, 4,  1, 5, 0, 4}
};
// clang-format on

/*---------------------------------------------*/
/* Conversion table for fast clipping process  */
/*---------------------------------------------*/

#if JD_TBLCLIP

#define BYTECLIP(v) Clip8[(unsigned int)(v)&0x3FF]

// clang-format off
static const DRAM_ATTR uint8_t Clip8[1024] = {
	/* 0..255 */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
	128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
	/* 256..511 */
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	/* -512..-257 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* -256..-1 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
// clang-format on

#else /* JD_TBLCLIP */

static inline uint_fast8_t
BYTECLIP(int32_t val)
{
	return (val < 0) ? 0 : (val > 255) ? 255 : val;
}

#endif


/*-----------------------------------------------------------------------*/
/* Allocate a memory block from memory pool                              */
/*-----------------------------------------------------------------------*/

static void* IRAM_ATTR
alloc_pool(                 /* Pointer to allocated memory block (NULL:no memory available) */
           JDEC* jd,        /* Pointer to the decompressor object */
           uint_fast16_t nd /* Number of bytes to allocate */
)
{
	char* rp = 0;

	nd = (nd + 3) & ~3; /* Align block size to the word boundary */

	if(jd->sz_pool >= nd)
	{
		jd->sz_pool -= nd;
		rp = (char*)jd->pool;        /* Get start of available memory pool */
		jd->pool = (void*)(rp + nd); /* Allocate requierd bytes */
	}

	return (void*)rp; /* Return allocated memory block (NULL:no memory to allocate) */
}


/*-----------------------------------------------------------------------*/
/* Create de-quantization and prescaling tables with a DQT segment       */
/*-----------------------------------------------------------------------*/

static int IRAM_ATTR
create_qt_tbl(                     /* 0:OK, !0:Failed */
              JDEC* jd,            /* Pointer to the decompressor object */
              const uint8_t* data, /* Pointer to the quantizer tables */
              uint_fast16_t ndata  /* Size of input data */
)
{
	uint_fast8_t d, z;
	int32_t* pb;

	do
	{              /* Process all tables in the segment */
		d = *data++; /* Get table property */
		if(d & 0xF0)
			return JDR_FMT1;                                   /* Err: not 8-bit resolution */
		pb = (int32_t*)alloc_pool(jd, 64 * sizeof(int32_t)); /* Allocate a memory block for the table */
		if(!pb)
			return JDR_MEM1;     /* Err: not enough memory */
		jd->qttbl[d & 3] = pb; /* Register the table */
		for(size_t i = 0; i < 64; ++i)
		{                                                 /* Load the table */
			z = ZIG(i);                                     /* Zigzag-order to raster-order conversion */
			pb[z] = (int32_t)((uint32_t)data[i] * IPSF(z)); /* Apply scale factor of Arai algorithm to the de-quantizers */
		}
		data += 64;
	} while(ndata -= 65);

	return JDR_OK;
}


/*-----------------------------------------------------------------------*/
/* Create huffman code tables with a DHT segment                         */
/*-----------------------------------------------------------------------*/

static int IRAM_ATTR
create_huffman_tbl(                     /* 0:OK, !0:Failed */
                   JDEC* jd,            /* Pointer to the decompressor object */
                   const uint8_t* data, /* Pointer to the packed huffman tables */
                   uint_fast16_t ndata  /* Size of input data */
)
{
	uint_fast16_t d, b, np, cls, num, hc = 0;
	uint8_t *pb, *pd;
	uint16_t* ph;

	do
	{              /* Process all tables in the segment */
		d = *data++; /* Get table number and class */
		if(d & 0xEE)
			return JDR_FMT1; /* Err: invalid class/number */
		cls = d >> 4;
		num = d & 0x0F;                    /* class = dc(0)/ac(1), table number = 0/1 */
		pb = (uint8_t*)alloc_pool(jd, 16); /* Allocate a memory block for the bit distribution table */
		if(!pb)
			return JDR_MEM1; /* Err: not enough memory */
		jd->huffbits[num][cls] = pb - 1;
		np = 0;
		for(size_t i = 0; i < 16; ++i)
		{                          /* Load number of patterns for 1 to 16-bit code */
			np += (pb[i] = data[i]); /* Get sum of code words for each code */
		}

		ph = (uint16_t*)alloc_pool(jd, (np * sizeof(uint16_t))); /* Allocate a memory block for the code word table */
		if(!ph)
			return JDR_MEM1; /* Err: not enough memory */
		jd->huffcode[num][cls] = ph - 1;
		hc = 0;
		for(size_t i = 0; i < 16; ++i)
		{ /* Re-build huffman code word table */
			b = pb[i];
			while(b--)
				*ph++ = hc++;
			hc <<= 1;
		}

		pd = (uint8_t*)alloc_pool(jd, np); /* Allocate a memory block for the decoded data */
		if(!pd)
			return JDR_MEM1; /* Err: not enough memory */
		jd->huffdata[num][cls] = pd - 1;

		memcpy(pd, data += 16, np); /* Load decoded data corresponds to each code ward */
		data += np;
	} while(ndata -= 17 + np);

	return JDR_OK;
}


/*-----------------------------------------------------------------------*/
/* Extract a huffman decoded data from input stream                      */
/*-----------------------------------------------------------------------*/

static int_fast16_t IRAM_ATTR
huffext(                    /* >=0: decoded data, <0: error code */
        JDEC* jd,           /* Pointer to the decompressor object */
        const uint8_t* hb,  /* Pointer to the bit distribution table */
        const uint16_t* hc, /* Pointer to the code word table */
        const uint8_t* hd   /* Pointer to the data table */
)
{
	const uint8_t* hb_end = hb + 17;
	uint_fast8_t msk = jd->dbit;
	uint_fast16_t w = *jd->dptr & ((1ul << msk) - 1);

	for(;;)
	{
		if(!msk)
		{ /* Next byte? */
			uint8_t* dp = jd->dptr;
			uint8_t* dpend = jd->dpend;
			msk = 8;
			if(++dp == dpend)
			{                 /* No input data is available, re-fill input buffer */
				dp = jd->inbuf; /* Top of input buffer */
				jd->dpend = dpend = dp + jd->infunc(jd, dp, JD_SZBUF);
				if(dp == dpend)
					return 0 - (int_fast16_t)JDR_INP; /* Err: read error or wrong stream termination */
			}
			uint_fast8_t s = *dp;
			w = (w << 8) + s;
			if(s == 0xFF)
			{ /* Is start of flag sequence? */
				if(++dp == dpend)
				{                 /* No input data is available, re-fill input buffer */
					dp = jd->inbuf; /* Top of input buffer */
					jd->dpend = dpend = dp + jd->infunc(jd, dp, JD_SZBUF);
					if(dp == dpend)
						return 0 - (int_fast16_t)JDR_INP; /* Err: read error or wrong stream termination */
				}
				if(*dp != 0)
					return 0 - (int_fast16_t)JDR_FMT1; /* Err: unexpected flag is detected (may be collapted data) */
				*dp = 0xFF;                          /* The flag is a data 0xFF */
			}
			jd->dptr = dp;
		}

		do
		{
			uint_fast16_t v = w >> --msk;
			uint_fast8_t nc = *++hb;
			if(hb == hb_end)
				return 0 - (int_fast16_t)JDR_FMT1; /* Err: code not found (may be collapted data) */
			if(nc)
			{
				const uint8_t* hd_end = hd + nc;
				do
				{ /* Search the code word in this bit length */
					if(v == *++hc)
						goto huffext_match; /* Matched? */
				} while(++hd != hd_end);
			}
		} while(msk);
	}
huffext_match:
	jd->dbit = msk;
	return *++hd; /* Return the decoded data */
}


/*-----------------------------------------------------------------------*/
/* Extract N bits from input stream                                      */
/*-----------------------------------------------------------------------*/

static inline int_fast16_t IRAM_ATTR
bitext(                  /* >=0: extracted data, <0: error code */
       JDEC* jd,         /* Pointer to the decompressor object */
       int_fast16_t nbit /* Number of bits to extract (1 to 11) */
)
{
	uint_fast8_t msk = jd->dbit;
	uint8_t* dp = jd->dptr;
	uint32_t w = *dp;

	if(msk < nbit)
	{
		do
		{ /* Next byte? */
			uint8_t* dpend = jd->dpend;
			if(++dp == dpend)
			{                 /* No input data is available, re-fill input buffer */
				dp = jd->inbuf; /* Top of input buffer */
				dpend = dp + jd->infunc(jd, dp, JD_SZBUF);
				if(dp == dpend)
					return 0 - (int_fast16_t)JDR_INP; /* Err: read error or wrong stream termination */
				jd->dpend = dpend;
			}
			uint_fast8_t s = *dp;
			w = (w << 8) + s;
			if(s == 0xff)
			{ /* Is start of flag sequence? */
				if(++dp == dpend)
				{                 /* No input data is available, re-fill input buffer */
					dp = jd->inbuf; /* Top of input buffer */
					dpend = dp + jd->infunc(jd, dp, JD_SZBUF);
					if(dp == dpend)
						return 0 - (int_fast16_t)JDR_INP; /* Err: read error or wrong stream termination */
					jd->dpend = dpend;
				}
				if(*dp != 0)
					return 0 - (int_fast16_t)JDR_FMT1; /* Err: unexpected flag is detected (may be collapted data) */
				*dp = 0xff;                          /* The flag is a data 0xFF */
			}
			jd->dptr = dp;
			msk += 8; /* Read from MSB */
		} while(msk < nbit);
	}
	msk -= nbit;
	jd->dbit = msk;

	return (w >> msk) & ((1 << nbit) - 1); /* Get bits */
}


/*-----------------------------------------------------------------------*/
/* Process restart interval                                              */
/*-----------------------------------------------------------------------*/

#if JD_USE_RESTART_INTERVAL >= 1
static JRESULT IRAM_ATTR
restart(JDEC* jd,          /* Pointer to the decompressor object */
        uint_fast16_t rstn /* Expected restert sequense number */
)
{
	uint_fast16_t d;
	uint8_t *dp, *dpend;

	/* Discard padding bits and get two bytes from the input stream */
	dp = jd->dptr;
	dpend = jd->dpend;
	d = 0;
	for(size_t i = 0; i < 2; i++)
	{
		if(++dp == dpend)
		{ /* No input data is available, re-fill input buffer */
			dp = jd->inbuf;
			jd->dpend = dpend = dp + jd->infunc(jd, dp, JD_SZBUF);
			if(dp == dpend)
				return JDR_INP;
		}
		d = (d << 8) | *dp; /* Get a byte */
	}
	jd->dptr = dp;
	jd->dbit = 0;

	/* Check the marker */
	if((d & 0xFFD8) != 0xFFD0 || (d & 7) != (rstn & 7))
	{
		return JDR_FMT1; /* Err: expected RSTn marker is not detected (may be collapted data) */
	}

	/* Reset DC offset */
	jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0;

	return JDR_OK;
}
#endif

/*-----------------------------------------------------------------------*/
/* Apply Inverse-DCT in Arai Algorithm (see also aa_idct.png)            */
/*-----------------------------------------------------------------------*/

static void IRAM_ATTR
block_idct(int32_t* src, /* Input block data (de-quantized and pre-scaled for Arai Algorithm) */
           jd_yuv_t* dst /* Pointer to the destination to store the block as byte array */
)
{
	const int32_t M13 = (int32_t)(1.41421 * 256);
	const int32_t M4 = (int32_t)(2.61313 * 256);
	const float F2 = 1.08239, F5 = 1.84776;

	int32_t v0, v1, v2, v3, v4, v5, v6, v7;
	int32_t t10, t11, t12, t13;

	/* Process columns */
	for(size_t i = 0; i < 8; ++i)
	{
		/* Get and Process the even elements */
		t12 = src[8 * 0];
		t10 = src[8 * 4];
		t10 += t12;
		t12 = (t12 << 1) - t10;

		t11 = src[8 * 2];
		t13 = src[8 * 6];
		t13 += t11;
		t11 = (t11 << 1) - t13;
		t11 = t11 * M13 >> 8;
		t11 = t11 - t13;

		v0 = t10 + t13;
		v3 = t10 - t13;
		v1 = t12 + t11;
		v2 = t12 - t11;

		/* Get and Process the odd elements */
		v4 = src[8 * 1];
		v5 = src[8 * 7];
		v5 += v4;
		v4 = (v4 << 1) - v5;

		v7 = src[8 * 3];
		v6 = src[8 * 5];
		v6 -= v7;
		v7 = (v7 << 1) + v6;
		v7 += v5;

		t13 = v4 + v6;
		t13 *= F5;
		v6 = v6 * M4 >> 8;
		v6 += v7;
		v6 = t13 - v6;
		v5 = (v5 << 1) - v7;
		v5 = v5 * M13 >> 8;
		v5 -= v6;
		v4 *= F2;
		v4 += v5;
		v4 = t13 - v4;

		/* Write-back transformed values */
		src[8 * 0] = v0 + v7;
		src[8 * 7] = v0 - v7;
		src[8 * 1] = v1 + v6;
		src[8 * 6] = v1 - v6;
		src[8 * 2] = v2 + v5;
		src[8 * 5] = v2 - v5;
		src[8 * 3] = v3 + v4;
		src[8 * 4] = v3 - v4;

		++src; /* Next column */
	}

	/* Process rows */
	src -= 8;
	for(size_t i = 0; i < 8; ++i)
	{
		/* Get and Process the even elements */
		t12 = src[0] + (128L << 8); /* remove DC offset (-128) here */
		t10 = src[4];
		t10 += t12;
		t12 = (t12 << 1) - t10;

		t11 = src[2];
		t13 = src[6];
		t13 += t11;
		t11 = (t11 << 1) - t13;
		t11 = t11 * M13 >> 8;
		t11 -= t13;

		v0 = t10 + t13;
		v3 = t10 - t13;
		v1 = t12 + t11;
		v2 = t12 - t11;

		/* Get and Process the odd elements */
		v4 = src[1];
		v5 = src[7];
		v5 += v4;
		v4 = (v4 << 1) - v5;

		v7 = src[3];
		v6 = src[5];
		v6 -= v7;
		v7 = (v7 << 1) + v6;
		v7 += v5;

		t13 = v4 + v6;
		t13 *= F5;
		v6 = v6 * M4 >> 8;
		v6 += v7;
		v6 = t13 - v6;
		v5 = (v5 << 1) - v7;
		v5 = v5 * M13 >> 8;
		v5 -= v6;
		v4 *= F2;
		v4 += v5;
		v4 = t13 - v4;

		/* Descale the transformed values 8 bits and output */
#if JD_FASTDECODE >= 1
		dst[0] = (int16_t)((v0 + v7) >> 8);
		dst[7] = (int16_t)((v0 - v7) >> 8);
		dst[1] = (int16_t)((v1 + v6) >> 8);
		dst[6] = (int16_t)((v1 - v6) >> 8);
		dst[2] = (int16_t)((v2 + v5) >> 8);
		dst[5] = (int16_t)((v2 - v5) >> 8);
		dst[3] = (int16_t)((v3 + v4) >> 8);
		dst[4] = (int16_t)((v3 - v4) >> 8);
#else
		dst[0] = BYTECLIP((v0 + v7) >> 8);
		dst[7] = BYTECLIP((v0 - v7) >> 8);
		dst[1] = BYTECLIP((v1 + v6) >> 8);
		dst[6] = BYTECLIP((v1 - v6) >> 8);
		dst[2] = BYTECLIP((v2 + v5) >> 8);
		dst[5] = BYTECLIP((v2 - v5) >> 8);
		dst[3] = BYTECLIP((v3 + v4) >> 8);
		dst[4] = BYTECLIP((v3 - v4) >> 8);
#endif
		dst += 8;
		src += 8; /* Next row */
	}
}


/*-----------------------------------------------------------------------*/
/* Load all blocks in an MCU into working buffer                         */
/*-----------------------------------------------------------------------*/

static JRESULT IRAM_ATTR
mcu_load(JDEC* jd,     /* Pointer to the decompressor object */
         jd_yuv_t* bp, /* mcubuf */
         int32_t* tmp  /* Block working buffer for de-quantize and IDCT */
)
{
	int_fast16_t b, d, e = 0;
	uint_fast8_t blk, nby, nbc, i, z = 0;
	const uint8_t *hb, *hd;
	const uint16_t* hc;

	nby = jd->msx * jd->msy; /* Number of Y blocks (1, 2 or 4) */
	nbc = 2;                 /* Number of C blocks (2) */

	for(blk = 0; blk < nby + nbc; blk++)
	{
		uint_fast8_t cmp = (blk < nby) ? 0 : blk - nby + 1; /* Component number 0:Y, 1:Cb, 2:Cr */
		uint_fast8_t id = cmp ? 1 : 0;                      /* Huffman table ID of the component */

		/* Extract a DC element from input stream */
		hb = jd->huffbits[id][0]; /* Huffman table for the DC element */
		hc = jd->huffcode[id][0];
		hd = jd->huffdata[id][0];
		b = huffext(jd, hb, hc, hd); /* Extract a huffman coded data (bit length) */
		if(b < 0)
			return (JRESULT)(-b); /* Err: invalid code or input */
		d = jd->dcv[cmp];       /* DC value of previous block */
		if(b)
		{                    /* If there is any difference from previous block */
			e = bitext(jd, b); /* Extract data bits */
			if(e < 0)
				return (JRESULT)(-e); /* Err: input */
			b = 1 << (b - 1);       /* MSB position */
			if(!(e & b))
				e -= (b << 1) - 1; /* Restore sign if needed */
			d += e;              /* Get current value */
			jd->dcv[cmp] = d;    /* Save current DC value for next block */
		}
		const int32_t* dqf = jd->qttbl[jd->qtid[cmp]]; /* De-quantizer table ID for this component */
		tmp[0] = d * dqf[0] >> 8; /* De-quantize, apply scale factor of Arai algorithm and descale 8 bits */

		/* Extract following 63 AC elements from input stream */
		memset(&tmp[1], 0, 4 * 63); /* Clear rest of elements */
		hb = jd->huffbits[id][1];   /* Huffman table for the AC elements */
		hc = jd->huffcode[id][1];
		hd = jd->huffdata[id][1];
		i = 1; /* Top of the AC elements */
		do
		{
			b = huffext(jd, hb, hc, hd); /* Extract a huffman coded value (zero runs and bit length) */
			if(b == 0)
				break; /* EOB? */
			if(b < 0)
				return (JRESULT)(-b); /* Err: invalid code or input error */
			i += b >> 4;
			if(b &= 0x0F)
			{                    /* Bit length */
				d = bitext(jd, b); /* Extract data bits */
				if(d < 0)
					return (JRESULT)(-d); /* Err: input device */
				b = 1 << (b - 1);       /* MSB position */
				if(!(d & b))
					d -= (b << 1) - 1;      /* Restore negative value if needed */
				z = ZIG(i);               /* Zigzag-order to raster-order converted index */
				tmp[z] = d * dqf[z] >> 8; /* De-quantize, apply scale factor of Arai algorithm and descale 8 bits */
			}
		} while(++i != 64); /* Next AC element */

		if(z == 1)
		{ /* If no AC element or scale ratio is 1/8, IDCT can be ommited and the block is filled with DC value */
			d = (jd_yuv_t)((*tmp / 256) + 128);
			if(JD_FASTDECODE >= 1)
			{
				for(i = 0; i < 64; bp[i++] = d)
					;
			}
			else
			{
				memset(bp, d, 64);
			}
		}
		else
		{
			block_idct(tmp, bp); /* Apply IDCT and store the block to the MCU buffer */
		}

		bp += 64; /* Next block */
	}

	return JDR_OK; /* All blocks have been loaded successfully */
}


/*-----------------------------------------------------------------------*/
/* Output an MCU: Convert YCrCb to RGB and output it in RGB form         */
/*-----------------------------------------------------------------------*/

static JRESULT IRAM_ATTR
mcu_output(JDEC* jd, /* Pointer to the decompressor object */
           jd_yuv_t* mcubuf,
           uint8_t* workbuf,
           uint32_t (*outfunc)(JDEC*, void*, JRECT*), /* RGB output function */
           uint_fast16_t x,                           /* MCU position in the image (left of the MCU) */
           uint_fast16_t y                            /* MCU position in the image (top of the MCU) */
)
{
	uint_fast16_t ix, iy, mx, my, rx, ry;
	jd_yuv_t *py, *pc;
	JRECT rect;

	mx = jd->msx * 8;
	my = jd->msy * 8;                                /* MCU size (pixel) */
	rx = (x + mx <= jd->width) ? mx : jd->width - x; /* Output rectangular size (it may be clipped at right/bottom end) */
	ry = (y + my <= jd->height) ? my : jd->height - y;

	rect.left = x;
	rect.right = x + rx - 1; /* Rectangular area in the frame buffer */
	rect.top = y;
	rect.bottom = y + ry - 1;

	static const float frr = 1.402;
	static const float fgr = 0.71414;
	static const float fgb = 0.34414;
	static const float fbb = 1.772;

	// uint16_t w, *dw = (uint16_t*)workbuf;

	/* Build an RGB MCU from discrete comopnents */
	const int8_t* btbase = Bayer[jd->bayer];
	const int8_t* btbl;
	uint_fast8_t ixshift = (mx == 16);
	uint_fast8_t iyshift = (my == 16);
	iy = 0;
	uint8_t* prgb = workbuf;
	do
	{
		btbl = &btbase[(iy & 3) << 3];
		py = &mcubuf[((iy & 8) + iy) << 3];
		pc = &mcubuf[((mx << iyshift) + (iy >> iyshift)) << 3];
		ix = 0;
		do
		{
			do
			{
				float cb = (pc[0] - 128); /* Get Cb/Cr component and restore right level */
				float cr = (pc[64] - 128);
				++pc;

				/* Convert CbCr to RGB */
				int32_t gg = fgb * cb + fgr * cr;
				int32_t rr = frr * cr;
				int32_t bb = fbb * cb;
				int32_t yy = btbl[0] + py[0]; /* Get Y component */
				prgb[0] = BYTECLIP(yy + rr);
				prgb[1] = BYTECLIP(yy - gg);
				prgb[2] = BYTECLIP(yy + bb);

				// uint8_t r_val = BYTECLIP(yy + rr);
				// uint8_t g_val = BYTECLIP(yy - gg);
				// uint8_t b_val = BYTECLIP(yy + bb);

				// w = ((r_val & 0xF8) << 8) | ((g_val & 0xFC) << 3) | (b_val >> 3);
				// *dw++ = (w << 8) | (w >> 8);

				if(ixshift)
				{
					yy = btbl[1] + py[1]; /* Get Y component */
					prgb[3] = BYTECLIP(yy + rr);
					prgb[4] = BYTECLIP(yy - gg);
					prgb[5] = BYTECLIP(yy + bb);

					// r_val = BYTECLIP(yy + rr);
					// g_val = BYTECLIP(yy - gg);
					// b_val = BYTECLIP(yy + bb);

					// w = ((r_val & 0xF8) << 8) | ((g_val & 0xFC) << 3) | (b_val >> 3);
					// *dw++ = (w << 8) | (w >> 8);
				}
				prgb += 3 << ixshift;
				btbl += 1 << ixshift;
				py += 1 << ixshift;
				ix += 1 << ixshift;
			} while(ix & 7);
			btbl -= 8;
			py += 64 - 8; /* Jump to next block if double block heigt */
		} while(ix != mx);
	} while(++iy != my);

	// if(rx < mx)
	// {
	// 	uint8_t *s, *d;
	// 	s = d = (uint8_t*)workbuf;
	// 	rx *= 3;
	// 	mx *= 3;
	// 	for(size_t y = 1; y < ry; ++y)
	// 	{
	// 		memcpy(d += rx, s += mx, rx); /* Copy effective pixels */
	// 	}
	// }

	/* Convert RGB888 to RGB565 if needed */
	if(JD_FORMAT == 1)
	{
		uint8_t* s = (uint8_t*)workbuf;
		uint16_t w, *d = (uint16_t*)s;
		unsigned int n = rx * ry;

		do
		{
			w = (*s++ & 0xF8) << 8;     // RRRRR-----------
			w |= (*s++ & 0xFC) << 3;    // -----GGGGGG-----
			w |= *s++ >> 3;             // -----------BBBBB
			*d++ = (w << 8) | (w >> 8); // Swap bytes
		} while(--n);
	}

	/* Output the RGB rectangular */
	return outfunc(jd, workbuf, &rect) ? JDR_OK : JDR_INTR;
}


/*-----------------------------------------------------------------------*/
/* Analyze the JPEG image and Initialize decompressor object             */
/*-----------------------------------------------------------------------*/

#define LDB_WORD(ptr) (uint16_t)(((uint16_t) * ((uint8_t*)(ptr)) << 8) | (uint16_t) * (uint8_t*)((ptr) + 1))

JRESULT IRAM_ATTR
jd_prepare(JDEC* jd,
           uint32_t (*infunc)(JDEC*, uint8_t*, uint32_t), /* JPEG strem input function */
           void* pool,
           size_t sz_pool,
           void* dev /* I/O device identifier for the session */
)
{
	uint8_t* seg;
	uint_fast8_t b, marker;
	uint_fast16_t i, len;
	JRESULT rc;

	jd->pool = pool;       /* Work memory */
	jd->sz_pool = sz_pool; /* Size of given work memory */
	jd->infunc = infunc;   /* Stream input function */
	jd->device = dev;      /* I/O device identifier */
	jd->nrst = 0;          /* No restart interval (default) */

	jd->inbuf = seg = jd->dptr = (uint8_t*)alloc_pool(jd, JD_SZBUF); /* Allocate stream input buffer */
	if(!seg)
		return JDR_MEM1;

	uint32_t dctr = infunc(jd, jd->dptr, 16);
	seg = jd->dptr;
	if(dctr <= 2)
		return JDR_INP; /* Check SOI marker */
	if(LDB_WORD(seg) != 0xFFD8)
		return JDR_FMT1; /* Err: SOI is not detected */
	jd->dptr += 2;
	dctr -= 2;

	for(;;)
	{
		/* Get a JPEG marker */
		if(dctr < 4)
		{
			if(4 > (JD_SZBUF - (jd->dptr - jd->inbuf)))
				return JDR_MEM2;
			dctr += infunc(jd, jd->dptr + dctr, 4);
			if(dctr < 4)
				return JDR_INP;
		}
		seg = jd->dptr;
		jd->dptr += 4;
		dctr -= 4;

		if(*seg++ != 0xFF)
			return JDR_FMT1;
		marker = *(seg++);   /* Marker */
		len = LDB_WORD(seg); /* Length field */
		if(len <= 2)
			return JDR_FMT1;
		len -= 2; /* Content size excluding length field */

		/* Load segment data */
		if(dctr < len)
		{
			if(len - dctr > (JD_SZBUF - (jd->dptr - jd->inbuf)))
				return JDR_MEM2;
			dctr += infunc(jd, jd->dptr + dctr, len - dctr);
			if(dctr < len)
				return JDR_INP;
		}
		seg = jd->dptr;
		jd->dptr += len;
		dctr -= len;

		switch(marker)
		{
		case 0xC0:                        /* SOF0 (baseline JPEG) */
			jd->width = LDB_WORD(seg + 3);  /* Image width in unit of pixel */
			jd->height = LDB_WORD(seg + 1); /* Image height in unit of pixel */
			if(seg[5] != 3)
				return JDR_FMT3; /* Err: Supports only Y/Cb/Cr format */

			/* Check three image components */
			for(i = 0; i < 3; i++)
			{
				b = seg[7 + 3 * i]; /* Get sampling factor */
				if(!i)
				{ /* Y component */
					if(b != 0x11 && b != 0x22 && b != 0x21)
					{                  /* Check sampling factor */
						return JDR_FMT3; /* Err: Supports only 4:4:4, 4:2:0 or 4:2:2 */
					}
					jd->msx = b >> 4;
					jd->msy = b & 15; /* Size of MCU [blocks] */
				}
				else
				{ /* Cb/Cr component */
					if(b != 0x11)
						return JDR_FMT3; /* Err: Sampling factor of Cr/Cb must be 1 */
				}
				b = seg[8 + 3 * i]; /* Get dequantizer table ID for this component */
				if(b > 3)
					return JDR_FMT3; /* Err: Invalid ID */
				jd->qtid[i] = b;
			}
			break;

#if JD_USE_RESTART_INTERVAL >= 1
		case 0xDD: /* DRI */
			/* Get restart interval (MCUs) */
			jd->nrst = LDB_WORD(seg);
			break;
#endif

		case 0xC4: /* DHT */
			/* Create huffman tables */
			rc = (JRESULT)create_huffman_tbl(jd, seg, len);
			if(rc)
				return rc;
			break;

		case 0xDB: /* DQT */
			/* Create de-quantizer tables */
			rc = (JRESULT)create_qt_tbl(jd, seg, len);
			if(rc)
				return rc;
			break;

		case 0xDA: /* SOS */
			if(!jd->width || !jd->height)
				return JDR_FMT1; /* Err: Invalid image size */

			if(seg[0] != 3)
				return JDR_FMT3; /* Err: Supports only three color components format */

			/* Check if all tables corresponding to each components have been loaded */
			for(i = 0; i < 3; i++)
			{
				b = seg[2 + 2 * i]; /* Get huffman table ID */
				if(b != 0x00 && b != 0x11)
					return JDR_FMT3; /* Err: Different table number for DC/AC element */
				b = i ? 1 : 0;
				/* Check dc/ac huffman table for this component */
				if(!jd->huffbits[b][0] || !jd->huffbits[b][1])
				{
					return JDR_FMT1; /* Err: Nnot loaded */
				}
				/* Check dequantizer table for this component */
				if(!jd->qttbl[jd->qtid[i]])
				{
					return JDR_FMT1; /* Err: Not loaded */
				}
			}

			/* Allocate working buffer for MCU and RGB */
			if(!jd->msy || !jd->msx)
				return JDR_FMT1; /* Err: SOF0 has not been loaded */
			jd->dbit = 0;
			jd->dpend = jd->dptr + dctr;
			--jd->dptr;

			return JDR_OK; /* Initialization succeeded. Ready to decompress the JPEG image. */

		case 0xC1:         /* SOF1 */
		case 0xC2:         /* SOF2 */
		case 0xC3:         /* SOF3 */
		case 0xC5:         /* SOF5 */
		case 0xC6:         /* SOF6 */
		case 0xC7:         /* SOF7 */
		case 0xC9:         /* SOF9 */
		case 0xCA:         /* SOF10 */
		case 0xCB:         /* SOF11 */
		case 0xCD:         /* SOF13 */
		case 0xCE:         /* SOF14 */
		case 0xCF:         /* SOF15 */
		case 0xD9:         /* EOI */
			return JDR_FMT3; /* Unsuppoted JPEG standard (may be progressive JPEG) */

		default: /* Unknown segment (comment, exif or etc..) */
			break;
		}
	}
}


/*-----------------------------------------------------------------------*/
/* Start to decompress the JPEG picture                                  */
/*-----------------------------------------------------------------------*/

JRESULT IRAM_ATTR
jd_decomp(JDEC* jd, uint32_t (*outfunc)(JDEC*, void*, JRECT*) /* RGB output function */
)
{
	uint16_t x, y, mx, my;
	JRESULT rc = JDR_OK;
#if JD_USE_RESTART_INTERVAL >= 1
	uint16_t rst, rsc = 0;
#endif

	jd->bayer = (jd->bayer + 1) & 7;

	/* Size of the MCU (pixel) */
	mx = jd->msx * 8;
	my = jd->msy * 8;

	/* Initialize DC values */
	jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0;

	/* Vertical loop of MCUs */
	for(y = 0; y < jd->height; y += my)
	{
		/* Horizontal loop of MCUs */
		for(x = 0; x < jd->width; x += mx)
		{
#if JD_USE_RESTART_INTERVAL >= 1
			/* Process restart interval if enabled */
			if(jd->nrst && rst++ == jd->nrst)
			{
				rc = restart(jd, rsc++);
				if(rc != JDR_OK)
					return rc;
				rst = 1;
			}
#endif
			/* Load an MCU (decompress huffman coded stream and apply IDCT) */
			rc = mcu_load(jd, jd_mcubuf, (int32_t*)jd_workbuf);
			if(rc != JDR_OK)
				return rc;

			/* Output the MCU (color space conversion, scaling and output) */
			rc = mcu_output(jd, jd_mcubuf, (uint8_t*)jd_workbuf, outfunc, x, y);
			if(rc != JDR_OK)
				return rc;
		}
	}

	return rc;
}
