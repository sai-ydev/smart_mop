/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include <project.h>


/* Function prototypes */
void StoreCalibration(void);
void ProcessUprobe(void);
void InitialUartMessage(void);
void CalUartCommands(void);
void CalUartMessage(void);
void TestUartMessage(void);
void ProcessUart(void);
void TxIntNumber(int32 number, int8 leadingZeros);
void TxIntFixedNumber(int32 number, uint8 fixedShift, uint8 numDecimal);
cystatus Em_EEPROM_Write(const uint8 srcBuf[], const uint8 eepromPtr[], uint32 byteCount);


/* Project Constants */
/* uProbe constants */  
#define NUMSAMPLES          (20u)          /* Number of sensor levels to test */
/* UART constants */
#define UART_DELAY          (500u)         /* Delay in ms to control data logging rate */
#define UART_NONE           (0u)
#define UART_BASIC          (1u)
#define UART_CSVINIT        (2u)
#define UART_CSV            (3u)
/* [] END OF FILE */
