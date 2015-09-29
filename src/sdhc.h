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

#include <platsupport/io.h>

struct sdhc {
    volatile void   *base;
    int             status;
    struct mmc_card *card;
    ps_dma_man_t* dalloc;
};
typedef struct sdhc* sdhc_dev_t;

struct mmc_cmd;
typedef void (*sdhc_cb)(sdhc_dev_t sdhc, struct mmc_cmd* cmd, void* token);

#include "mmc.h"


sdhc_dev_t sdhc_init(enum sdhc_id id, mmc_card_t card, ps_io_ops_t* io_ops);

/* Platform specific code */
enum sdhc_id sdhc_plat_default_id(void);
int sdhc_plat_init(enum sdhc_id id, ps_io_ops_t* io_ops, sdhc_dev_t sdhc);


#endif /* _SDHC_H_ */
