-c
-heap  0x1000
-stack 0xa000

/* Memory Map */
MEMORY
{
    L1PSRAM (RWX)  : org = 0x00E00000, len = 0x7FFF
    L1DSRAM (RWX)  : org = 0x00F00000, len = 0x7FFF
    L2SRAM (RWX)   : org = 0x00800000, len = 0x100000
    MSMCSRAM (RWX) : org = 0x0C000000, len = 0x100000
    DDR3 (RWX)     : org = 0x80000000, len = 0x10000000
}

SECTIONS
{
    .csl_vect    >      L2SRAM
    .stack       >      L2SRAM
    .text        >      L2SRAM
    GROUP (NEAR_DP)
    {
        .neardata
        .rodata
        .bss
    } load       >      L2SRAM
    .cinit       >      L2SRAM
    .cio         >      L2SRAM
    .const       >      L2SRAM
    .data        >      L2SRAM
    .switch      >      L2SRAM
    .sysmem      >      L2SRAM
    .far         >      L2SRAM
    .testMem     >      L2SRAM
    .fardata     >      L2SRAM
    .shared      >      MSMCSRAM
    platform_lib > 	L2SRAM
}
