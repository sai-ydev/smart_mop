/*****************************************************************************
* File Name: main.c
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
#include <interface.h>
#include <main.h>


/* Global variables */
/* Global variables used because this is the method uProbe uses to access firmware data */
/* CapSense tuning variables */
uint8 modDac = SENSOR_MODDAC;               /* Modulation DAC current setting */
uint8 compDac[NUMSENSORS] = {SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC}; /* Compensation DAC current setting */
uint8 senseDivider = SENSOR_SENDIV;         /* Sensor clock divider */
uint8 modDivider = SENSOR_MODDIV;           /* Modulation clock divider */
/* Liquid Level variables */
int32 sensorRaw[NUMSENSORS] = {0u};         /* Sensor raw counts */
int32 sensorDiff[NUMSENSORS] = {0u};        /* Sensor difference counts */
int16 sensorEmptyOffset[NUMSENSORS] = {0u}; /* Sensor counts when empty to calculate diff counts. Loaded from EEPROM array */
const int16 CYCODE eepromEmptyOffset[] = {0u};/* Sensor counts when empty to calculate diff counts. Loaded from EEPROM array */
int16 sensorScale[NUMSENSORS] = {0x01D0, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x01C0}; /* Scaling factor to normalize sensor full scale counts. 0x0100 = 1.0 in fixed precision 8.8 */
int32 sensorProcessed[NUMSENSORS] = {0u, 0u}; /* fixed precision 24.8 */
uint16 sensorLimit = SENSORLIMIT;           /* Threshold for determining if a sensor is submerged. Set to half of SENSORMAX value */
uint8 sensorActiveCount = 0u;               /* Number of sensors currently submerged */
int32 levelPercent = 0u;                    /* fixed precision 24.8 */
int32 levelMm = 0u;                         /* fixed precision 24.8 */   
int32 sensorHeight = SENSORHEIGHT;          /* Height of a single sensor. Fixed precision 24.8 */
uint8 calFlag = FALSE;                      /* Flag to signal when new sensor calibration values should be stored to EEPROM */
/* Communication variables */
uint16 delayMs = UART_DELAY;                /* Main loop delay in ms to control UART data log output speed */

static void InitializeSystem(void);
static void HandleCapSenseSlider(void);

int main()
{   
    InitializeSystem();
    uint8 i;
    while(1u)
    {
        CyBle_ProcessEvents();
        
        if(TRUE == deviceConnected)
		{
            /* When the Client Characteristic Configuration descriptor (CCCD) is
             * written by Central device for enabling/disabling notifications, 
             * then the same descriptor value has to be explicitly updated in 
             * application so that it reflects the correct value when the 
             * descriptor is read */
			UpdateNotificationCCCD();
			
			/* Send CapSense Slider data when respective notification is enabled */
			if(TRUE == sendCapSenseSliderNotifications)
			{
				/* Check for CapSense slider swipe and send data accordingly */
				SendCapSenseNotification(levelPercent);
			}
		}

        
        /* Can do other main loop stuff here */
        /* Check for CapSense scan complete*/
	    if(CapSense_CSD_IsBusy() == FALSE)
	    {
            /* Read and store new sensor raw counts*/
		    for(i = 0; i < NUMSENSORS; i++)
            {
			  sensorRaw[i] = CapSense_CSD_ReadSensorRaw(i);
			  sensorDiff[i] = sensorRaw[i];
		    }

		  /* Start scan for next iteration */
		  CapSense_CSD_ScanEnabledWidgets();
		
		  /* Check if we should store new empty offset calibration values */
		  if(calFlag == TRUE)
		  {
			  calFlag = FALSE;
			  StoreCalibration();
		  }
		
		  /* Remove empty offset calibration from sensor raw counts and normalize sensor full count values */
		  for(i = 0; i < NUMSENSORS; i++)
		  {
			  sensorDiff[i] -= sensorEmptyOffset[i];
			  sensorProcessed[i] = (sensorDiff[i] * sensorScale[i]) >> 8;
		  }
					
		  /* Find the number of submerged sensors */
		  sensorActiveCount = 0;
		  for(i = 0; i < NUMSENSORS; i++)
		  {
			  if(sensorProcessed[i] > sensorLimit)
			  {
				  /* First and last sensor are half the height of middle sensors */
				  if((i == 0) || (i == NUMSENSORS - 1))
				  {
					  sensorActiveCount += 1;
				  }
				  /* Middle sensors are twice the height of 1st and last sensors */
				  else
				  {
					  sensorActiveCount += 2;
				  }
			  }
		  }
		
		  /* Calculate liquid level height in mm */
		  levelMm = sensorActiveCount * (sensorHeight >> 1);
		  /* If level is near full value then round to full. Avoids fixed precision rounding errors */
		  if(levelMm > ((int32)LEVELMM_MAX << 8) - (sensorHeight >> 2))
		  {
			  levelMm = LEVELMM_MAX << 8;
		  }

		  /* Calculate level percent. Stored in fixed precision 24.8 format to hold fractional percent */
		  levelPercent = (levelMm * 100) / LEVELMM_MAX;
		
		  /* Report level and process uProbe and UART interfaces */
		  ProcessUprobe();
		  ProcessUart();

	    }
        
    }
}



void InitializeSystem(void)
{
	/* Enable global interrupt mask */
	CyGlobalIntEnable; 
		
	/* Start BLE component and register the CustomEventHandler function. This 
	 * function exposes the events from BLE component for application use */
    CyBle_Start(CustomEventHandler);
    
    /* Read stored empty offset values from EEPROM */
    for(uint8 i = 0; i < NUMSENSORS; i++)
    {
        sensorEmptyOffset[i] = eepromEmptyOffset[i];
    }
    
	/* Start both the PrISM components for LED control*/
    PRS_1_Start();
    PRS_2_Start();
	
	/* The RGB LED on BLE Pioneer kit are active low. Drive HIGH on 
	 * pin for OFF and drive LOW on pin for ON*/
	PRS_1_WritePulse0(RGB_LED_OFF);
	PRS_1_WritePulse1(RGB_LED_OFF);
	PRS_2_WritePulse0(RGB_LED_OFF);
	
	/* Set Drive mode of output pins from HiZ to Strong */
	RED_SetDriveMode(RED_DM_STRONG);
	GREEN_SetDriveMode(GREEN_DM_STRONG);
	BLUE_SetDriveMode(BLUE_DM_STRONG);
	
	/* ADD_CODE to initialize CapSense component and initialize baselines*/
	CapSense_CSD_Start();
	CapSense_CSD_ScanEnabledWidgets();
    UART_Start();
    InitialUartMessage();
    
}

void HandleCapSenseSlider(void)
{
    
}
/* [] END OF FILE */

