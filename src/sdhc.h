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
sdhc_dev_t sdhc_plat_init(enum sdhc_id id, mmc_card_t card, ps_io_ops_t* io_ops);

int sdhc_send_cmd(struct sdhc *host, struct mmc_cmd *cmd);
void sdhc_plat_reset(struct sdhc *host);
void sdhc_plat_interrupt(void);

enum sdhc_id plat_sdhc_default_id(void);


#endif /* _SDHC_H_ */
