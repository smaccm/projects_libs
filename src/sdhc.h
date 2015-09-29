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
#include <sdhc/sdio.h>

struct sdhc {
    volatile void   *base;
    ps_dma_man_t* dalloc;
};
typedef struct sdhc* sdhc_dev_t;

int sdhc_init(void* vbase, ps_io_ops_t* io_ops, sdio_host_dev_t* dev);

#endif /* _SDHC_H_ */
