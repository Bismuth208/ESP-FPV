/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor include file               (C)ChaN, 2019
/-----------------------------------------------------------------------------/
/  modify by lovyan03
/ May 29, 2019 Tweak for ArduinoESP32
/-----------------------------------------------------------------------------/
/  modify by Bismuth208
/ October 15, 2022 cleanup for ESP-IDF
/----------------------------------------------------------------------------*/

#ifndef DEF_TJPGDEC
#define DEF_TJPGDEC

#ifdef __cplusplus
extern "C" {
#endif

#include "tjpgdcnf.h"

#include <string.h>
#include <stdint.h>


#if JD_FASTDECODE >= 1
typedef int16_t jd_yuv_t;
#else
typedef uint8_t jd_yuv_t;
#endif


/* Error code */
typedef enum
{
	JDR_OK = 0, /* 0: Succeeded */
	JDR_INTR,   /* 1: Interrupted by output function */
	JDR_INP,    /* 2: Device error or wrong termination of input stream */
	JDR_MEM1,   /* 3: Insufficient memory pool for the image */
	JDR_MEM2,   /* 4: Insufficient stream input buffer */
	JDR_PAR,    /* 5: Parameter error */
	JDR_FMT1,   /* 6: Data format error (may be broken data) */
	JDR_FMT2,   /* 7: Right format but not supported */
	JDR_FMT3    /* 8: Not supported JPEG standard */
} JRESULT;


/* Rectangular region in the output image */
typedef struct
{
	uint16_t left;   /* Left end */
	uint16_t right;  /* Right end */
	uint16_t top;    /* Top end */
	uint16_t bottom; /* Bottom end */
} JRECT;


/* Decompressor object structure */
typedef struct JDEC JDEC;
struct JDEC
{
	uint8_t* dptr;				/* Current data read ptr */
	uint8_t* dpend;				/* data end ptr */
	uint8_t* inbuf;				/* Bit stream input buffer */
	uint8_t dbit;				/* Current bit in the current read byte */
	uint8_t bayer;				/* Output bayer gain */
	uint8_t msx, msy;			/* MCU size in unit of block (width, height) */
	uint8_t qtid[3];			/* Quantization table ID of each component */
	int16_t dcv[3];				/* Previous DC element of each component */
	uint16_t nrst;				/* Restart inverval */
	int32_t width, height;		/* Size of the input image (pixel) */
	uint8_t* huffbits[2][2];	/* Huffman bit distribution tables [id][dcac] */
	uint16_t* huffcode[2][2];	/* Huffman code word tables [id][dcac] */
	uint8_t* huffdata[2][2];	/* Huffman decoded data tables [id][dcac] */
	int32_t* qttbl[4];			/* Dequantizer tables [id] */
	void* pool;					/* Pointer to available memory pool */
	uint16_t sz_pool;			/* Size of momory pool (bytes available) */
	uint32_t (*infunc)(JDEC*, uint8_t*, uint32_t);/* Pointer to jpeg stream input function */
	void* device;				/* Pointer to I/O device identifiler for the session */
};


/* TJpgDec API functions */
JRESULT jd_prepare(JDEC* jd, uint32_t (*infunc)(JDEC*, uint8_t*, uint32_t), void* pool, size_t sz_pool, void* dev);
JRESULT jd_decomp(JDEC* jd, uint32_t (*outfunc)(JDEC*, void*, JRECT*));


#ifdef __cplusplus
}
#endif

#endif /* _TJPGDEC */
