/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef _SDHCI_H
#define _SDHCI_H

#define DMA_ADDR            0x00 //DMA Address 
#define BLK_SIZE            0x04 //Block Size 
#define BLK_COUNT           0x06 //Block Count
#define ARGUMENT            0x08 //Command Argument 
#define TRANS_MODE          0x0C //Command Transfer Mode 
#define COMMAND             0x0E //Command Index
#define CMD_RSP0            0x10 //Command Response 
#define CMD_RSP1            0x14 //Command Response 
#define CMD_RSP2            0x18 //Command Response 
#define CMD_RSP3            0x1C //Command Response 
#define BUFFER              0x20 //Data Buffer Access Port
#define PRES_STATE          0x24 //Present State  
#define HOST_CTRL           0x28 //First Host Control  
#define PWR_CTRL            0x29 //Power Control
#define BLK_GAP_CTRL        0x2A //Block Gap Control
#define WAKEUP_CTRL         0x2B //Wakeup Event Control
#define CLK_CTRL            0x2C //Clock Control 
#define TIMEOUT_CTRL        0x2E //Timeout Control
#define SW_RESET            0x2F //Software Reset
#define INT_STATUS          0x30 //Interrupt Status 
#define INT_ENABLE          0x34 //Interrupt Enable 
#define SIG_ENABLE          0x38 //Signal Enable 
#define ACMD12_ERR          0x3C //Auto CMD12 Error Status 
#define HOST_CTRL2          0x3E //Second Host Control
#define HOST_CAP            0x40 //First Host Controller Capabilities 
#define HOST_CAP1           0x44 //Second Host Controller Capabilities
#define MAX_CURRENT         0x48 //Max Current Control 
#define SET_ACMD12_ERR      0x50 //Force ACMD12 Error
#define SET_INT_ERR         0x52 //Force Interrupt Error
#define ADMA_ERROR          0x54 //ADMA Error Status Register 
#define ADMA_ADDR           0x58 //ADMA System Address 
#define PRESET_FOR_SDR12    0x66 //TODO: Description Unknown
#define PRESET_FOR_SDR25    0x68 //TODO: Description Unknown
#define PRESET_FOR_SDR50    0x6A //TODO: Description Unknown
#define PRESET_FOR_SDR104   0x6C //TODO: Description Unknown
#define PRESET_FOR_DDR50    0x6E //TODO: Description Unknown
#define SLOT_INT_STATUS     0xFC //TODO: Description Unknown
#define HOST_VERSION        0xFE //Host Version

/* Block Size Register */
#define BLK_SIZE_BOUNDARY_SHF    12     //Blocks Buffer Boundary
#define BLK_SIZE_BOUNDARY_MASK   0x7    //Blocks Buffer Boundary Mask
#define BLK_SIZE_MASK            0xFFF  //Block Size Mask

/* Command Transfer Mode Register */
#define TRANS_MODE_DMA           (1 << 0)
#define TRANS_MODE_BLK_CNT_EN    (1 << 1)
#define TRANS_MODE_AUTO_CMD12    (1 << 2)
#define TRANS_MODE_AUTO_CMD23    (1 << 3)
#define TRANS_MODE_READ          (1 << 4)
#define TRANS_MODE_MULTI         (1 << 5)

/* Command Register */
#define CMD_INDEX_SHF            8        //Opcode shift
#define CMD_RESP_MASK            0x3
#define CMD_CRC                  (1 << 3)
#define CMD_INDEX                (1 << 4)
#define CMD_DATA                 (1 << 5)
#define CMD_ABORT                (3 << 6)

/* Present State Register */
#define PRES_STATE_CMD_IHB       (1 << 0)  //Command Inhibit(CMD)
#define PRES_STATE_DATA_IHB      (1 << 1)  //Command Inhibit(DATA)
#define PRES_STATE_WRITING       (1 << 8)  //Write Transfer Active
#define PRES_STATE_READING       (1 << 9)  //Read Transfer Active
#define PRES_STATE_SPACE_AVAIL   (1 << 10)
#define PRES_STATE_DATA_AVAIL    (1 << 11)
#define PRES_STATE_CARD          (1 << 16) //Card Inserted
#define PRES_STATE_WRITE_PROT    (1 << 19) //Write Protect
#define PRES_STATE_DATA          (1 << 20)
#define PRES_STATE_DATA_LVL_MASK 0xF
#define PRES_STATE_DATA_LVL_SHF  20

/* Host Control Register */
#define HOST_CTRL_LED            (1 << 0)
#define HOST_CTRL_4BITBUS        (1 << 1)
#define HOST_CTRL_HISPD          (1 << 2)
#define HOST_CTRL_DMA_MASK       0x3
#define HOST_CTRL_DMA_SHF        3

/* Wakeup Control Register */
#define WAKEUP_CTRL_INT          (1 << 0)
#define WAKEUP_CTRL_INSERT       (1 << 1)
#define WAKEUP_CTRL_REMOVE       (1 << 2)

/* Clock Control Register */
#define CLK_CTRL_INT_EN          (1 << 0)
#define CLK_CTRL_INT_STABLE      (1 << 1)
#define CLK_CTRL_CARD_EN         (1 << 2)
#define CLK_CTRL_MODE            (1 << 5)

/* Software Reset Register */
#define SW_RESET_ALL             (1 << 0)
#define SW_RESET_CMD             (1 << 1)
#define SW_RESET_DATA            (1 << 2)


/* Interrupt Status/Enable/Signal Register */
#define INT_ADMA_ERR             (1 << 25)
#define INT_ACMD12_ERR           (1 << 24) //Auto CMD12 Error
#define INT_BUS_POWER            (1 << 23)
#define INT_DATA_END_BIT         (1 << 22) //Data End Bit Error
#define INT_DATA_CRC             (1 << 21) //Data CRC Error
#define INT_DATA_TIMEOUT         (1 << 20) //Data Timeout Error
#define INT_INDEX                (1 << 19) //Command Index Error
#define INT_END_BIT              (1 << 18) //Command End Bit Error
#define INT_CRC                  (1 << 17) //Command CRC Error
#define INT_TIMEOUT              (1 << 16) //Command Timeout Error
#define INT_ERROR                (1 << 15)
#define INT_CARD_INT             (1 << 8)  //Card Interrupt
#define INT_CARD_REMOVE          (1 << 7)  //Card Removal
#define INT_CARD_INSERT          (1 << 6)  //Card Insertion
#define INT_DATA_AVAIL           (1 << 5)  //Buffer Read Ready
#define INT_SPACE_AVAIL          (1 << 4)  //Buffer Write Ready
#define INT_DMA_END              (1 << 3)  //DMA Interrupt
#define INT_BLK_GAP              (1 << 2)  //Block Gap Event
#define INT_DATA_END             (1 << 1)  //Transfer Complete
#define INT_RESP                 (1 << 0)  //Command Complete

/* Host Controller Capabilities Register */
#define HOST_CAP_64BIT     (1 << 28)
#define HOST_CAP_VS18      (1 << 26) //Voltage Support 1.8V
#define HOST_CAP_VS30      (1 << 25) //Voltage Support 3.0V
#define HOST_CAP_VS33      (1 << 24) //Voltage Support 3.3V
#define HOST_CAP_SDMA      (1 << 22) //DMA Support
#define HOST_CAP_HISPD     (1 << 21) //High Speed Support
#define HOST_CAP_ADMA1     (1 << 20) //ADMA Support
#define HOST_CAP_ADMA2     (1 << 19) //ADMA Support

#define CAP_MAX_BLK_SHF   16    //Max Block Length
#define CAP_MAX_BLK_MASK  0x3   //Max Block Length

#include "../../sdhc.h"
#include "../../mmc.h"

#endif //_SDHCI_H
