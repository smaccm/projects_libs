/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _SDHC_MMC_H_
#define _SDHC_MMC_H_

#include <platsupport/io.h>
#include <dma/dma.h>
#include <sdhc/sdhc.h>

typedef struct mmc_card* mmc_card_t;

/** Return the default MMC interface ID for the platform
 * @return the device ID of the default SDHC interface for the
 *         running platform.
 */
static inline int mmc_default_id(void)
{
    return sdhc_default_id();
}

static inline int mmc_block_size(mmc_card_t mmc_card)
{
    return 512;
}

/** Initialise an MMC card
 * @param[in]  id            The ID of the interface to
 *                           probe
 * @param[in]  io_ops        Handle to a structure which provides IO
 *                           and DMA operations.
 * @param[out] mmc_card      On success, this will be filled with
 *                           a handle to the MMC card 
 *                           associated with the provided id.
 * @return                   0 on success.
 */
int mmc_init(enum sdhc_id id, ps_io_ops_t *io_ops, mmc_card_t* mmc_card);

/** Read blocks from the MMC
 * @param[in] mmc_card  A handle to an initialised MMC card
 * @param[in] start     the starting block number of the operation
 * @param[in] nblocks   The number of blocks to read
 * @param[in] buf       The address of a buffer to read the data into
 * @return              The number of bytes read, 0 on failure.
 */
unsigned long mmc_block_read(mmc_card_t mmc_card,
                             unsigned long start_block,
                             int nblocks,
                             void* data);

/** Write blocks to the MMC
 * @param[in] mmc_card  A handle to an initialised MMC card
 * @param[in] start     The starting block number of the operation
 * @param[in] nblocks   The number of blocks to write
 * @param[in] buf       The address of a buffer that contains the data to be written
 * @return              The number of bytes read, 0 on failure.
 */
unsigned long mmc_block_write(mmc_card_t mmc_card, 
                             unsigned long start_block,
                             int nblocks,
                             const void* data);

/** Get card capacity
 * @param[in] mmc_card  A handle to an initialised MMC card
 * @return              Card capacity in bytes
 */
unsigned long long mmc_card_capacity(mmc_card_t mmc_card);
#endif /* _SDHC_MMC_H_ */
