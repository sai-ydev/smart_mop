/**
 * Copyright (C) 2021 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <I2C_I2C.h>
#include "common_bmi270.h"
#include "bmi2_defs.h"

/******************************************************************************/
/*!                 Macro definitions                                         */
#define BMI2XY_SHUTTLE_ID  UINT16_C(0x1B8)

/*! Macro that defines read write length */
#define READ_WRITE_LEN     UINT8_C(46)

/******************************************************************************/
/*!                Static variable definition                                 */

/*! Variable that holds the I2C device address or SPI chip selection */
static uint8_t dev_addr;

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to COINES platform
 */
BMI2_INTF_RETURN_TYPE bmi2_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t dev_addr = *(uint8_t*)intf_ptr;
    uint8_t I2CWriteBuffer[0x01] = {reg_addr}; //Location from which the calibration data is to be read
    
    I2C_I2CMasterWriteBuf(dev_addr, I2CWriteBuffer, sizeof(I2CWriteBuffer), I2C_I2C_MODE_COMPLETE_XFER);
    while(!(I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT)); //Wait till the master completes writing
    I2C_I2CMasterClearStatus(); //Clear I2C master status
    
    int result = I2C_I2CMasterReadBuf(dev_addr, reg_data, len, I2C_I2C_MODE_COMPLETE_XFER);
    while(!(I2C_I2CMasterStatus() & I2C_I2C_MSTAT_RD_CMPLT)); //Wait till the master completes reading
    I2C_I2CMasterClearStatus(); //Clear I2C master status

    return result;
    
}

/*!
 * I2C write function map to COINES platform
 */
BMI2_INTF_RETURN_TYPE bmi2_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t dev_addr = *(uint8_t*)intf_ptr;
    uint8 I2CWriteBuffer[len + 0x01]; 
    I2CWriteBuffer[0] = reg_addr; 
    memcpy(&I2CWriteBuffer[1], reg_data, len);

    int result = I2C_I2CMasterWriteBuf(dev_addr, I2CWriteBuffer, sizeof(I2CWriteBuffer), I2C_I2C_MODE_COMPLETE_XFER);
    while(!(I2C_I2CMasterStatus() & I2C_I2C_MSTAT_WR_CMPLT)); //Wait till the master completes writing
    I2C_I2CMasterClearStatus(); //Clear I2C master status
    
    return result;       
}


/*!
 * Delay function map to COINES platform
 */
void bmi2_delay_us(uint32_t period, void *intf_ptr)
{
   CyDelayUs(period);
}

/*!
 *  @brief Function to select the interface between SPI and I2C.
 *  Also to initialize coines platform
 */
int8_t bmi2_interface_init(struct bmi2_dev *bmi, uint8_t intf)
{
    int8_t rslt = BMI2_OK;
    

    if (bmi != NULL)
    {
       

#if defined(PC)
        setbuf(stdout, NULL);
#endif

        /* Bus configuration : I2C */
        if (intf == BMI2_I2C_INTF)
        {
            //printf("I2C Interface \n");

            /* To initialize the user I2C function */
            dev_addr = BMI2_I2C_PRIM_ADDR;
            bmi->intf = BMI2_I2C_INTF;
            bmi->read = bmi2_i2c_read;
            bmi->write = bmi2_i2c_write;
        }
        

        /* Assign device address to interface pointer */
        bmi->intf_ptr = &dev_addr;

        /* Configure delay in microseconds */
        bmi->delay_us = bmi2_delay_us;

        /* Configure max read/write length (in bytes) ( Supported length depends on target machine) */
        bmi->read_write_len = READ_WRITE_LEN;

        /* Assign to NULL to load the default config file. */
        bmi->config_file_ptr = NULL;
    }
    else
    {
        rslt = BMI2_E_NULL_PTR;
    }

    return rslt;

}




