#ifndef _GSLX680_FW_H_
#define _GSLX680_FW_H_

#include "../../../utility/pgmspace.h"

struct __attribute__((packed)) gsl_fw_data
{
  uint8_t offset;
  uint32_t val;
};

/*
 ファームウェアの解像度情報は、PAGE 0x06 のレジスタ 0x24 と 0x28 から読み取ることができる。;

例 : 
{0xf0,0x6},        // ← PAGE 0x06
{0x00,0x0000000f},
{0x04,0x00000000},
{0x08,0x0000000a},
{0x0c,0x04020402},
{0x10,0x00000032},
{0x14,0x140a010a},
{0x18,0x00000000},
{0x1c,0x00000001},
{0x20,0x00002904},
{0x24,0x000001e0},  // ← 0x24:Height  0x01E0 = 480
{0x28,0x00000320},  // ← 0x28:Width   0x0320 = 800

*/

#endif
