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

/*************************Macro Definitions**********************************/
#define LED_DELAY_COUNT 0x32 //Counter value for LED Delay
#define NOTIFICATION_DELAY_COUNT 0x20 //Counter value for Notification Delay
/*****************************************************************************/

/*****************************************************************************
* MACRO Definitions
*****************************************************************************/   

/* Refresh interval in milliseconds for fast scan mode */
#define LOOP_TIME_FASTSCANMODE          (10u)

/* Refresh interval in milliseconds for slow scan mode */
#define LOOP_TIME_SLOWSCANMODE          (500u)

#define MILLI_SEC_TO_MICRO_SEC          (1000u)

#if (!CY_IP_SRSSV2)
    /* ILO frequency for PSoC 4 S-Series device */
    #define ILO_CLOCK_FACTOR            (40u)
#else
    /* ILO frequency for PSoC 4 device */
    #define ILO_CLOCK_FACTOR            (32u)
#endif

/* Refresh rate control parameters */    
#define WDT_TIMEOUT_FAST_SCAN           (ILO_CLOCK_FACTOR * LOOP_TIME_FASTSCANMODE)   
#define WDT_TIMEOUT_SLOW_SCAN           (ILO_CLOCK_FACTOR * LOOP_TIME_SLOWSCANMODE)


/* This timeout is for changing the refresh interval from fast to slow rate
*  The timeout value is WDT_TIMEOUT_FAST_SCAN * SCANMODE_TIMEOUT_VALUE */
#define SCANMODE_TIMEOUT_VALUE          (150u)  

/* Finite state machine states for device operating states */
typedef enum
{
    SENSOR_SCAN = 0x01u, /* Sensor is scanned in this state */
    WAIT_FOR_SCAN_COMPLETE = 0x02u, /* CPU is put to sleep in this state */
    PROCESS_DATA = 0x03u, /* Sensor data is processed */
    BLE_PROCESS = 0x04u,
    SLEEP = 0x05u /* Device is put to deep sleep */
} DEVICE_STATE;

/* Global variables */
volatile uint8 wdtInterruptOccured = FALSE;

volatile uint32 watchdogMatchValue = WDT_TIMEOUT_FAST_SCAN;
/* Global variables used because this is the method uProbe uses to access firmware data */
/* CapSense tuning variables */
uint8 modDac = SENSOR_MODDAC;               /* Modulation DAC current setting */
uint8 compDac[NUMSENSORS] = {SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC, SENSOR_CMPDAC}; /* Compensation DAC current setting */
uint8 senseDivider = SENSOR_SENDIV;         /* Sensor clock divider */
uint8 modDivider = SENSOR_MODDIV;           /* Modulation clock divider */
uint8 NotificationDelayCounter = NOTIFICATION_DELAY_COUNT; //Counter to handle the periodic notification
uint8 LEDDelayCounter = LED_DELAY_COUNT; //Counter to handle the LED blinking
extern long LastCapSenseData;
extern uint8 StartAdvertisement; //This flag is used to start advertisement
extern uint8 DeviceConnected; //This flag is set when a Central device is connected
extern uint8 CapSenseNotificationEnabled; //This flag is set when the Central device writes to CCCD to enable temperature notification
extern uint8 CapSenseNotificationData; //The temperature notification value is stored in this array

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
/* This variable is used to generate required WDT interrupt period */ 
uint32 ILODelayCycles = WDT_MATCH_VALUE_200MS;

struct bmi2_sens_config config;
struct bmi2_feat_sensor_data sensor_data = { 0 };
uint8_t sens_list[2] = { BMI2_ACCEL, BMI2_SIG_MOTION };

struct bmi2_dev bmi2_dev;
struct bmi2_sens_int_config sens_int = { .type = BMI2_SIG_MOTION, .hw_int_pin = BMI2_INT1 };
struct bmi2_int_pin_config int_cfg;
int8_t rslt;
uint8 i;

static void InitializeSystem(void);
void ReadSensorDataAndNotify(void);
void HandleStatusLED(void);
void WDT_Start(uint32 *wdtMatchValFastMode, uint32 *wdtMatchValSlowMode);
uint32 CalibrateWdtMatchValue(void);


CY_ISR_PROTO(Pin_BMI270);
CY_ISR_PROTO (WDT_ISR);

int main()
{   
    InitializeSystem();
    
    /* Compensated Watchdog match value in fast scan mode */
    uint32 wdtMatchValFastMode = 0u;

    /* Compensated Watchdog match value in slow scan mode */
    uint32 wdtMatchValSlowMode = 0u;
    
    /* Variable to store interrupt state */
    uint8 interruptState = 0u;
    
    /* Variable to hold the current device state 
    *  State machine starts with sensor scan state after power-up
    */
    DEVICE_STATE currentState = SENSOR_SCAN; 
    
    
     
    WDT_Start(&wdtMatchValFastMode, &wdtMatchValSlowMode);
    
    while(1u)
    {
        CyBle_ProcessEvents();
        HandleStatusLED();
        switch(currentState){
            case SENSOR_SCAN:
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

            	  //ProcessUart();
                  currentState = WAIT_FOR_SCAN_COMPLETE;
                  
                }
                break;
            case WAIT_FOR_SCAN_COMPLETE:
                interruptState = CyEnterCriticalSection();
                
                /* Check if CapSense scanning is complete */
                if(!CapSense_CSD_IsBusy())
                {
                    /* If CapSense scanning is in progress, put CPU to sleep */
                    CySysPmSleep();
                }
                /* If CapSense scanning is complete, process the CapSense data */
                else
                {
                    currentState = PROCESS_DATA;
                }
                /* Enable interrupts for servicing ISR */
                CyExitCriticalSection(interruptState);
                break;
            case PROCESS_DATA:
                
                   	
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
                
                
                currentState = BLE_PROCESS;
        	   
                break;
                
            case BLE_PROCESS:
                currentState = SLEEP;
                interruptState = CyEnterCriticalSection();
                
                if(StartAdvertisement)
                {
                    StartAdvertisement = FALSE; //Clear the advertisement flag
                    
                    CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST); //Start advertisement and enter Discoverable mode
                    
                    LED_Write(ZERO); //Turn on the status LED
                    LED_SetDriveMode(LED_DM_STRONG); //Set the LED pin drive mode to Strong
                    
                }
        
                if(DeviceConnected)
                {
                    
                    UpdateConnectionParameters();
                    UpdateNotificationCCCDAttribute();
                    
                    
                    if(CapSenseNotificationEnabled)
                    {
                    
                        if(previousLevelPercent != levelPercent)
                        {
                            previousLevelPercent = levelPercent;
                            SendCapSenseNotification(levelPercent>>8);
                        }
                        
                        
                    }
                    
                    
                    
                }
                CyExitCriticalSection(interruptState);
                break;
            
            case SLEEP:
                CySysPmSleep();
                
                if(wdtInterruptOccured)
                {
                    /* Set state to scan sensor after device wakes up from sleep */
                    currentState = SENSOR_SCAN;
                    
                    wdtInterruptOccured = FALSE;  
                }
                
                break;
            default:
                CYASSERT(0);
                break;
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

/*************************************************************************************************************************
* Function Name: HandleStatusLED
**************************************************************************************************************************
* Summary: This function controls the status LED depending upon the BLE state.
*
* Parameters:
*  void
*
* Return:
*  void
*
*************************************************************************************************************************/
void HandleStatusLED(void)
{
    /* Check whether the device is advertising and blink the LED */
    if(CyBle_GetState() == CYBLE_STATE_ADVERTISING)
    {
        if(--LEDDelayCounter == ZERO)
        {
            LEDDelayCounter = LED_DELAY_COUNT; //Reset the LED state counter
            LED_Write(!LED_Read()); //Toggle the status LED when the counter is reset
        }
    }    
    else
    {
        LED_Write(ONE); //Turn off the status LED
        LED_SetDriveMode(LED_DM_ALG_HIZ); //Set the LED pin drive mode to Hi-Z
    }
}



CY_ISR(Pin_BMI270)
{
    /* Clear the pending interrupts */
    BMI270_Interrupt_ClearPending();    
    Pin_BMI270_ClearInterrupt();
    StartAdvertisement = TRUE;
}

/******************************************************************************
* Function Name: WDT_Interrupt
*******************************************************************************
* Summary:
*  The main function performs the following actions:
*   1. Clears the interrupt
*   2. The WDT match value is updated depending on the device
*   3. Flag is set to indicate that WDT interrupt was triggered
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(Timer_Interrupt)
{
    #if (CY_IP_SRSSV2)    
        /* Clears interrupt request  */
        CySysWdtClearInterrupt(CY_SYS_WDT_COUNTER0_INT);
    #else
        /* Clear the watchdog interrupt */
        CySysWdtClearInterrupt();    
        
        /* WDT match value is updated in order to obtain periodic interrupts */
        CySysWdtWriteMatch(CySysWdtReadMatch() + watchdogMatchValue); 
    #endif /* !CY_IP_SRSSV2 */
    
    /* Set to variable that indicates that WDT interrupt had triggered*/
    wdtInterruptOccured = TRUE;   
}

/******************************************************************************
* Function Name: WDT_Start
*******************************************************************************
* Summary:
*  The main function performs the following actions:
*   1. Link the ISR to a function
*   2. Set match values for WDT delay
*   3. Configure WDT to needed specification
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*
*******************************************************************************/
void WDT_Start(uint32 *wdtMatchValFastMode, uint32 *wdtMatchValSlowMode)
{   
    /* Setup ISR */
    WDT_Interrupt_StartEx(Timer_Interrupt);
    
    /* Get the actual match value required to generate a given delay */    
    watchdogMatchValue = WDT_TIMEOUT_SLOW_SCAN;
    *wdtMatchValSlowMode = CalibrateWdtMatchValue();
    
    watchdogMatchValue = WDT_TIMEOUT_FAST_SCAN;
    *wdtMatchValFastMode = CalibrateWdtMatchValue();
    
    #if (CY_IP_SRSSV2)    
        /* Configure Match value */
        CySysWdtWriteMatch(CY_SYS_WDT_COUNTER0, *wdtMatchValSlowMode);
        
        /* Set up WDT mode */
        CySysWdtSetMode(CY_SYS_WDT_COUNTER0, CY_SYS_WDT_MODE_INT);
        
        /* Automatically reset counter */
        CySysWdtSetClearOnMatch(CY_SYS_WDT_COUNTER0, TRUE);
        CySysWdtResetCounters(CY_SYS_WDT_COUNTER0_RESET);
        
        /* Enable WDT */
        CySysWdtEnable(CY_SYS_WDT_COUNTER0_MASK);
    #else 
        /* WDT match value is updated in order to obtain periodic interrupts */
        CySysWdtWriteMatch(CySysWdtReadMatch() + watchdogMatchValue);
        
        /* Enable WDT interrupt */
        CySysWdtUnmaskInterrupt();
        
    #endif
}

/*******************************************************************************
* Function Name: CalibrateWdtMatchValue
********************************************************************************
* Summary: 
*  The main function performs the following actions:
*   1. Calculate the desired delay in microseconds
*   2. Complete an ILO trimming to improve accuracy
*   3. Update watchdogMatchValue with the current ILO count
*
* Parameter:
*  None
*
* Return:
*  void
*
*******************************************************************************/
uint32 CalibrateWdtMatchValue(void)
{    
    /* Contains ILO Trimmed value */
    uint32 tempIloCounts = 0u;
    
    /* Desired delay in microseconds for ILO Trimming */
    uint32 desiredDelay = ((watchdogMatchValue / ILO_CLOCK_FACTOR) * MILLI_SEC_TO_MICRO_SEC);  
    
    /* Get the ILO compensated counts i.e. the actual counts for the desired ILO frequency 
    *  Trimming is done to improve ILO accuracy using IMO; ILO default accuracy is +/- 60% */
    if(CYRET_SUCCESS == CySysClkIloCompensate(desiredDelay, &tempIloCounts))
    {    
        watchdogMatchValue = (uint32)tempIloCounts;
    }
    return watchdogMatchValue;
}
/* [] END OF FILE */

