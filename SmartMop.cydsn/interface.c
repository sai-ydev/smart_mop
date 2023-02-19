/*****************************************************************************
* File Name: interface.c
*
* Version: 1.00
*
* Description: Demonstrates Liquid Level Sensing (LLS) using CapSense CSD with 12 sensors.
*
* Related Document: Code example CE202479
*
* Hardware Dependency: See code example CE202479
*
******************************************************************************
* Copyright (2015), Cypress Semiconductor Corporation.
******************************************************************************
* This software is owned by Cypress Semiconductor Corporation (Cypress) and is
* protected by and subject to worldwide patent protection (United States and
* foreign), United States copyright laws and international treaty provisions.
* Cypress hereby grants to licensee a personal, non-exclusive, non-transferable
* license to copy, use, modify, create derivative works of, and compile the
* Cypress Source Code and derivative works for the sole purpose of creating
* custom software in support of licensee product to be used only in conjunction
* with a Cypress integrated circuit as specified in the applicable agreement.
* Any reproduction, modification, translation, compilation, or representation of
* this software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH
* REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* Cypress reserves the right to make changes without further notice to the
* materials described herein. Cypress does not assume any liability arising out
* of the application or use of any product or circuit described herein. Cypress
* does not authorize its products for use as critical components in life-support
* systems where a malfunction or failure may reasonably be expected to result in
* significant injury to the user. The inclusion of Cypress' product in a life-
* support systems application implies that the manufacturer assumes all risk of
* such use and in doing so indemnifies Cypress against all charges. Use may be
* limited by and subject to the applicable Cypress software license agreement.
*****************************************************************************/
#include <project.h>
#include <main.h>
#include <interface.h>


/* Global variables */
/* Global variables used because this is the method uProbe uses to access firmware data */
/* uProbe variables */
uint8 storeSampleFlag = FALSE;
uint8 resetSampleFlag = FALSE;
int16 arrayAxisLabel[NUMSAMPLES] = {-5,0,10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,153,160,0};
/* UART variables */
uint8 uartTxMode = UART_BASIC;
/* External globals */
extern uint8 modDac;
extern uint8 compDac[];
extern uint8 senseDivider;
extern uint8 modDivider;
extern int16 sensorEmptyOffset[];
extern uint8 storeSampleFlag;
extern uint8 resetSampleFlag;
extern uint8 uartTxMode;
extern int16 arrayAxisLabel[];
extern int32 levelPercent;                 
extern int32 levelMm; 
extern int32 sensorRaw[];
extern int32 sensorDiff[];
extern int32 sensorProcessed[];
extern uint8 calFlag;
extern int16 eepromEmptyOffset[];
extern uint8 sensorActiveCount;




/*******************************************************************************
* Function Name: ProcessUprobe
********************************************************************************/
/* Check if storage of current raw and level values to array has been commanded.            */
/* Set CapSense tuning parameters based on current global variable values.                  */
/* For more information on the CapSense_CSD functions called by this function, please       */
/* refer to the CapSense component datasheet and the PSoC 4 CapSense Tuning Guide document. */
void ProcessUprobe(void)
{
    uint8 i;
    
    /* Update CapSense scan settings from uProbe*/
    for(i = 0; i < NUMSENSORS; i++)
    {
        CapSense_CSD_SetSenseClkDivider(i, senseDivider);
        modDivider = senseDivider;      /* Divider values should be the same */
        CapSense_CSD_SetModulatorClkDivider(i, modDivider);
        CapSense_CSD_SetModulationIDAC(i, modDac);
        CapSense_CSD_SetCompensationIDAC(i, compDac[i]);
    }
        
}



/*******************************************************************************
* Function Name: Em_EEPROM_Write
********************************************************************************
*
* Summary:
*  Writes the specified number of bytes from the source buffer in SRAM to the
*  emulated EEPROM array in flash, without modifying other data in flash.
*
* Parameters:
*  srcBuf:    Pointer to the SRAM buffer holding the data to write.
*  eepromPtr: Pointer to the array or variable in flash representing
*             the emulated EEPROM.
*  byteCount: Number of bytes to write from srcBuf to eepromPtr.
*
* Return:
*    CYRET_SUCCESS     Successful write.
*    CYRET_BAD_PARAM   eepromPtr is out of range of flash memory.
*    CYRET_UNKNOWN     Other error in writing flash.
*
*******************************************************************************/
cystatus Em_EEPROM_Write(const uint8 srcBuf[], const uint8 eepromPtr[], uint32 byteCount)
{
    uint8 writeBuffer[CY_FLASH_SIZEOF_ROW];
    uint32 arrayId;
    uint32 rowId;
    uint32 dstIndex;
    uint32 srcIndex;
    cystatus rc;
    uint32 eeOffset;
    uint32 byteOffset;
    
    eeOffset = (uint32)eepromPtr;

    /* Make sure, that eepromPtr[] points to ROM */
    if (((uint32)eepromPtr + byteCount) < Em_EEPROM_FLASH_END_ADDR)
    {
        arrayId = eeOffset / CY_FLASH_SIZEOF_ARRAY;
        rowId = (eeOffset / CY_FLASH_SIZEOF_ROW) % Em_EEPROM_ROWS_IN_ARRAY;
        byteOffset = (CY_FLASH_SIZEOF_ARRAY * arrayId) + (CY_FLASH_SIZEOF_ROW * rowId);
        srcIndex = 0u;

        rc = CYRET_SUCCESS;

        while ((srcIndex < byteCount) && (CYRET_SUCCESS == rc))
        {
            /* Copy data to the write buffer either from the source buffer or from the flash */
            for (dstIndex = 0u; dstIndex < CY_FLASH_SIZEOF_ROW; dstIndex++)
            {
                if ((byteOffset >= eeOffset) && (srcIndex < byteCount))
                {
                    writeBuffer[dstIndex] = srcBuf[srcIndex];
                    srcIndex++;
                }
                else
                {
                    writeBuffer[dstIndex] = CY_GET_XTND_REG8(CYDEV_FLASH_BASE + byteOffset);
                }
                byteOffset++;
            }

            /* Write flash row */
            rc = CySysFlashWriteRow(rowId, writeBuffer);

            /* Go to the next row */
            rowId++;
            #if (CY_FLASH_NUMBER_ARRAYS > 1)
                if (rowId >= Em_EEPROM_ROWS_IN_ARRAY)
                {
                    arrayId++;
                    rowId = 0u;
                }
            #endif /* (CY_FLASH_NUMBER_ARRAYS > 1) */
        }

        /* Flush both Cache and PHUB prefetch buffer */
    }
    else
    {
        rc = CYRET_BAD_PARAM;
    }

    /* Mask return codes from flash, if they are not supported */
    if ((CYRET_SUCCESS != rc) && (CYRET_BAD_PARAM != rc))
    {
        rc = CYRET_UNKNOWN;
    }
    
    return (rc);
}



/* [] END OF FILE */
