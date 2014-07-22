/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _SDHC_H_
#define _SDHC_H_

#include <sdhc/sdhc.h>
#include "mmc.h"

struct dma_allocator;
struct ps_io_mapper;
struct sdhc;

#define swab(x) __be32_to_cpu(x)

static inline uint32_t
__be32_to_cpu(uint32_t x){
    int i;
    uint32_t ret;
    char* a = (char*)&x;
    char* b = (char*)&ret;
    for(i = 0; i < sizeof(x); i++){
        b[i] = a[sizeof(x) - i - 1];
    }
    return ret;
}


/* Interface for platform specific code */

/* void sdhc_plat_init(void) */
extern sdhc_dev_t sdhc_plat_init(int id, mmc_card_t card,
                          struct dma_allocator* dma_allocator,
                          struct ps_io_mapper* io_map);


int sdhc_send_cmd(struct sdhc *host, struct mmc_cmd *cmd);
void sdhc_plat_reset(struct sdhc *host);
void sdhc_plat_interrupt(void);

extern int plat_sdhc_default_id(void);


#endif /* _SDHC_H_ */
