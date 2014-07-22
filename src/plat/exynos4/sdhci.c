/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "sdhci.h"
#include "../../services.h"

#include "../../mmc.h"

#define DBG_INFO "info:"

//#define DEBUG
#undef DEBUG
#ifdef DEBUG
#define D(x, ...) printf(__VA_ARGS__)
#else
#define D(...) do{}while(0)
#endif

#define writel(v, a)  (*(volatile uint32_t*)(a) = (v))
#define writew(v, a)  (*(volatile uint16_t*)(a) = (v))
#define writeb(v, a)  (*(volatile uint8_t*)(a) = (v))
#define readl(a)      (*(volatile uint32_t*)(a))
#define readw(a)      (*(volatile uint16_t*)(a))
#define readb(a)      (*(volatile uint8_t*)(a))

#define SDMMC0_PADDR 0x12510000
#define SDMMC1_PADDR 0x12520000
#define SDMMC2_PADDR 0x12530000
#define SDMMC3_PADDR 0x12540000
#define SDMMC4_PADDR 0x12550000

#define SDMMC0_SIZE  0x1000
#define SDMMC1_SIZE  0x1000
#define SDMMC2_SIZE  0x1000
#define SDMMC3_SIZE  0x1000
#define SDMMC4_SIZE  0x1000

#define SDHC4_VADDR mem_data

struct sdhc {
	volatile void   *base;
	int             status;
	struct mmc_card *card;
	struct dma_allocator * dalloc;
};

/* Perform some type checking when getting/setting private data */
static inline struct sdhc*
_mmc_get_sdhc(struct mmc_card* mmc){
    return (struct sdhc*)mmc->priv;
}

static inline void
_mmc_set_sdhc(struct mmc_card* mmc, struct sdhc* sdhc){
    mmc->priv = (void*)sdhc;
}


/** Print uSDHC registers. */
static void print_sdhc_regs(struct sdhc *host)
{
	for (int i = DMA_ADDR; i <= HOST_VERSION; i += 0x4) {
		printf("%x: %X\n", i, readl(host->base + i));
	}
}

/**
 * Send MMC/SD/SDIO command to the card.
 *
 * @reval 0 Success.
 * @reval 1 Timeout.
 *
 * @TODO: Recover from timeout.
 */
int sdhc_send_cmd(struct sdhc *host, struct mmc_cmd *cmd)
{
	uint32_t val;

	writel(0xffffffff, host->base + INT_STATUS);

	/* Check if the Host is ready for transit. */
	while ((readl(host->base + PRES_STATE) & PRES_STATE_CMD_IHB) ||
		(readl(host->base + PRES_STATE) & PRES_STATE_DATA_IHB));

	/* Two commands need to have at least 8 clock cycles in between. */
	udelay(1000);

	if (cmd->data) {
		/* Use the default timeout. */
		writeb(0xE, host->base + TIMEOUT_CTRL);

		/* Set DMA address */
		writel(dma_paddr(cmd->data->dma_buf), host->base + DMA_ADDR);

		val = readb(host->base + HOST_CTRL);
		val &= ~(HOST_CTRL_DMA_MASK << HOST_CTRL_DMA_SHF);
		writeb(val, host->base + HOST_CTRL);

		/* Hard-code DMA boundary to 512KB */
		val = (BLK_SIZE_BOUNDARY_MASK << BLK_SIZE_BOUNDARY_SHF);
		val |= (cmd->data->block_size & BLK_SIZE_MASK);
		writew(val, host->base + BLK_SIZE);
		writew(cmd->data->blocks, host->base + BLK_COUNT);
	}


	/* Write to the argument register. */
	D(DBG_INFO, "CMD: %d with arg %x ", cmd->index, cmd->arg);
	writel(cmd->arg, host->base + ARGUMENT);

	/* Set transfer mode */
	if (cmd->data) {
		val = TRANS_MODE_BLK_CNT_EN | TRANS_MODE_DMA;
		if (cmd->data->blocks > 1) {
			val |= TRANS_MODE_MULTI;
		}
		/* FIXME: Single block read/write is the only command that we use
		 * currently. But there are many others.
		 * Ideally, all read/write operations can be merged, as they are almost
		 * identical.
		 * We just need to introduce a direction flag to differentiate them.
		 */
		if (cmd->index == MMC_READ_SINGLE_BLOCK) {
			val |= TRANS_MODE_READ;
		}
		writew(val, host->base + TRANS_MODE);
	}

	/* Set command */
	val = (cmd->index << CMD_INDEX_SHF);

	/* Set response type */
	switch (cmd->rsp_type) {
		case MMC_RSP_TYPE_R2:
			val |= 0x1;       //Long response(136bits)
			val |= CMD_CRC;
			break;
		case MMC_RSP_TYPE_R3:
		case MMC_RSP_TYPE_R4:
			val |= 0x2;       //Short response
			break;
		case MMC_RSP_TYPE_R1:
		case MMC_RSP_TYPE_R5:
		case MMC_RSP_TYPE_R6:
			val |= 0x2;
			val |= CMD_CRC;
			val |= CMD_INDEX;
			break;
		case MMC_RSP_TYPE_R1b:
		case MMC_RSP_TYPE_R5b:
			val |= 0x3;       //Short response(maybe busy)
			val |= CMD_CRC;
			val |= CMD_INDEX;
			break;
		default:
			break;
	}

	if (cmd->data) {
		val |= CMD_DATA;
	}
	
	/* Issue the command. */
	writew(val, host->base + COMMAND);

	/* Wait for the response. */
	do {
		val = readl(host->base + INT_STATUS);
	} while (!(val & (INT_RESP | INT_TIMEOUT)));

	/* Clear complete bit and error bits.
	 * We will clear these bits later together with those data bits.
	 */
	if (!cmd->data) {
		writel(val, host->base + INT_STATUS);
	}
	
	/* Return 1 when timeout */
	if (val & INT_TIMEOUT) {
		D(DBG_ERR, "CMD Timeout...\n");
		return 1;
	}

	/* Copy response */
	if (cmd->rsp_type == MMC_RSP_TYPE_R2) {
		cmd->response[0] = readl(host->base + CMD_RSP0);
		cmd->response[1] = readl(host->base + CMD_RSP1);
		cmd->response[2] = readl(host->base + CMD_RSP2);
		cmd->response[3] = readl(host->base + CMD_RSP3);
	} else if (cmd->rsp_type == MMC_RSP_TYPE_R1b &&
		   cmd->rsp_type == MMC_RSP_TYPE_R5b) {
		while (!(readl(host->base + PRES_STATE) & PRES_STATE_DATA)); 
		cmd->response[0] = readl(host->base + CMD_RSP0);
	} else if (cmd->rsp_type == MMC_RSP_TYPE_NONE) {
	} else {
		cmd->response[0] = readl(host->base + CMD_RSP0);
	}

	/* Wait for the data transmission to complete. */
	if (cmd->data) {
		do {
			val = readl(host->base + INT_STATUS);
		} while (!(val & (INT_DATA_END | INT_ERROR | INT_DMA_END)));

		writel(val, host->base + INT_STATUS);
		if (val & INT_ERROR) {
			printf("Data transfer error!\n");
		}
	}

	D(DBG_INFO, "CMD_RSP %u: %x %x %x %x\n", cmd->index,
		cmd->response[0], cmd->response[1], cmd->response[2], cmd->response[3]);

	return 0;
}

/** Software Reset */
static void sdhc_reset(struct sdhc *host)
{
	uint32_t val;
	struct mmc_cmd cmd = {.data = NULL};

	/* Reset the host */
	val = SW_RESET_ALL;
	writeb(val, host->base + SW_RESET);
	do {
		val = readb(host->base + SW_RESET);
	} while (val & SW_RESET_ALL);

	/* Enable IRQs */
	val = (INT_ADMA_ERR | INT_BUS_POWER | INT_DATA_END_BIT | INT_DATA_CRC | INT_DATA_TIMEOUT |
		INT_CARD_REMOVE | INT_CARD_INSERT | INT_DATA_AVAIL | INT_SPACE_AVAIL |
		INT_INDEX | INT_END_BIT | INT_CRC | INT_TIMEOUT | INT_DATA_END | INT_RESP);
	writel(val, host->base + INT_ENABLE);
	writel(val, host->base + SIG_ENABLE);

	/* Set clock */
	val = readw(host->base + CLK_CTRL);
	val |= CLK_CTRL_INT_EN;
	writew(val, host->base + CLK_CTRL);
	do {
		val = readw(host->base + CLK_CTRL);
	} while (!(val & CLK_CTRL_INT_STABLE));
	val |= CLK_CTRL_CARD_EN;
	writew(val, host->base + CLK_CTRL);

	/* Wait until the Command and Data Lines are ready. */
	while ((readl(host->base + PRES_STATE) & PRES_STATE_CMD_IHB) ||
		(readl(host->base + PRES_STATE) & PRES_STATE_DATA_IHB));

	/* Check if a SD card is inserted. */
	val = readl(host->base + PRES_STATE);
	if (val & PRES_STATE_CARD) {
		printf("Card Inserted");
		if (!(val & PRES_STATE_WRITE_PROT)) {
			printf("(Read Only)");
		}
		printf("...\n");
		
	} else {
		printf("Card Not Present...\n");
	}

	/* Reset the card with CMD0 */
	cmd.index = MMC_GO_IDLE_STATE;
	cmd.arg = 0;
	cmd.rsp_type = MMC_RSP_TYPE_NONE;
	sdhc_send_cmd(host, &cmd);

	/* TODO: review this command. */
	cmd.index = MMC_SEND_EXT_CSD;
	cmd.arg = 0x1AA;
	cmd.rsp_type = MMC_RSP_TYPE_R1;
	sdhc_send_cmd(host, &cmd);
}

/**
 * Card voltage validation.
 */
static void sdhc_voltage_validation(struct sdhc *host)
{
	D(DBG_INFO, "\n");

	int ret;
	int val;
	int voltage;
	struct mmc_cmd cmd = {.data = NULL};
	struct mmc_card *card = host->card;

	/* Send CMD55 to issue an application specific command. */
	cmd.index = MMC_APP_CMD;
	cmd.arg = 0;
	cmd.rsp_type = MMC_RSP_TYPE_R1;
	ret = sdhc_send_cmd(host, &cmd);
	if (!ret) {
		/* It is a SD card. */
		cmd.index = SD_SD_APP_OP_COND;
		cmd.arg = 0;
		cmd.rsp_type = MMC_RSP_TYPE_R3;
		card->type = CARD_TYPE_SD;
	} else {
		/* It is a MMC card. */
		cmd.index = MMC_SEND_OP_COND;
		cmd.arg = 0;
		cmd.rsp_type = MMC_RSP_TYPE_R3;
		card->type = CARD_TYPE_MMC;
	}
	ret = sdhc_send_cmd(host, &cmd);
	if (ret) {
		card->type = CARD_TYPE_UNKNOWN;
		/* TODO: Be nicer */
		assert(0);
	}
	card->ocr = cmd.response[0];

	/* TODO: Check uSDHC compatibility */
	voltage = MMC_VDD_29_30 | MMC_VDD_30_31;
	val = readl(host->base + HOST_CAP);
	if ((val & HOST_CAP_VS33) && (card->ocr & voltage)) {
		/* Voltage compatible */
		voltage |= (1 << 30);
		voltage |= (1 << 25);
		voltage |= (1 << 24);
	}

	/* Wait until the voltage level is set. */
	do {
		if (card->type == CARD_TYPE_SD) {
			cmd.index = MMC_APP_CMD;
			cmd.arg = 0;
			cmd.rsp_type = MMC_RSP_TYPE_R1;
			sdhc_send_cmd(host, &cmd);
		}

		cmd.index = SD_SD_APP_OP_COND;
		cmd.arg = voltage;
		cmd.rsp_type = MMC_RSP_TYPE_R3;
		sdhc_send_cmd(host, &cmd);
		udelay(100000);
	} while (!(cmd.response[0] & (1U << 31)));
	card->ocr = cmd.response[0];

	/* Check CCS bit */
	if (card->ocr & (1 << 30)) {
		card->high_capacity = 1;
	} else {
		card->high_capacity = 0;
	}

	D(DBG_INFO, "Voltage set!\n");
}

/**
 * MMC/SD/SDIO card registry.
 */
static void sdhc_card_registry(struct sdhc *host)
{
	D(DBG_INFO, "\n");

	int ret;
	struct mmc_cmd cmd = {.data = NULL};
	struct mmc_card *card = host->card;

	/* Get card ID */
	cmd.index = MMC_ALL_SEND_CID;
	cmd.arg = 0;
	cmd.rsp_type = MMC_RSP_TYPE_R2;
	ret = sdhc_send_cmd(host, &cmd);
	if (ret) {
		D(DBG_ERR, "No response!\n");
		card->status = CARD_STS_INACTIVE;
		return;
	} else {
		card->status = CARD_STS_ACTIVE;
	}

	/* Left shift the response by 8. Consult SDHC manual. */
	cmd.response[3] = ((cmd.response[3] << 8) | (cmd.response[2] >> 24));
	cmd.response[2] = ((cmd.response[2] << 8) | (cmd.response[1] >> 24));
	cmd.response[1] = ((cmd.response[1] << 8) | (cmd.response[0] >> 24));
	cmd.response[0] = (cmd.response[0] << 8);
	memcpy(card->raw_cid, cmd.response, sizeof(card->raw_cid));


	/* Retrieve RCA number. */
	cmd.index = MMC_SEND_RELATIVE_ADDR;
	cmd.arg = 0;
	cmd.rsp_type = MMC_RSP_TYPE_R6;
	sdhc_send_cmd(host, &cmd);
	card->raw_rca = (cmd.response[0] >> 16);
	D(DBG_INFO, "New Card RCA: %x\n", card->raw_rca);

	/* Read CSD, Status */
	cmd.index = MMC_SEND_CSD;
	cmd.arg = card->raw_rca << 16;
	cmd.rsp_type = MMC_RSP_TYPE_R2;
	sdhc_send_cmd(host, &cmd);

	/* Left shift the response by 8. Consult SDHC manual. */
	cmd.response[3] = ((cmd.response[3] << 8) | (cmd.response[2] >> 24));
	cmd.response[2] = ((cmd.response[2] << 8) | (cmd.response[1] >> 24));
	cmd.response[1] = ((cmd.response[1] << 8) | (cmd.response[0] >> 24));
	cmd.response[0] = (cmd.response[0] << 8);
	memcpy(card->raw_csd, cmd.response, sizeof(card->raw_csd));

	cmd.index = MMC_SEND_STATUS;
	cmd.rsp_type = MMC_RSP_TYPE_R1;
	sdhc_send_cmd(host, &cmd);

	/* Select the card */
	cmd.index = MMC_SELECT_CARD;
	cmd.arg = card->raw_rca << 16;
	cmd.rsp_type = MMC_RSP_TYPE_R1b;
	sdhc_send_cmd(host, &cmd);

	/* Set Bus width */
	cmd.index = MMC_APP_CMD;
	cmd.arg = card->raw_rca << 16;
	cmd.rsp_type = MMC_RSP_TYPE_R1;
	sdhc_send_cmd(host, &cmd);
	cmd.index = SD_SET_BUS_WIDTH;
	sdhc_send_cmd(host, &cmd);
}

int sdhc_card_block_write(struct mmc_card *card, struct mmc_data *data)
{
	D(DBG_INFO, "\n");

	int ret;
	struct mmc_cmd cmd;
	struct sdhc* sdhc = _mmc_get_sdhc(card);
	cmd.data = data;

	/* Start transfer */
	cmd.index = MMC_WRITE_BLOCK;

	if (card->high_capacity) {
		cmd.arg = data->data_addr;
	} else {
		cmd.arg = data->data_addr * data->block_size;
	}

	cmd.rsp_type = MMC_RSP_TYPE_R1;
	ret = sdhc_send_cmd(sdhc, &cmd);
	if (ret) {
		D(DBG_INFO, "Write single block error.\n");
	}

	D(DBG_INFO, "Write complete...\n");
	return data->block_size * data->blocks;
}

int sdhc_card_block_read(struct mmc_card *card, struct mmc_data *data)
{
	D(DBG_INFO, "\n");

	int ret;
	struct mmc_cmd cmd = {.data = data};
    	struct sdhc* sdhc = _mmc_get_sdhc(card);

	/* Start transfer */
	cmd.index = MMC_READ_SINGLE_BLOCK;

	if (card->high_capacity) {
		cmd.arg = data->data_addr;
	} else {
		cmd.arg = data->data_addr * data->block_size;
	}

	cmd.rsp_type = MMC_RSP_TYPE_R1;
	ret = sdhc_send_cmd(sdhc, &cmd);
	D(DBG_INFO, "\n");
	if (ret) {
		D(DBG_INFO, "Read single block error.\n");
	}

	D(DBG_INFO, "Read complete...\n");
	return data->block_size * data->blocks;
}

sdhc_dev_t
sdhc_plat_init(int id, mmc_card_t card,
               struct dma_allocator* dma_allocator,
               struct ps_io_mapper* o)
{
	sdhc_dev_t sdhc;
    void* iobase;
    switch(id){
    case 0: iobase = RESOURCE(o, SDMMC0); break;
    case 1: iobase = RESOURCE(o, SDMMC1); break;
    case 2: iobase = RESOURCE(o, SDMMC2); break;
    case 3: iobase = RESOURCE(o, SDMMC3); break;
    case 4: iobase = RESOURCE(o, SDMMC4); break;
    default:
        return NULL;
    }

    sdhc = (sdhc_dev_t)malloc(sizeof(*sdhc));
	if (!sdhc) {
		D(DBG_ERR, "Not enough memory!\n");
		assert(0);
	}

	if (!card) {
		D(DBG_ERR, "Invalid MMC card structure!\n");
		free(sdhc);
		sdhc = NULL;
		assert(0);
	}
	sdhc->card = card;
	sdhc->base = iobase;
	sdhc->dalloc = dma_allocator;
	_mmc_set_sdhc(sdhc->card, sdhc);

	sdhc_reset(sdhc);
	sdhc_voltage_validation(sdhc);
	sdhc_card_registry(sdhc);

    return sdhc;
}

void sdhc_interrupt(void)
{
	D(DBG_INFO, "SDHC intr fired ...\n");
}

int
plat_sdhc_default_id(void){
    return 2;
}

