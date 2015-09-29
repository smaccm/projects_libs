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

#include <autoconf.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "services.h"
#include "mmc.h"

#define DBG_INFO "info:"

//#define DEBUG
#undef DEBUG
#ifdef DEBUG
#define D(x, ...) printf(__VA_ARGS__)
#else
#define D(...) do{}while(0)
#endif

#define DS_ADDR               0x00 //DMA System Address 
#define BLK_ATT               0x04 //Block Attributes 
#define CMD_ARG               0x08 //Command Argument 
#define CMD_XFR_TYP           0x0C //Command Transfer Type 
#define CMD_RSP0              0x10 //Command Response0 
#define CMD_RSP1              0x14 //Command Response1 
#define CMD_RSP2              0x18 //Command Response2  
#define CMD_RSP3              0x1C //Command Response3 
#define DATA_BUFF_ACC_PORT    0x20 //Data Buffer Access Port  
#define PRES_STATE            0x24 //Present State  
#define PROT_CTRL             0x28 //Protocol Control  
#define SYS_CTRL              0x2C //System Control 
#define INT_STATUS            0x30 //Interrupt Status 
#define INT_STATUS_EN         0x34 //Interrupt Status Enable 
#define INT_SIGNAL_EN         0x38 //Interrupt Signal Enable 
#define AUTOCMD12_ERR_STATUS  0x3C //Auto CMD12 Error Status 
#define HOST_CTRL_CAP         0x40 //Host Controller Capabilities 
#define WTMK_LVL              0x44 //Watermark Level 
#define MIX_CTRL              0x48 //Mixer Control 
#define FORCE_EVENT           0x50 //Force Event 
#define ADMA_ERR_STATUS       0x54 //ADMA Error Status Register 
#define ADMA_SYS_ADDR         0x58 //ADMA System Address 
#define DLL_CTRL              0x60 //DLL (Delay Line) Control 
#define DLL_STATUS            0x64 //DLL Status 
#define CLK_TUNE_CTRL_STATUS  0x68 //CLK Tuning Control and Status 
#define VEND_SPEC             0xC0 //Vendor Specific Register 
#define MMC_BOOT              0xC4 //MMC Boot Register 
#define VEND_SPEC2            0xC8 //Vendor Specific 2 Register  
#define HOST_VERSION          0xFE //Host Version


/* Block Attributes Register */
#define BLK_ATT_BLKCNT_SHF      16        //Blocks Count For Current Transfer
#define BLK_ATT_BLKCNT_MASK     0xFFFF    //Blocks Count For Current Transfer
#define BLK_ATT_BLKSIZE_SHF     0         //Transfer Block Size
#define BLK_ATT_BLKSIZE_MASK    0xFFF     //Transfer Block Size

/* Command Transfer Type Register */
#define CMD_XFR_TYP_CMDINX_SHF  24        //Command Index
#define CMD_XFR_TYP_CMDINX_MASK 0x3F      //Command Index
#define CMD_XFR_TYP_CMDTYP_SHF  22        //Command Type
#define CMD_XFR_TYP_CMDTYP_MASK 0x3       //Command Type
#define CMD_XFR_TYP_DPSEL       (1 << 21) //Data Present Select
#define CMD_XFR_TYP_CICEN       (1 << 20) //Command Index Check Enable
#define CMD_XFR_TYP_CCCEN       (1 << 19) //Command CRC Check Enable
#define CMD_XFR_TYP_RSPTYP_SHF  16        //Response Type Select
#define CMD_XFR_TYP_RSPTYP_MASK 0x3       //Response Type Select

/* System Control Register */
#define SYS_CTRL_INITA          (1 << 27) //Initialization Active 
#define SYS_CTRL_RSTD           (1 << 26) //Software Reset for DAT Line
#define SYS_CTRL_RSTC           (1 << 25) //Software Reset for CMD Line
#define SYS_CTRL_RSTA           (1 << 24) //Software Reset for ALL
#define SYS_CTRL_DTOCV_SHF      16        //Data Timeout Counter Value
#define SYS_CTRL_DTOCV_MASK     0xF       //Data Timeout Counter Value
#define SYS_CTRL_SDCLKS_SHF     8         //SDCLK Frequency Select
#define SYS_CTRL_SDCLKS_MASK    0xFF      //SDCLK Frequency Select
#define SYS_CTRL_DVS_SHF        4         //Divisor
#define SYS_CTRL_DVS_MASK       0xF       //Divisor
#define SYS_CTRL_CLK_INT_EN     (1 << 0)  //Internal clock enable (exl. IMX6)
#define SYS_CTRL_CLK_INT_STABLE (1 << 1)  //Internal clock stable (exl. IMX6)
#define SYS_CTRL_CLK_CARD_EN    (1 << 2)  //SD clock enable       (exl. IMX6)

/* Present State Register */
#define PRES_STATE_DAT3         (1 << 23)
#define PRES_STATE_DAT2         (1 << 22)
#define PRES_STATE_DAT1         (1 << 21)
#define PRES_STATE_DAT0         (1 << 20)
#define PRES_STATE_WPSPL        (1 << 19) //Write Protect Switch Pin Level
#define PRES_STATE_CDPL         (1 << 18) //Card Detect Pin Level
#define PRES_STATE_CINST        (1 << 16) //Card Inserted
#define PRES_STATE_BWEN         (1 << 10) //Buffer Write Enable
#define PRES_STATE_RTA          (1 << 9)  //Read Transfer Active
#define PRES_STATE_WTA          (1 << 8)  //Write Transfer Active
#define PRES_STATE_SDSTB        (1 << 3)  //SD Clock Stable
#define PRES_STATE_DLA          (1 << 2)  //Data Line Active
#define PRES_STATE_CDIHB        (1 << 1)  //Command Inhibit(DATA)
#define PRES_STATE_CIHB         (1 << 0)  //Command Inhibit(CMD)

/* Interrupt Status Register */
#define INT_STATUS_DMAE         (1 << 28) //DMA Error            (only IMX6)
#define INT_STATUS_TNE          (1 << 26) //Tuning Error
#define INT_STATUS_ADMAE        (1 << 25) //ADMA error           (exl. IMX6)
#define INT_STATUS_AC12E        (1 << 24) //Auto CMD12 Error
#define INT_STATUS_OVRCURE      (1 << 23) //Bus over current     (exl. IMX6)
#define INT_STATUS_DEBE         (1 << 22) //Data End Bit Error
#define INT_STATUS_DCE          (1 << 21) //Data CRC Error
#define INT_STATUS_DTOE         (1 << 20) //Data Timeout Error
#define INT_STATUS_CIE          (1 << 19) //Command Index Error
#define INT_STATUS_CEBE         (1 << 18) //Command End Bit Error
#define INT_STATUS_CCE          (1 << 17) //Command CRC Error
#define INT_STATUS_CTOE         (1 << 16) //Command Timeout Error
#define INT_STATUS_ERR          (1 << 15) //Error interrupt      (exl. IMX6)
#define INT_STATUS_TP           (1 << 14) //Tuning Pass
#define INT_STATUS_RTE          (1 << 12) //Re-Tuning Event
#define INT_STATUS_CINT         (1 << 8)  //Card Interrupt
#define INT_STATUS_CRM          (1 << 7)  //Card Removal
#define INT_STATUS_CINS         (1 << 6)  //Card Insertion
#define INT_STATUS_BRR          (1 << 5)  //Buffer Read Ready
#define INT_STATUS_BWR          (1 << 4)  //Buffer Write Ready
#define INT_STATUS_DINT         (1 << 3)  //DMA Interrupt
#define INT_STATUS_BGE          (1 << 2)  //Block Gap Event
#define INT_STATUS_TC           (1 << 1)  //Transfer Complete
#define INT_STATUS_CC           (1 << 0)  //Command Complete

/* Host Controller Capabilities Register */
#define HOST_CTRL_CAP_VS18      (1 << 26) //Voltage Support 1.8V
#define HOST_CTRL_CAP_VS30      (1 << 25) //Voltage Support 3.0V
#define HOST_CTRL_CAP_VS33      (1 << 24) //Voltage Support 3.3V
#define HOST_CTRL_CAP_SRS       (1 << 23) //Suspend/Resume Support
#define HOST_CTRL_CAP_DMAS      (1 << 22) //DMA Support
#define HOST_CTRL_CAP_HSS       (1 << 21) //High Speed Support
#define HOST_CTRL_CAP_ADMAS     (1 << 20) //ADMA Support
#define HOST_CTRL_CAP_MBL_SHF   16        //Max Block Length
#define HOST_CTRL_CAP_MBL_MASK  0x7       //Max Block Length

/* Mixer Control Register */
#define MIX_CTRL_MSBSEL         (1 << 5)  //Multi/Single Block Select.
#define MIX_CTRL_DTDSEL         (1 << 4)  //Data Transfer Direction Select.
#define MIX_CTRL_DDR_EN         (1 << 3)  //Dual Data Rate mode selection
#define MIX_CTRL_AC12EN         (1 << 2)  //Auto CMD12 Enable
#define MIX_CTRL_BCEN           (1 << 1)  //Block Count Enable
#define MIX_CTRL_DMAEN          (1 << 0)  //DMA Enable

/* Watermark Level register */
#define WTMK_LVL_WR_WML_SHF     16        //Write Watermark Level
#define WTMK_LVL_RD_WML_SHF     0         //Write Watermark Level

#define writel(v, a)  (*(volatile uint32_t*)(a) = (v))
#define readl(a)      (*(volatile uint32_t*)(a))


/** Print uSDHC registers. */
UNUSED static void
print_sdhc_regs(struct sdhc *host)
{
    int i;
    for (i = DS_ADDR; i <= HOST_VERSION; i += 0x4) {
        printf("%x: %X\n", i, readl(host->base + i));
    }
}


/** Return the default SDHC interface ID for the platform
 * @return the device ID of the default SDHC interface for the
 *         running platform.
 */
enum sdhc_id
sdhc_default_id(void)
{
    return sdhc_plat_default_id();
}


/** Pass control to the devices IRQ handler
 * @param[in] sd_dev  The sdhc interface device that triggered 
 *                    the interrupt event.
 */
static int
sdhc_handle_irq(sdhc_dev_t sd_dev, int irq)
{
    (void)sd_dev;
    D(DBG_INFO, "SDHC intr fired ...\n");
    return 0;
}

static int
priv_sdhc_handle_irq(void* sdhc_priv, int irq)
{
    sdhc_dev_t sdhc = (sdhc_dev_t)sdhc_priv;
    return sdhc_handle_irq(sdhc, irq);
}

static int
sdhc_send_cmd(sdhc_dev_t host, struct mmc_cmd *cmd, sdhc_cb cb, void* token)
{
    uint32_t val;

    writel(0xffffffff, host->base + INT_STATUS);

    /* Check if the Host is ready for transit. */
    while ((readl(host->base + PRES_STATE) & PRES_STATE_CIHB) ||
        (readl(host->base + PRES_STATE) & PRES_STATE_CDIHB));
    while (readl(host->base + PRES_STATE) & PRES_STATE_DLA);

    /* Two commands need to have at least 8 clock cycles in between. */
    udelay(1000);

    /* Write to the argument register. */
    D(DBG_INFO, "CMD: %d with arg %x ", cmd->index, cmd->arg);
    writel(cmd->arg, host->base + CMD_ARG);

    if(cmd->data){
        /* Use the default timeout. */
        val = readl(host->base + SYS_CTRL);
        val &= ~(0xffUL << 16);
        val |= 0xE << 16; 
        writel(val, host->base + SYS_CTRL);

        /* Set the DMA boundary. */
        val = (cmd->data->block_size & BLK_ATT_BLKSIZE_MASK);
        val |= (cmd->data->blocks << BLK_ATT_BLKCNT_SHF);
        writel(val, host->base + BLK_ATT);

        /* Set watermark level */
        val = cmd->data->block_size / 4;
        if (val > 0x80) {
            val = 0x80;
        }
        if (cmd->index == MMC_READ_SINGLE_BLOCK) {
            val = (val << WTMK_LVL_RD_WML_SHF);
        }else{
            val = (val << WTMK_LVL_WR_WML_SHF);
        }
        writel(val, host->base + WTMK_LVL);

        /* Set Mixer Control */
        val = MIX_CTRL_BCEN | MIX_CTRL_DMAEN;
        if (cmd->data->blocks > 1) {
            val |= MIX_CTRL_MSBSEL;
        }
        if (cmd->index == MMC_READ_SINGLE_BLOCK) {
            val |= MIX_CTRL_DTDSEL;
        }
        writel(val, host->base + MIX_CTRL);

        /* Set DMA address */
        writel(cmd->data->pbuf, host->base + DS_ADDR);
    }

    /* The command should be MSB and the first two bits should be '00' */
    val = (cmd->index & CMD_XFR_TYP_CMDINX_MASK) << CMD_XFR_TYP_CMDINX_SHF;
    val &= ~(CMD_XFR_TYP_CMDTYP_MASK << CMD_XFR_TYP_CMDTYP_SHF);
    if (cmd->data) {
        /* Some controllers implement MIX_CTRL as part of the XFR_TYP */
        val |= MIX_CTRL_BCEN | MIX_CTRL_DMAEN;
        if (cmd->data->blocks > 1) {
            val |= MIX_CTRL_MSBSEL;
        }
        if (cmd->index == MMC_READ_SINGLE_BLOCK) {
            val |= MIX_CTRL_DTDSEL;
        }
    }

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

    /* Issue the command. */
    writel(val, host->base + CMD_XFR_TYP);

    /* Wait for the response. */
    do {
        val = readl(host->base + INT_STATUS);
    } while (!(val & (INT_STATUS_CC | INT_STATUS_CTOE)));

    /* Clear complete bit and error bits */
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

    /* Wait for the data transmission to complete. */
    if (cmd->data) {
        /* Wait for the transfer completion. */
        D(DBG_INFO, "Waiting for read transaction completion\n");
        do {
            val = readl(host->base + INT_STATUS);
        } while (!(val & (INT_STATUS_TC | INT_STATUS_ERR | INT_STATUS_DINT)));
        writel(val, host->base + INT_STATUS);
        D(DBG_INFO, "Read transaction completed\n");

        /* Check CRC error. */
        if (val & INT_STATUS_DTOE) {
            D(DBG_INFO, "Data transfer error.\n");
        }
        if (val & INT_STATUS_ERR) {
            printf("Data transfer error!\n");
        }

        D(DBG_INFO, "Read/write complete...\n");
    }
    return 0;
}

static int
priv_sdhc_send_cmd(void* sdhc_priv, struct mmc_cmd *cmd, sdhc_cb cb, void* token)
{
    sdhc_dev_t sdhc = (sdhc_dev_t)sdhc_priv;
    return sdhc_send_cmd(sdhc, cmd, cb, token);
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
    ret = sdhc_send_cmd(host, &cmd, NULL, NULL);
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
    ret = sdhc_send_cmd(host, &cmd, NULL, NULL);
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
            sdhc_send_cmd(host, &cmd, NULL, NULL);
        }

        cmd.index = SD_SD_APP_OP_COND;
        cmd.arg = voltage;
        cmd.rsp_type = MMC_RSP_TYPE_R3;
        sdhc_send_cmd(host, &cmd, NULL, NULL);
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
    ret = sdhc_send_cmd(host, &cmd, NULL, NULL);
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
    sdhc_send_cmd(host, &cmd, NULL, NULL);
    card->raw_rca = (cmd.response[0] >> 16);
    D(DBG_INFO, "New Card RCA: %x\n", card->raw_rca);

    /* Read CSD, Status */
    cmd.index = MMC_SEND_CSD;
    cmd.arg = card->raw_rca << 16;
    cmd.rsp_type = MMC_RSP_TYPE_R2;
    sdhc_send_cmd(host, &cmd, NULL, NULL);

    /* Left shift the response by 8. Consult SDHC manual. */
    cmd.response[3] = ((cmd.response[3] << 8) | (cmd.response[2] >> 24));
    cmd.response[2] = ((cmd.response[2] << 8) | (cmd.response[1] >> 24));
    cmd.response[1] = ((cmd.response[1] << 8) | (cmd.response[0] >> 24));
    cmd.response[0] = (cmd.response[0] << 8);
    memcpy(card->raw_csd, cmd.response, sizeof(card->raw_csd));

    cmd.index = MMC_SEND_STATUS;
    cmd.rsp_type = MMC_RSP_TYPE_R1;
    sdhc_send_cmd(host, &cmd, NULL, NULL);

    /* Select the card */
    cmd.index = MMC_SELECT_CARD;
    cmd.arg = card->raw_rca << 16;
    cmd.rsp_type = MMC_RSP_TYPE_R1b;
    sdhc_send_cmd(host, &cmd, NULL, NULL);

    /* Set Bus width */
    cmd.index = MMC_APP_CMD;
    cmd.arg = card->raw_rca << 16;
    cmd.rsp_type = MMC_RSP_TYPE_R1;
    sdhc_send_cmd(host, &cmd, NULL, NULL);
    cmd.index = SD_SET_BUS_WIDTH;
    sdhc_send_cmd(host, &cmd, NULL, NULL);
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

    /* Enable IRQs */
    val = ( INT_STATUS_ADMAE | INT_STATUS_OVRCURE | INT_STATUS_DEBE
          | INT_STATUS_DCE   | INT_STATUS_DTOE    | INT_STATUS_CRM
          | INT_STATUS_CINS  | INT_STATUS_BRR     | INT_STATUS_BWR
          | INT_STATUS_CIE   | INT_STATUS_CEBE    | INT_STATUS_CCE
          | INT_STATUS_CTOE  | INT_STATUS_TC      | INT_STATUS_CC);
    writel(val, host->base + INT_STATUS_EN);
    writel(val, host->base + INT_SIGNAL_EN);

    /* Set clock */
    val = readl(host->base + SYS_CTRL);
    val |= SYS_CTRL_CLK_INT_EN;
    writel(val, host->base + SYS_CTRL);
    do {
        val = readl(host->base + SYS_CTRL);
    } while (!(val & SYS_CTRL_CLK_INT_STABLE));
    val |= SYS_CTRL_CLK_CARD_EN;
    writel(val, host->base + SYS_CTRL);

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
    while(readl(host->base + SYS_CTRL) & SYS_CTRL_INITA);

    /* Check if a SD card is inserted. */
    val = readl(host->base + PRES_STATE);
    if (val & PRES_STATE_CINST) {
        printf("Card Inserted");
        if (!(val & PRES_STATE_WPSPL)) {
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
    sdhc_send_cmd(host, &cmd, NULL, NULL);


    /* TODO: review this command. */
    cmd.index = MMC_SEND_EXT_CSD;
    cmd.arg = 0x1AA;
    cmd.rsp_type = MMC_RSP_TYPE_R1;
    sdhc_send_cmd(host, &cmd, NULL, NULL);

}



sdhc_dev_t
sdhc_init(enum sdhc_id id, mmc_card_t card, ps_io_ops_t* io_ops)
{
    sdhc_dev_t sdhc;
    int err;
    /* Check that a card structure was provided */
    if (!card) {
        LOG_ERROR("Invalid MMC card structure!\n");
        return NULL;
    }
    /* Allocate memory for SDHC structure */
    sdhc = (sdhc_dev_t)malloc(sizeof(*sdhc));
    if (!sdhc) {
        LOG_ERROR("Not enough memory!\n");
        return NULL;
    }
    /* Call platform initialisation code */
    err = sdhc_plat_init(id, io_ops, sdhc);
    if(err){
        LOG_ERROR("SDHC platform specific initialisation failed\n");
        return NULL;
    }
    /* Complete the initialisation of the SDHC structure */
    sdhc->card = card;
    sdhc->dalloc = &io_ops->dma_manager;
    card->handle_irq = &priv_sdhc_handle_irq;
    card->send_command = &priv_sdhc_send_cmd;
    card->host = sdhc;
    /* Initialise the card */
    sdhc_reset(sdhc);
    sdhc_voltage_validation(sdhc);
    sdhc_card_registry(sdhc);
    return sdhc;
}


