/*****************************************************************************
* File Name: main.h
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
#include <BLEApplications.h>
#include "CyFlash.h"


/* Function prototypes */
cystatus Em_EEPROM_Write(const uint8 srcBuf[], const uint8 eepromPtr[], uint32 byteCount);

/* Project Constants */
/* CapSense tuning constants */
#define SENSOR_MODDAC       (150u)          /* Modulation DAC current setting */
#define SENSOR_CMPDAC       (20u)           /* Compensation DAC current setting */
#define SENSOR_SENDIV       (9u)          /* Sensor clock divider */
#define SENSOR_MODDIV       (9u)          /* Modulation clock divider */
/* Liquid Level constants */
#define NUMSENSORS          (12u)          /* Number of CapSense sensors */
#define SENSORMAX           (600u)         /* Maximum difference count of each calibrated sensor at full level */
#define SENSORLIMIT         (SENSORMAX / 2) /* Threshold for determining if a sensor is submerged. Set to half of SENSORMAX value */
#define LEVELMM_MAX         (153u)         /* Max sensor height in mm */
#define SENSORHEIGHT        ((LEVELMM_MAX * 256) / (NUMSENSORS - 1)) /* Height of a single sensor. Fixed precision 24.8 */
/*****************************************************************************
* Macros 
*****************************************************************************/
#define CAPSENSE_ENABLED
#define BLE_ENABLED

#define RED_INDEX						(0)
#define GREEN_INDEX						(1)
#define BLUE_INDEX						(2)
#define INTENSITY_INDEX					(3)

#define TRUE							(1)
#define FALSE							(0)
#define ZERO							(0)
#define ONE                             (1)

#define RGB_LED_MAX_VAL					(255)
#define RGB_LED_OFF						(255)
#define RGB_LED_ON						(0)
/* EEPROM constants */
#define Em_EEPROM_FLASH_BASE_ADDR        (CYDEV_FLASH_BASE)
#define Em_EEPROM_FLASH_SIZE             (CYDEV_FLASH_SIZE)
#define Em_EEPROM_FLASH_END_ADDR         (Em_EEPROM_FLASH_BASE_ADDR + Em_EEPROM_FLASH_SIZE)
#define Em_EEPROM_ROWS_IN_ARRAY          (CY_FLASH_SIZEOF_ARRAY/CY_FLASH_SIZEOF_ROW)

/* Adjust WDT time */
#define WDT_MATCH_VALUE_30MS		(32 * 30) //30ms (IL0~32K)
#define WDT_MATCH_VALUE_200MS		(32 * 200) //200ms (IL0~32K)

#define SLEEP_30MS 0x01
#define SLEEP_200MS 0x02

/* [] END OF FILE */
