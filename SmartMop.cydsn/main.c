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
#include "common_bmi270.h"
#include "bmi270.h"
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
const int16 CYCODE eepromEmptyOffset[NUMSENSORS] = {0,1486,1864,2563,2680,1892,1905,1825,1904,2024,2086,884};/* Sensor counts when empty to calculate diff counts. Loaded from EEPROM array */
int16 sensorScale[NUMSENSORS] = {0x01D0, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x01C0}; /* Scaling factor to normalize sensor full scale counts. 0x0100 = 1.0 in fixed precision 8.8 */
int32 sensorProcessed[NUMSENSORS] = {0u, 0u}; /* fixed precision 24.8 */
uint16 sensorLimit = SENSORLIMIT;           /* Threshold for determining if a sensor is submerged. Set to half of SENSORMAX value */
uint8 sensorActiveCount = 0u;               /* Number of sensors currently submerged */
int32 previousLevelPercent = 0u;
int32 levelPercent = 0u;                    /* fixed precision 24.8 */
int32 levelMm = 0u;                         /* fixed precision 24.8 */   
int32 sensorHeight = SENSORHEIGHT;          /* Height of a single sensor. Fixed precision 24.8 */


struct bmi2_sens_config config;
struct bmi2_feat_sensor_data sensor_data = { 0 };
uint8_t sens_list[2] = { BMI2_ACCEL, BMI2_SIG_MOTION };

struct bmi2_dev bmi2_dev;
struct bmi2_sens_int_config sens_int = { .type = BMI2_SIG_MOTION, .hw_int_pin = BMI2_INT1 };
struct bmi2_int_pin_config int_cfg;
int8_t rslt;

static void InitializeSystem(void);
CY_ISR_PROTO(Pin_BMI270);

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
                if(levelPercent != previousLevelPercent)
                {
                    previousLevelPercent = levelPercent;
                    SendCapSenseNotification(levelPercent>>8);
                }
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
		  //ProcessUart();

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
    	
	/* ADD_CODE to initialize CapSense component and initialize baselines*/
	CapSense_CSD_Start();
	CapSense_CSD_ScanEnabledWidgets();
    I2C_Start();
    
    rslt = bmi2_interface_init(&bmi2_dev, BMI2_I2C_INTF);
    
    rslt = bmi270_init(&bmi2_dev);
    
    if(rslt == BMI2_OK){
        
        rslt = bmi270_sensor_enable(sens_list, 2, &bmi2_dev);
        
        if(rslt == BMI2_OK){
            config.type = BMI2_SIG_MOTION;
            rslt = bmi270_get_sensor_config(&config, 1, &bmi2_dev);
            if(rslt == BMI2_OK){
                
                bmi2_get_int_pin_config(&int_cfg, &bmi2_dev);

	            int_cfg.pin_type = BMI2_INT1;
	            int_cfg.pin_cfg[0].lvl = BMI2_INT_ACTIVE_HIGH;/*Config INT1 rising edge trigging*/
	            int_cfg.pin_cfg[0].od = BMI2_INT_PUSH_PULL;
	            int_cfg.pin_cfg[0].output_en= BMI2_INT_OUTPUT_ENABLE;

	            rslt = bmi2_set_int_pin_config(&int_cfg, &bmi2_dev);
                if(rslt == BMI2_OK){
                    bmi270_map_feat_int(&sens_int, 1, &bmi2_dev);
                }
                
            }
        }
        
    }
    
    BMI270_Interrupt_StartEx(Pin_BMI270);
}

CY_ISR(Pin_BMI270)
{
    /* Clear the pending interrupts */
    BMI270_Interrupt_ClearPending();    
    Pin_BMI270_ClearInterrupt();
}


/* [] END OF FILE */

