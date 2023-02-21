/*****************************************************************************
* File Name: BleApplications.c
*
* Version: 1.0
*
* Description:
* This file implements the BLE capability.
*
* Hardware Dependency:
* CY8CKIT-042 BLE Pioneer Kit
*
******************************************************************************
* Copyright (2014), Cypress Semiconductor Corporation.
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

/*****************************************************************************
* Included headers
*****************************************************************************/
#include <main.h>
#include <BLEApplications.h>

/*************************Variables Declaration*************************************************************************/
uint8 StartAdvertisement = FALSE; //This flag is used to start advertisement
uint8 DeviceConnected = FALSE; //This flag is set when a Central device is connected
uint8 BLEStackStatus = FALSE; //Variable store the BLE stack status
uint8 ConnectionParametersUpdateRequired = TRUE; //This flag is used to send the connection parameters update
uint8 CapSenseNotificationEnabled = FALSE; //This flag is set when the Central device writes to CCCD to enable temperature notification
uint8 UpdateCapSenseNotificationAttribute = FALSE; //This flags is used to update the respective temperature CCCD value
uint8 CapSenseNotificationCCCDValue[0x02];
extern int32 previousLevelPercent;
extern int32 levelPercent;

/*****************************************************************************
* Static variables 
*****************************************************************************/
static CYBLE_CONN_HANDLE_T ConnectionHandle; //This handle stores the connection parameters
static CYBLE_GATTS_WRITE_REQ_PARAM_T *WriteRequestedParameter; //Variable to store the data received as part of the Write request event
static CYBLE_GATTS_HANDLE_VALUE_NTF_T CapSenseNotificationHandle;
static CYBLE_GATT_HANDLE_VALUE_PAIR_T CapSenseNotificationCCCDHandle; //This handle is used to update the temperature CCCD
static CYBLE_GAP_CONN_UPDATE_PARAM_T ConnectionParametersHandle = {CONN_PARAM_UPDATE_MIN_CONN_INTERVAL, CONN_PARAM_UPDATE_MAX_CONN_INTERVAL,
    CONN_PARAM_UPDATE_SLAVE_LATENCY, CONN_PARAM_UPDATE_SUPRV_TIMEOUT}; //Connection Parameter update values
/***********************************************************************************************************************/


/*************************************************************************************************************************
* Function Name: CustomEventHandler
**************************************************************************************************************************
* Summary: This is a call back event function to handle various events from BLE stack.
*
* Parameters:
*  Event - Event returned
*  EventParameter - Link to value of the event returned
*
* Return:
*  void
*
*************************************************************************************************************************/
void CustomEventHandler(uint32 Event, void *EventParameter)
{
    switch(Event)
	{
        /**********************************************************
        *                       General Events
        ***********************************************************/
		case CYBLE_EVT_STACK_ON: //This event is received when the BLE component is started			
            //StartAdvertisement = TRUE; //Set the advertisement flag
		break;
            
        case CYBLE_EVT_STACK_BUSY_STATUS: //This event is generated when the internal stack buffer is full and no more data can be accepted or the stack has buffer available and can accept data
            BLEStackStatus = *(uint8*)EventParameter; //Extract the BLE stack status
        break;
        
        /**********************************************************
        *                       GAP Events
        ***********************************************************/		
        case CYBLE_EVT_GAPP_ADVERTISEMENT_START_STOP: //This event is received when the device starts or stops advertising
            if(CyBle_GetState() == CYBLE_STATE_DISCONNECTED)
            {
                //StartAdvertisement = TRUE; //Set the advertisement flag
            }
        break;
			
		case CYBLE_EVT_GAP_DEVICE_DISCONNECTED: //This event is received when the device is disconnected
			//StartAdvertisement = TRUE; //Set the advertisement flag
        break;

        /**********************************************************
        *                       GATT Events
        ***********************************************************/
        case CYBLE_EVT_GATT_CONNECT_IND: //This event is received when the device is connected over GATT level
			ConnectionHandle = *(CYBLE_CONN_HANDLE_T *)EventParameter; //Update the attribute handle on GATT connection
			
			DeviceConnected = TRUE; //Set device connection status flag
            
        break;
			
        case CYBLE_EVT_GATT_DISCONNECT_IND: //This event is received when device is disconnected
			DeviceConnected = FALSE; //Clear device connection status flag
            CapSenseNotificationEnabled = FALSE;
            UpdateCapSenseNotificationAttribute = TRUE;
            previousLevelPercent = ZERO;
            levelPercent = ZERO;
            ConnectionParametersUpdateRequired = TRUE; //Set the Connection Parameters Update flag
            UpdateNotificationCCCDAttribute(); //Update the CCCD writing by the Central device
		break;
            
        case CYBLE_EVT_GATTS_WRITE_REQ: //When this event is triggered, the peripheral has received a write command on the custom characteristic
			/* Extract the write value from the event parameter */
            WriteRequestedParameter = (CYBLE_GATTS_WRITE_REQ_PARAM_T *)EventParameter;
            
            if(WriteRequestedParameter->handleValPair.attrHandle == cyBle_customs[CAPSENSE_SERVICE_INDEX].\
				customServiceInfo[CAPSENSE_SLIDER_CHAR_INDEX].customServiceCharDescriptors[CAPSENSE_SLIDER_CCC_INDEX])
            {
               CapSenseNotificationEnabled = WriteRequestedParameter->handleValPair.value.val[CCC_DATA_INDEX];
				
				/* Set flag to allow CCCD to be updated for next read operation */
				UpdateCapSenseNotificationAttribute = TRUE;
            }
			    
            /* Send response to the write command received */
			CyBle_GattsWriteRsp(ConnectionHandle);
        break;
            
        /**********************************************************
        *                       L2CAP Events
        ***********************************************************/
            
        case CYBLE_EVT_L2CAP_CONN_PARAM_UPDATE_RSP: //This event is generated when the L2CAP connection parameter update response received
            ConnectionParametersUpdateRequired = FALSE; //Clear the Connection Parameters Update flag
        break;
			
        default:
        break;
	}
}

/*******************************************************************************
* Function Name: SendCapSenseNotification
********************************************************************************
* Summary:
* Send CapSense Slider data as BLE Notifications. This function updates
* the notification handle with data and triggers the BLE component to send 
* notification
*
* Parameters:
*  CapSenseSliderData:	CapSense slider value	
*
* Return:
*  void
*
*******************************************************************************/
void SendCapSenseNotification(uint8 CapSenseSliderData)
{
    
    /* Update notification handle with CapSense slider data*/
    	CapSenseNotificationHandle.attrHandle = CAPSENSE_SLIDER_CHAR_HANDLE;				
    	CapSenseNotificationHandle.value.val = &CapSenseSliderData;
    	CapSenseNotificationHandle.value.len = CAPSENSE_CHAR_DATA_LEN;
    	
    	/* Send notifications. */
    	CyBle_GattsNotification(cyBle_connHandle, &CapSenseNotificationHandle);
   

}


/*************************************************************************************************************************
* Function Name: UpdateNotificationCCCDAttribute
**************************************************************************************************************************
* Summary: This function updates the notification handle status and reports 
* it to BLE component database so that it can be read by Central device.
*
* Parameters:
*  void
*
* Return:
*  void
*
*************************************************************************************************************************/
void UpdateNotificationCCCDAttribute(void)
{
    if(UpdateCapSenseNotificationAttribute)
    {
        UpdateCapSenseNotificationAttribute = FALSE;
        
        /* Write the present CapSense notification status to the local variable */
        CapSenseNotificationCCCDValue[0] = CapSenseNotificationEnabled;
		CapSenseNotificationCCCDValue[1] = 0x00;
        
        /* Update CCCD handle with notification status data*/
		CapSenseNotificationCCCDHandle.attrHandle = CAPSENSE_CCC_HANDLE;
		CapSenseNotificationCCCDHandle.value.val = CapSenseNotificationCCCDValue;
		CapSenseNotificationCCCDHandle.value.len = CCC_DATA_LEN;
        
        /* Send the updated handle as part of attribute for notifications */
    	CyBle_GattsWriteAttributeValue(&CapSenseNotificationCCCDHandle, 0x00, &ConnectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
        
    }
	
}


/*************************************************************************************************************************
* Function Name: UpdateConnectionParameters
**************************************************************************************************************************
* Summary: This function sends the Connection Parameters Update Request to the Central device
* immediately after connected and modifies the connection interval for low power operation.
* The Central device can accept or reject this request based on the Central device configuration.
*
* Parameters:
*  void
*
* Return:
*  void
*
*************************************************************************************************************************/
void UpdateConnectionParameters(void)
{
    if(DeviceConnected && ConnectionParametersUpdateRequired)
    {
        ConnectionParametersUpdateRequired = FALSE; //Clear the Connection Parameters Update flag
        /* Send Connection Parameter Update request with desired parameter values */
        CyBle_L2capLeConnectionParamUpdateRequest(ConnectionHandle.bdHandle, &ConnectionParametersHandle);
    }
}
/***********************************************************************************************************************/


/* [] END OF FILE */
