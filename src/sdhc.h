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

#include "mmc.h"

#define swab(x) __be32_to_cpu(x)

struct sdhc {
    volatile void   *base;
    int             status;
    struct mmc_card *card;
    ps_dma_man_t* dalloc;
};
typedef struct sdhc* sdhc_dev_t;



/* Perform some type checking when getting/setting private data */
static inline struct sdhc*
_mmc_get_sdhc(struct mmc_card* mmc){
    return (struct sdhc*)mmc->priv;
}

static inline void
_mmc_set_sdhc(struct mmc_card* mmc, struct sdhc* sdhc){
    mmc->priv = (void*)sdhc;
}



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


int sdhc_send_cmd(struct sdhc *host, struct mmc_cmd *cmd);
sdhc_dev_t sdhc_init(enum sdhc_id id, mmc_card_t card, ps_io_ops_t* io_ops);

/* Platform specific code */
enum sdhc_id sdhc_plat_default_id(void);
int sdhc_plat_init(enum sdhc_id id, ps_io_ops_t* io_ops, sdhc_dev_t sdhc);


#endif /* _SDHC_H_ */
