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

/** Return the default SDHC interface ID for the platform
 * @return the device ID of the default SDHC interface for the
 *         running platform.
 */
enum sdhc_id sdhc_default_id(void){
    return plat_sdhc_default_id();
}


sdhc_dev_t sdhc_init(enum sdhc_id id, ps_io_ops_t* io_ops, sdhc_dev_t* sd_dev){
    return sdhc_plat_init(id, NULL, io_ops);
}

/** Pass control to the devices IRQ handler
 * @param[in] sd_dev  The sdhc interface device that triggered 
 *                    the interrupt event.
 */
void sdhc_handle_irq(sdhc_dev_t sd_dev){
    (void)sd_dev;
}



