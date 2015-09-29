/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "mmc.h"
#include "sdhc.h"
#include "services.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

static uint32_t slice_bits(uint32_t *val, int start, int size)
{
    int idx;
    int high, low;
    uint32_t ret = 0;

    /* Can not return more than 32 bits. */
    assert(size <= 32);

    idx = start / 32;
    low = start % 32;
    high = (start + size) % 32;

    if (high == 0 && low == 0) {
        ret = val[idx];
    } else if (high == 0 && low != 0) {
        ret = val[idx] >> low;
    } else {
        if (high > low) {
            ret = val[idx] & ((1U << high) - 1);
            ret = ret >> low;
        } else {
            ret = val[idx] >> low;
            ret |= (val[idx + 1] & ((1U << high) - 1)) << (32 - low);
        }

    }

    return ret;
}

#if 0 /* Commenting this out as it appears unused. */
static int mmc_decode_cid(mmc_card_t mmc_card, struct cid *cid)
{
    if (mmc_card == NULL || cid == NULL) {
        return -1;
    }

    if (mmc_card->type == CARD_TYPE_SD) {
        cid->manfid         = slice_bits(mmc_card->raw_cid, 120,  8);
        cid->sd_cid.oemid   = slice_bits(mmc_card->raw_cid, 104, 16);
        cid->sd_cid.name[0] = slice_bits(mmc_card->raw_cid,  96,  8);
        cid->sd_cid.name[1] = slice_bits(mmc_card->raw_cid,  88,  8);
        cid->sd_cid.name[2] = slice_bits(mmc_card->raw_cid,  80,  8);
        cid->sd_cid.name[3] = slice_bits(mmc_card->raw_cid,  72,  8);
        cid->sd_cid.name[4] = slice_bits(mmc_card->raw_cid,  64,  8);
        cid->sd_cid.rev     = slice_bits(mmc_card->raw_cid,  56,  8);
        cid->sd_cid.serial  = slice_bits(mmc_card->raw_cid,  24, 32);
        cid->sd_cid.date    = slice_bits(mmc_card->raw_cid,   8, 12);

        printf("manfid(%x), oemid(%x), name(%c%c%c%c%c), rev(%x), serial(%x), date(%x)\n",
            cid->manfid, cid->sd_cid.oemid,
            cid->sd_cid.name[0], cid->sd_cid.name[1], cid->sd_cid.name[2],
            cid->sd_cid.name[3], cid->sd_cid.name[4],
            cid->sd_cid.rev, cid->sd_cid.serial, cid->sd_cid.date);
    } else {
        printf("Not Implemented!\n");
        return -1;
    }

    return 0;
}
#endif

static int mmc_decode_csd(mmc_card_t mmc_card, struct csd *csd)
{
    if (mmc_card == NULL || csd == NULL) {
        return -1;
    }

#define CSD_BITS(start, size) \
    slice_bits(mmc_card->raw_csd, start, size)

    csd->structure = CSD_BITS(126, 2);

    if (csd->structure == 0) {
        printf("CSD Version 1.0\n");
        csd->c_size      = CSD_BITS(62, 12);
        csd->c_size_mult = CSD_BITS(47,  3);
        csd->read_bl_len = CSD_BITS(80,  4);
        csd->tran_speed  = CSD_BITS(96,  8);
    } else if (csd->structure == 1) {
        printf("CSD Version 2.0\n");
        csd->c_size      = CSD_BITS(48, 22);
        csd->c_size_mult = 0;
        csd->read_bl_len = CSD_BITS(80,  4);
        csd->tran_speed  = CSD_BITS(96,  8);
    } else {
        printf("Unknown CSD version!\n");
        return -1;
    }

    return 0;
}

int
mmc_init(enum sdhc_id id, ps_io_ops_t *io_ops, mmc_card_t* mmc_card){
    mmc_card_t mmc;
    sdhc_dev_t sdhc = NULL;

    /* Allocate the mmc card structure */
    mmc = (mmc_card_t)_malloc(sizeof(*mmc));
    assert(mmc);
    if(!mmc){
        return -1;
    }

    /* Need some abstraction here... May not be an sdhc iface */
    sdhc = sdhc_init(id, mmc, io_ops);
    assert(sdhc);
    if(!sdhc){
        free(mmc);
        return -1;
    }
    mmc->dalloc = &io_ops->dma_manager;
    *mmc_card = mmc;
    assert(mmc);
    return 0;
}

long
mmc_block_read(mmc_card_t mmc_card, unsigned long start,
               int nblocks, void* data, mmc_cb cb, void* token)
{
    struct mmc_cmd cmd;
    struct mmc_data mdata;
    void* buf;
    uintptr_t pbuf = 0;
    int bs = mmc_block_size(mmc_card);
    int bytes = bs * nblocks;
    unsigned long ret;
    /* Allocate the dma buffer */
    buf = ps_dma_alloc_pinned(mmc_card->dalloc, bytes, 0x1000, 0, PS_MEM_NORMAL, &pbuf);
    assert(buf);
    /* Populate the data descriptor */
    mdata.pbuf = pbuf;
    mdata.data_addr = start;
    mdata.block_size = bs;
    mdata.blocks = nblocks;
    /* Write the block */
    cmd.data = &mdata;
    cmd.index = MMC_READ_SINGLE_BLOCK;
    if (mmc_card->high_capacity) {
        cmd.arg = mdata.data_addr;
    } else {
        cmd.arg = mdata.data_addr * mdata.block_size;
    }
    cmd.rsp_type = MMC_RSP_TYPE_R1;

    ret = send_command(mmc_card, &cmd, NULL, NULL);
    /* Copy in the data */
    memcpy(data, buf, bytes);
    ps_dma_free_pinned(mmc_card->dalloc, buf, bytes);
    if(ret){
        return ret;
    }else{
        return mdata.block_size * mdata.blocks;
    }
}


long
mmc_block_write(mmc_card_t mmc_card, unsigned long start,
                int nblocks, const void* data, mmc_cb cb, void* token)
{
    struct mmc_cmd cmd;
    struct mmc_data mdata;
    void* buf;
    uintptr_t pbuf = 0;
    int bs = mmc_block_size(mmc_card);
    int bytes = bs * nblocks;
    unsigned long ret;
    /* Allocate the dma buffer */
    buf = ps_dma_alloc_pinned(mmc_card->dalloc, bytes, 0x1000, 0, PS_MEM_NORMAL, &pbuf);
    assert(buf);
    /* Populate the data descriptor */
    mdata.pbuf = pbuf;
    mdata.data_addr = start;
    mdata.block_size = bs;
    mdata.blocks = nblocks;
    /* Copy in the data */
    memcpy(buf, data, bytes);
    /* Write the block */
    cmd.data = &mdata;
    cmd.index = MMC_WRITE_BLOCK;
    if (mmc_card->high_capacity) {
        cmd.arg = mdata.data_addr;
    } else {
        cmd.arg = mdata.data_addr * mdata.block_size;
    }
    cmd.rsp_type = MMC_RSP_TYPE_R1;
    ret = send_command(mmc_card, &cmd, NULL, NULL);
    /* Free memory */
    ps_dma_free_pinned(mmc_card->dalloc, buf, bytes);
    if(ret){
        return ret;
    }else{
        return mdata.block_size * mdata.blocks;
    }
}

unsigned long long
mmc_card_capacity(mmc_card_t mmc_card) {
    int ret;
    unsigned long long capacity;
    struct csd csd;

    ret = mmc_decode_csd(mmc_card, &csd);
    if (ret) {
        return -1;
    }

    if (csd.structure == 0) {
        capacity = (csd.c_size + 1) * (1U << (csd.c_size_mult + 2));
        capacity *= (1U << csd.read_bl_len);
    } else if (csd.structure == 1) {
        capacity = (csd.c_size + 1) * 512 * 1024;
    } else {
        return -1;
    }

    return capacity;
}


enum sdhc_id
mmc_default_id(void)
{
    return sdhc_plat_default_id();
}
