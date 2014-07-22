/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "sdhc.h"

struct sdhc_dev {
    struct dma_allocator* alloc;
};

/** Return the default SDHC interface ID for the platform
 * @return the device ID of the default SDHC interface for the
 *         running platform.
 */
int sdhc_default_id(void){
    return plat_sdhc_default_id();
}


/** Initialise and SDHC interface
 * @param[in]  id            The ID of the SDHC interface to
 *                           initialise
 * @param[out] sd_dev        On success, this will be filled with
 *                           a handle to the sdhc interface 
 *                           associated with the provided id.
 * @param[in]  dma_allocator a DMA memory allocator instance for
 *                           the SDHC interface to use.
 * @param[in]  io_ops        A structure defining operations for
 *                           device access.
 * @return                   Pointer to SDHC data.
 */
sdhc_dev_t sdhc_init(int id, sdhc_dev_t* sd_dev,
              struct dma_allocator* dma_allocator,
              struct ps_io_mapper* io_map){
    return sdhc_plat_init(id, NULL, dma_allocator, io_map); 
}

/** Pass control to the devices IRQ handler
 * @param[in] sd_dev  The sdhc interface device that triggered 
 *                    the interrupt event.
 */
void sdhc_handle_irq(sdhc_dev_t sd_dev){
    (void)sd_dev;
}



