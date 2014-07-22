/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _SDHC_H
#define _SDHC_H

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

/* Present State Register */
#define PRES_STATE_CLSL         (1 << 23) //CMD Line Signal Level
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
#define INT_STATUS_DMAE         (1 << 28) //DMA Error
#define INT_STATUS_TNE          (1 << 26) //Tuning Error
#define INT_STATUS_AC12E        (1 << 24) //Auto CMD12 Error
#define INT_STATUS_DEBE         (1 << 22) //Data End Bit Error
#define INT_STATUS_DCE          (1 << 21) //Data CRC Error
#define INT_STATUS_DTOE         (1 << 20) //Data Timeout Error
#define INT_STATUS_CIE          (1 << 19) //Command Index Error
#define INT_STATUS_CEBE         (1 << 18) //Command End Bit Error
#define INT_STATUS_CCE          (1 << 17) //Command CRC Error
#define INT_STATUS_CTOE         (1 << 16) //Command Timeout Error
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

#include "../../sdhc.h"
#include "../../mmc.h"

#endif //_SDHC_H
