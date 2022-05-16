/*
 * Description: GPIO driver for AAUSat camera payload
 * Dato: 		2022-04-06
 * Author:		Stephan
 */
#include "gpio_ctrl.h"

#include "xparameters.h"
#include "xgpiops.h"
#include "xstatus.h"
#include "xplatform_info.h"
#include <xil_printf.h>

#define status_pin 17+54 			// Because Xilinx have a wierd arbitrary pin numbering (EMIO pins start from 54 and
#define vdma_write_interrupt_pin 16+54	// the array in Vivado has array index 17 at P15 aka gpio21 on trenz schematic)
#define vdma_read_interrupt_pin 12+54	// the array in Vivado has array index 17 at P15 aka gpio21 on trenz schematic)

XGpioPs Gpio;	/* The driver instance for GPIO Device. */

int gpio_init(void) {
	int Status;

	xil_printf("Initializing GPIO...\r\n");

	XGpioPs_Config *ConfigPtr;

	ConfigPtr = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID);

	Status = XGpioPs_CfgInitialize(&Gpio, ConfigPtr, ConfigPtr->BaseAddr);

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XGpioPs_SetDirectionPin(&Gpio, status_pin, 1);
	XGpioPs_SetOutputEnablePin(&Gpio, status_pin, 1);

	XGpioPs_SetDirectionPin(&Gpio, vdma_write_interrupt_pin, 1);
	XGpioPs_SetOutputEnablePin(&Gpio, vdma_write_interrupt_pin, 1);

	XGpioPs_SetDirectionPin(&Gpio, vdma_read_interrupt_pin, 1);
	XGpioPs_SetOutputEnablePin(&Gpio, vdma_read_interrupt_pin, 1);
/*
	// Debug code to enable all pins high.
	for (int var = 0; var < 24; ++var) {
		XGpioPs_SetDirectionPin( &Gpio, var + 54, 1 );
		XGpioPs_SetOutputEnablePin( &Gpio, var + 54, 1 );
		XGpioPs_WritePin( &Gpio, var + 54, 1 );
	}
*/
	return XST_SUCCESS;
}

int gpio_toggle(int pin) {
	//xil_printf("gpio read: %x\r\n", XGpioPs_ReadPin(&Gpio, status_pin));
	XGpioPs_WritePin( &Gpio, pin, !XGpioPs_ReadPin(&Gpio, pin) );
	return XST_SUCCESS;
}

int gpio_set(int pin, int lvl) {
	//printf("gpio read: %x\r\n", XGpioPs_ReadPin(&Gpio, status_pin));
	XGpioPs_WritePin( &Gpio, pin, lvl );
	return XST_SUCCESS;
}
