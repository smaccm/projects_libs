/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "../../sdhc.h"
#include "../../services.h"

#define SDHC0_PADDR 0x12510000
#define SDHC1_PADDR 0x12520000
#define SDHC2_PADDR 0x12530000
#define SDHC3_PADDR 0x12540000
#define SDHC4_PADDR 0x12550000

#define SDHC0_SIZE  0x1000
#define SDHC1_SIZE  0x1000
#define SDHC2_SIZE  0x1000
#define SDHC3_SIZE  0x1000
#define SDHC4_SIZE  0x1000

enum sdhc_id
sdhc_plat_default_id(void){
    return SDHC_DEFAULT;
}

int
sdhc_plat_init(enum sdhc_id id, ps_io_ops_t* io_ops, sdhc_dev_t sdhc)
{
    void* iobase;
    switch(id){
    case SDHC0: iobase = RESOURCE(io_ops, SDHC0); break;
    case SDHC1: iobase = RESOURCE(io_ops, SDHC1); break;
    case SDHC2: iobase = RESOURCE(io_ops, SDHC2); break;
    case SDHC3: iobase = RESOURCE(io_ops, SDHC3); break;
    case SDHC4: iobase = RESOURCE(io_ops, SDHC4); break;
    default:
        return -1;
    }

    sdhc->base = iobase;
    if(sdhc->base == NULL){
        return -1;
    }else{
        return 0;
    }
}


