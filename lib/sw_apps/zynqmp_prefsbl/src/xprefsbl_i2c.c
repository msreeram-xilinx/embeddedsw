/******************************************************************************
* Copyright (c) 2020 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/*****************************************************************************/
/**
 * @file xprefsbl_i2c.c
 *
 * This file used to Read the Board Name from IIC EEPROM from 0xD0 location, Based
 * on the Board Name it will update the corresponding multiboot offset value.
 *
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  		Date     Changes
 * ----- ---- -------- ---------------------------------------------------------
 * 1.00  Ana     07/02/20 	First release
 *
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "xprefsbl_main.h"

#ifdef XPREFSBL_GET_BOARD_PARAMS
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
#if (XPAR_XIICPS_NUM_INSTANCES == 2U)
#define XPREFSBL_I2C_EEPROM_INDEX (1U)
#else
#define XPREFSBL_I2C_EEPROM_INDEX (0U)
#endif

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/
u8 WriteBuffer[sizeof(AddressType) + XPREFSBL_MAX_SIZE];
XIicPs IicInstance;

/************************** Function Definitions *****************************/
static int XPrefsbl_EepromWriteData(XIicPs *IicInstance, u16 ByteCount);
static int XPrefsbl_MuxInitChannel(u16 MuxIicAddr, u8 WriteBuffer);
static int XPrefsbl_IicPsConfig(u16 DeviceId);

/*****************************************************************************/
/**
* This API gives a XPrefsbl_Delay in microseconds
*
* @param	useconds requested
*
* @return	None
*
*
****************************************************************************/
static void XPrefsbl_Delay(unsigned int useconds)
{
	int i,j;

	for(j = 0U; j < useconds; j++) {
		for(i = 0U; i < 15U; i++) {
			asm("nop");
		}
	}
}

/*****************************************************************************/
/**
 * This function writes a buffer of data to the IIC serial EEPROM.
 *
 * @param	IicInstance Pointer to IIC instance
 * @param	ByteCount contains the number of bytes in the buffer to be
 *			written.
 *
 * @return	XST_SUCCESS if successful else XST_FAILURE.
 *
 * @note	The Byte count should not exceed the page size of the EEPROM as
 *		noted by the constant PAGE_SIZE.
 *
 ******************************************************************************/
static int XPrefsbl_EepromWriteData(XIicPs *IicInstance, u16 ByteCount)
{
	int Status = XST_FAILURE;
	u32 Timeout = XIICPS_POLL_DEFAULT_TIMEOUT_VAL;

	/*
	 * Send the Data.
	 */
	Status = XIicPs_MasterSendPolled(IicInstance, WriteBuffer,
			ByteCount, XPREFSBL_EEPROM_ADDRESS);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_IICPS_MASTER_SEND_POLLED_ERROR;
		goto END;
	}

	/*
	 * Wait until bus is idle to start another transfer.
	 */
	while (Timeout != 0U) {
		if(XIicPs_BusIsBusy(IicInstance) == FALSE ) {
			break;
		}
		XPrefsbl_Delay(XPREFSBL_DELAY);
		Timeout--;
		if(!Timeout) {
			Status = XPREFSBL_IICPS_TIMEOUT;
			goto END;
		}
	}

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function reads data from the IIC serial EEPROM into a specified buffer.
 *
 * @param	BufferPtr contains the address of the data buffer to be filled.
 * @param	ByteCount contains the number of bytes in the buffer to be read.
 *
 * @return	XST_SUCCESS if successful else XST_FAILURE.
 *
 ******************************************************************************/
int XPrefsbl_EepromReadData(u8 *BufferPtr, u16 ReadAddress, u16 ByteCount,
									u32 WrBfrOffset)
{
	int Status = XST_FAILURE;
	AddressType Address = ReadAddress;
	u32 Timeout = XIICPS_POLL_DEFAULT_TIMEOUT_VAL;

	/*
	 * Position the Pointer in EEPROM.
	 */
	WriteBuffer[0U] = (u8)(Address);

	Status = XPrefsbl_EepromWriteData(&IicInstance, WrBfrOffset);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_EEPROM_WRITE_ERROR;
		goto END;
	}
	/*
	 * Receive the Data.
	 */
	Status = XIicPs_MasterRecvPolled(&IicInstance, BufferPtr,
			ByteCount, XPREFSBL_EEPROM_ADDRESS);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_IICPS_MASTER_RECV_POLLED_ERROR;
		goto END;
	}

	/*
	 * Wait until bus is idle to start another transfer.
	 */
	while (Timeout != 0U) {
		if(XIicPs_BusIsBusy(&IicInstance) == FALSE ) {
			break;
		}
		XPrefsbl_Delay(XPREFSBL_DELAY);
		Timeout--;
		if(!Timeout) {
			Status = XPREFSBL_IICPS_TIMEOUT;
			goto END;
		}
	}

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function initializes the IIC MUX to select the required channel.
 *
 * @param	MuxAddress Contains the address of IIC MUX
 * @param 	Channel Contains the channel number.
 *
 * @return	XST_SUCCESS if pass, otherwise XST_FAILURE.
 *
 * @note		None.
 *
 ****************************************************************************/
static int XPrefsbl_MuxInitChannel(u16 MuxIicAddr, u8 Channel)
{
	int Status = XST_FAILURE;
	u8 Buffer = 0U;
	u32 Timeout = XIICPS_POLL_DEFAULT_TIMEOUT_VAL;


	/*
	 * Wait until bus is idle to start another transfer.
	 */
	while (Timeout != 0U) {
		if(XIicPs_BusIsBusy(&IicInstance) == FALSE ) {
			break;
		}
		XPrefsbl_Delay(XPREFSBL_DELAY);
		Timeout--;
		if(!Timeout) {
			Status = XPREFSBL_IICPS_TIMEOUT;
			goto END;
		}
	}

	/*
	 * Send the Data.
	 */
	Status = XIicPs_MasterSendPolled(&IicInstance, &Channel, 1U,
			MuxIicAddr);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_IICPS_MASTER_SEND_POLLED_ERROR;
		goto END;
	}

	Timeout = XIICPS_POLL_DEFAULT_TIMEOUT_VAL;

	/*
	 * Wait until bus is idle to start another transfer.
	 */
	while (Timeout != 0U) {
		if(XIicPs_BusIsBusy(&IicInstance) == FALSE ) {
			break;
		}
		XPrefsbl_Delay(XPREFSBL_DELAY);
		Timeout--;
		if(!Timeout) {
			Status = XPREFSBL_IICPS_TIMEOUT;
			goto END;
		}
	}

	/*
	 * Receive the Data.
	 */
	Status = XIicPs_MasterRecvPolled(&IicInstance, &Buffer, 1U, MuxIicAddr);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_IICPS_MASTER_RECV_POLLED_ERROR;
		goto END;
	}

	Timeout = XIICPS_POLL_DEFAULT_TIMEOUT_VAL;

	/*
	 * Wait until bus is idle to start another transfer.
	 */
	while (Timeout != 0U) {
		if(XIicPs_BusIsBusy(&IicInstance) == FALSE ) {
			break;
		}
		XPrefsbl_Delay(XPREFSBL_DELAY);
		Timeout--;
		if(!Timeout) {
			Status = XPREFSBL_IICPS_TIMEOUT;
			goto END;
		}
	}

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function perform the initial configuration for the IICPS Device.
 *
 * @param	DeviceId instance.
 *
 * @return	XST_SUCCESS if pass, otherwise XST_FAILURE.
 *
 ****************************************************************************/
static int XPrefsbl_IicPsConfig(u16 DeviceId)
{
	int Status = XST_FAILURE;
	XIicPs_Config *ConfigPtr;	/* Pointer to configuration data */

	/*
	 * Initialize the IIC driver so that it is ready to use.
	 */
	ConfigPtr = XIicPs_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		Status = XPREFSBL_IICPS_LKP_CONFIG_ERROR;
		goto END;
	}

	Status = XIicPs_CfgInitialize(&IicInstance, ConfigPtr,
			ConfigPtr->BaseAddress);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_IICPS_CONFIG_INIT_ERROR;
		goto END;
	}
	/*
	 * Set the IIC serial clock rate.
	 */
	Status = XIicPs_SetSClk(&IicInstance, XPREFSBL_IIC_SCLK_RATE);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_IICPS_SET_SCLK_ERROR;
		goto END;
	}

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function is use to configure the iic device and Init the mux channel
 *
 * @param	None
 *
 * @return	XST_SUCCESS if successful and also update the epprom slave
 * device address in addr variable else XST_FAILURE.
 *
 * @note		None.
 *
 ******************************************************************************/
int XPrefsbl_IicPsMuxInit(void)
{
	int Status = XST_FAILURE;
	u8 MuxChannel = XPREFSBL_I2C_MUX_INDEX;
	u16 DeviceId = XPREFSBL_I2C_EEPROM_INDEX;

	Status = XPrefsbl_IicPsConfig(DeviceId);
	if (Status != XST_SUCCESS) {
		Status = XPREFSBL_IICPS_CONFIG_ERROR;
		goto END;
	}

	Status = XPrefsbl_MuxInitChannel(XPREFSBL_MUX_ADDR, MuxChannel);
	if (Status != XST_SUCCESS) {
		XPreFsbl_Printf(DEBUG_INFO,"Failed to enable the MUX channel\r\n");
		Status = XPREFSBL_IICPS_MUX_ERROR;
		goto END;
	}

END:
	return Status;
}
#endif