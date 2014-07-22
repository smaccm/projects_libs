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

#include "sdhc.h"
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
#define readl(a)      (*(volatile uint32_t*)(a))

#define SDHC1_PADDR 0x02190000
#define SDHC2_PADDR 0x02194000
#define SDHC3_PADDR 0x02198000
#define SDHC4_PADDR 0x0219C000

#define SDHC1_SIZE  0x1000
#define SDHC2_SIZE  0x1000
#define SDHC3_SIZE  0x1000
#define SDHC4_SIZE  0x1000

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
	for (int i = DS_ADDR; i <= VEND_SPEC2; i += 0x4) {
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

	/* The command should be MSB and the first two bits should be '00' */
	val = (cmd->index & CMD_XFR_TYP_CMDINX_MASK) << CMD_XFR_TYP_CMDINX_SHF;

	/* Check if the Host is ready for transit. */
	while ((readl(host->base + PRES_STATE) & PRES_STATE_CDIHB) ||
		(readl(host->base + PRES_STATE) & PRES_STATE_CIHB));
	while (readl(host->base + PRES_STATE) & PRES_STATE_DLA);

	/* Two commands need to have at least 8 clock cycles in between. */
	udelay(1000);

	/* TODO: set CMDTYP, DPSEL, DTDSEL */
	val &= ~(CMD_XFR_TYP_CMDTYP_MASK << CMD_XFR_TYP_CMDTYP_SHF);

	/* Set response type */
	val &= ~CMD_XFR_TYP_CICEN;
	val &= ~CMD_XFR_TYP_CCCEN;
	val &= ~(CMD_XFR_TYP_RSPTYP_MASK << CMD_XFR_TYP_RSPTYP_SHF);
	switch (cmd->rsp_type) {
		case MMC_RSP_TYPE_R2:
			val |= (0x1 << CMD_XFR_TYP_RSPTYP_SHF);
			val |= CMD_XFR_TYP_CCCEN;
			break;
		case MMC_RSP_TYPE_R3:
		case MMC_RSP_TYPE_R4:
			val |= (0x2 << CMD_XFR_TYP_RSPTYP_SHF);
			break;
		case MMC_RSP_TYPE_R1:
		case MMC_RSP_TYPE_R5:
		case MMC_RSP_TYPE_R6:
			val |= (0x2 << CMD_XFR_TYP_RSPTYP_SHF);
			val |= CMD_XFR_TYP_CICEN;
			val |= CMD_XFR_TYP_CCCEN;
			break;
		case MMC_RSP_TYPE_R1b:
		case MMC_RSP_TYPE_R5b:
			val |= (0x3 << CMD_XFR_TYP_RSPTYP_SHF);
			val |= CMD_XFR_TYP_CICEN;
			val |= CMD_XFR_TYP_CCCEN;
			break;
		default:
			break;
	}

	if (cmd->data) {
		val |= CMD_XFR_TYP_DPSEL;
	}

	/* Write to the argument register. */
	D(DBG_INFO, "CMD: %d with arg %x", cmd->index, cmd->arg);
	writel(cmd->arg, host->base + CMD_ARG);

	/* Issue the command. */
	writel(val, host->base + CMD_XFR_TYP);

	/* Wait for the response. */
	do {
		val = readl(host->base + INT_STATUS);
	} while (!(val & (INT_STATUS_CC | INT_STATUS_CTOE)));

	/* TODO: Check error bits. */
	/* Clear CC bit and error bits. */
	writel(val, host->base + INT_STATUS);

	/* Return 1 when timeout */
	if (val & INT_STATUS_CTOE) {
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
		   cmd->index == MMC_STOP_TRANSMISSION) {
		cmd->response[3] = readl(host->base + CMD_RSP3);
	} else if (cmd->rsp_type == MMC_RSP_TYPE_NONE) {
	} else {
		cmd->response[0] = readl(host->base + CMD_RSP0);
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
	val = readl(host->base + SYS_CTRL);
	val |= SYS_CTRL_RSTA;
	writel(val, host->base + SYS_CTRL);

	do {
		val = readl(host->base + SYS_CTRL);
	} while (val & SYS_CTRL_RSTA);

	/* Set Clock
	 * TODO: Hard-coded clock freq based on a *198MHz* default input.
	 */
	/* make sure the clock state is stable. */
	if (readl(host->base + PRES_STATE) & PRES_STATE_SDSTB) {
		val = readl(host->base + SYS_CTRL);

		/* The SDCLK bit varies with Data Rate Mode. */
		if (readl(host->base + MIX_CTRL) & MIX_CTRL_DDR_EN) {
			val &= ~(SYS_CTRL_SDCLKS_MASK << SYS_CTRL_SDCLKS_SHF);
			val |= (0x80 << SYS_CTRL_SDCLKS_SHF);
			val &= ~(SYS_CTRL_DVS_MASK << SYS_CTRL_DVS_SHF);
			val |= (0x0 << SYS_CTRL_DVS_SHF);
		} else {
			val &= ~(SYS_CTRL_SDCLKS_MASK << SYS_CTRL_SDCLKS_SHF);
			val |= (0x80 << SYS_CTRL_SDCLKS_SHF);
			val &= ~(SYS_CTRL_DVS_MASK << SYS_CTRL_DVS_SHF);
			val |= (0x1 << SYS_CTRL_DVS_SHF);
		}

		/* Set data timeout value */
		val |= (0xE << SYS_CTRL_DTOCV_SHF);
		writel(val, host->base + SYS_CTRL);
	} else {
		D(DBG_ERR, "The clock is unstable, unable to change it!\n");
	}

	/* TODO: Select Voltage Level */

	/* Wait until the Command and Data Lines are ready. */
	while ((readl(host->base + PRES_STATE) & PRES_STATE_CDIHB) ||
		(readl(host->base + PRES_STATE) & PRES_STATE_CIHB));

	/* Send 80 clock ticks to card to power up. */
	val = readl(host->base + SYS_CTRL);
	val |= SYS_CTRL_INITA;
	writel(val, host->base + SYS_CTRL);

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
	val = readl(host->base + HOST_CTRL_CAP);
	if ((val & HOST_CTRL_CAP_VS33) && (card->ocr & voltage)) {
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
	uint32_t val;
	struct mmc_cmd cmd;
	struct sdhc* sdhc = _mmc_get_sdhc(card);
	cmd.data = data;

	/* Set the same length and number of blocks to the uSDHC register. */
	val = data->block_size;
	val |= (data->blocks << BLK_ATT_BLKCNT_SHF);
	writel(val, sdhc->base + BLK_ATT);

	/* Set watermark level */
	val = data->block_size / 4;
	if (val > 0x80) {
		val = 0x80;
	}
	val = (val << WTMK_LVL_WR_WML_SHF);
	writel(val, sdhc->base + WTMK_LVL);

	/* Set Mixer Control */
	val = readl(sdhc->base + MIX_CTRL);
	if (data->blocks > 1) {
		val |= MIX_CTRL_MSBSEL;
	} else {
		val &= ~MIX_CTRL_MSBSEL;
	}
	val &= ~MIX_CTRL_DTDSEL;
	val |= MIX_CTRL_DMAEN;
	writel(val, sdhc->base + MIX_CTRL);

	/* Disable the buffer write ready interrupt. */

	/* Configure DMA, and AC12EN bit. */
	writel(dma_paddr(data->dma_buf), sdhc->base + DS_ADDR);

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

	/* Wait for the transfer completion. */
	do {
		val = readl(sdhc->base + INT_STATUS);
	} while (!(val & (INT_STATUS_TC | INT_STATUS_DTOE)));

	/* Check CRC error. */
	if (val & INT_STATUS_DTOE) {
		D(DBG_INFO, "Data transfer error.\n");
	}

	D(DBG_INFO, "Write complete...\n");
	return data->block_size * data->blocks;
}

int sdhc_card_block_read(struct mmc_card *card, struct mmc_data *data)
{
	D(DBG_INFO, "\n");

	int ret;
	uint32_t val;
	struct mmc_cmd cmd = {.data = data};
    	struct sdhc* sdhc = _mmc_get_sdhc(card);

	/* Set the same length and number of blocks to the uSDHC register. */
	val = data->block_size;
	val |= (data->blocks << BLK_ATT_BLKCNT_SHF);
	writel(val, sdhc->base + BLK_ATT);
	D(DBG_INFO, "\n");

	/* Set watermark level */
	val = data->block_size / 4;
	if (val > 0x80) {
		val = 0x80;
	}
	val = (val << WTMK_LVL_RD_WML_SHF);
	writel(val, sdhc->base + WTMK_LVL);
	D(DBG_INFO, "\n");

	/* Set Mixer Control */
	val = readl(sdhc->base + MIX_CTRL);
	if (data->blocks > 1) {
		val |= MIX_CTRL_MSBSEL;
	} else {
		val &= ~MIX_CTRL_MSBSEL;
	}
	val |= MIX_CTRL_DTDSEL;
	val |= MIX_CTRL_DMAEN;
	writel(val, sdhc->base + MIX_CTRL);
	D(DBG_INFO, "\n");

	/* Disable the buffer write ready interrupt. */

	/* Configure DMA, and AC12EN bit. */
	writel(dma_paddr(data->dma_buf), sdhc->base + DS_ADDR);
	D(DBG_INFO, "\n");

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

	/* Wait for the transfer completion. */
	do {
		val = readl(sdhc->base + INT_STATUS);
	} while (!(val & (INT_STATUS_TC | INT_STATUS_DTOE)));

	/* Check CRC error. */
	if (val & INT_STATUS_DTOE) {
		D(DBG_INFO, "Data transfer error.\n");
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
    case 1: iobase = RESOURCE(o, SDHC1); break;
    case 2: iobase = RESOURCE(o, SDHC2); break;
    case 3: iobase = RESOURCE(o, SDHC3); break;
    case 4: iobase = RESOURCE(o, SDHC4); break;
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

	(void)print_sdhc_regs;
    return sdhc;
}

void sdhc_interrupt(void)
{
	D(DBG_INFO, "SDHC intr fired ...\n");
}

int
plat_sdhc_default_id(void){
    return 4;
}

