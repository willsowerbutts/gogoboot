# 2023-08-20 Will Sowerbutts

#  Cache control bits in the 68030 CACR
CACR_EI   = 1       /* Enable Instruction Cache      */
CACR_DI   = 0       /* Disable Instruction Cache     */
CACR_FI   = 1<<1    /* Freeze Instruction Cache      */
CACR_CEI  = 1<<2    /* Clear Entry in Instr. Cache   */
CACR_CI   = 1<<3    /* Clear Instruction Cache       */
CACR_IBE  = 1<<4    /* Instr. Cache Burst Enable     */

CACR_ED   = 1<<8    /* Enable Data Cache             */
CACR_FD   = 1<<9    /* Freeze Data Cache             */
CACR_CED  = 1<<10   /* Clear Entry in Data Cache     */
CACR_CD   = 1<<11   /* Clear Data Cache              */
CACR_DBE  = 1<<12   /* Data Cache Burst Enable       */
CACR_WA   = 1<<13   /* Write Allocate the Data Cache */

KISS68030_ROM_BASE      = 0xFFF00000
KISS68030_ROM_SIZE      = 0x00080000        /* 512KB */
KISS68030_ECBMEM_BASE   = 0xFFF80000
KISS68030_ECBMEM_SIZE   = 0x00040000        /* 256KB ECB MEM window */
KISS68030_ECBIO_BASE    = 0xFFFF0000
KISS68030_ECBIO_SIZE    = 0x00010000        /* 64KB ECB IO window */
KISS68030_SRAM_BASE     = 0xFFFE0000
KISS68030_SRAM_SIZE     = 0x00008000        /* 32KB SRAM chip (mapped twice) */

KISS68030_MFPIC_ADDR    = 0x40
NS32202_EOI             = 32
