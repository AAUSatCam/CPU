/*
    Video Direct Memory Access configuration

	Made by: 	Stephan Larsen
	Date: 		2022-03-15
	Based on: 	Trenz Zynqberry demo1 (MIT license)

	Purpose:	Configuration of Video timing controller and VDMA to allow piping video from CSI -> HDMI.
*/

// Trenz vdma library.
#include "te_vdma.h"

// Xilinx libraries.
#include "xparameters.h"
#include "xil_hal.h"
#include "sleep.h"
#include "xvtc.h"
#include "xil_printf.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

// Xilinx I2C driver
#include "xiicps.h"

#include "sensor_config.h"

#define IIC_DEVICE_ID		XPAR_XIICPS_1_DEVICE_ID
#define IIC_SLAVE_ADDR		0x36
#define IIC_SCLK_RATE		100000
#define IIC_BUF_SIZE 		3
#define MUX_ADDR			0x70
#define CSI_IIC_EN			0x07

XIicPs Iic;						/**< Instance of the IIC Device  	*/

// Define memory address. Set to same for direct pass through.
//#define HDMI_FB_ADDR			0x1FC00000
//#define CAMERA_FB_ADDR			0x1FC00000

#define DEBUGGING1

#ifndef DEBUGGING1

#define HDMI_FB_ADDR			0x100000
#define CAMERA_FB_ADDR			0x100000

#else

unsigned char VideoBuffer[1080*1920*4];
unsigned char VideoBuffer1[1080*1920*4];

#endif

u32 EnableVideoTimingController (void) {
	u32 Status;

	Status = XST_SUCCESS;

    xil_printf("\r\n--------------------------------------------------------------------------------\r\n");
    xil_printf("Configuration of VTC and VDMA\r\n");

	XVtc Vtc;

    xil_printf("Enabling VTC..\n\r");
	XVtc_Config *Config;
	Config = XVtc_LookupConfig(XPAR_VTC_0_DEVICE_ID);
	if (NULL == Config) {
		xil_printf("XVtc_LookupConfig failure\r\n");
		return XST_FAILURE;
	}

	Status = XVtc_CfgInitialize(&Vtc, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("XVtc_CfgInitialize failure\r\n");
		return XST_FAILURE;
	}

	XVtc_DisableSync(&Vtc);
	XVtc_EnableGenerator(&Vtc);

#ifndef DEBUGGING1

    xil_printf("Enabling Out VDMA at 0x%08x..\n\r",HDMI_FB_ADDR);
    //vdma_out_init(XPAR_VIDEO_OUT_AXI_VDMA_0_DEVICE_ID, HDMI_FB_ADDR, 1280, 720, 4);
    vdma_out_init(XPAR_VIDEO_OUT_AXI_VDMA_0_DEVICE_ID, HDMI_FB_ADDR, 1920, 1080, 4);

    xil_printf("Enabling In  VDMA at 0x%08x..\n\r",CAMERA_FB_ADDR);
    //vdma_in_init(XPAR_VIDEO_IN_AXI_VDMA_0_DEVICE_ID, CAMERA_FB_ADDR, 1280, 720, 4);
    vdma_in_init(XPAR_VIDEO_IN_AXI_VDMA_0_DEVICE_ID, CAMERA_FB_ADDR, 1920, 1080, 4);

#else
    //u32 Addr;

    //Addr = (u32)&(VideoBuffer[0]);

    xil_printf("Enabling Out VDMA at 0x%08x..\n\r", VideoBuffer);
    //vdma_out_init(XPAR_VIDEO_OUT_AXI_VDMA_0_DEVICE_ID, HDMI_FB_ADDR, 1280, 720, 4);
    vdma_out_init(XPAR_VIDEO_OUT_AXI_VDMA_0_DEVICE_ID, &VideoBuffer, 1920, 1080, 4);

    xil_printf("Enabling In  VDMA at 0x%08x..\n\r", VideoBuffer);
    //vdma_in_init(XPAR_VIDEO_IN_AXI_VDMA_0_DEVICE_ID, CAMERA_FB_ADDR, 1280, 720, 4);
    vdma_in_init(XPAR_VIDEO_IN_AXI_VDMA_0_DEVICE_ID, &VideoBuffer, 1920, 1080, 4, &VideoBuffer1);

#endif

    xil_printf("\r\n--------------------------------------------------------------------------------\r\n");

    usleep(5000);
	return (Status);
}

u32 videoSetColor (u32 color) {
	return 0;
}

u32 initIIC(void) {
	xil_printf("Initializing I2C...\r\n");
	int Status;
	XIicPs_Config *Config;

	// Init IIC based on config table
	Config = XIicPs_LookupConfig(IIC_DEVICE_ID);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	// Perform a self-test to ensure that the hardware was built correctly.
	Status = XIicPs_SelfTest(&Iic);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	// Set the IIC serial clock rate.
	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

	xil_printf("I2C init successfully...\r\n");
	usleep(10000);
	return XST_SUCCESS;
}

u32 i2c_sendconf(struct sensor_cmd *set) {
	int i, Status;
	u8 SendBuffer[IIC_BUF_SIZE];    /**< Buffer for Transmitting Data 	*/
	for(i=0; set[i].reg != TABLE_END; i++){

		SendBuffer[0] = set[i].reg >> 8;
		SendBuffer[1] = set[i].reg & 0xFF;
		SendBuffer[2] = set[i].val;

		Status = XIicPs_MasterSendPolled(&Iic, SendBuffer, IIC_BUF_SIZE, IIC_SLAVE_ADDR);

		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
	}
	return XST_SUCCESS;
}

u32 configRPI(void) {
/*
	i2c_reg_write(i2c_file, CAMERA_V1_3_IIC_ADDRESS, 0x0100, 0x00);					// Disable
	i2c_reg_write(i2c_file, CAMERA_V1_3_IIC_ADDRESS, 0x0103, 0x01);		  			// Reset
	usleep(1);																		// Wait
	i2c_reg_write(i2c_file, CAMERA_V1_3_IIC_ADDRESS, 0x0103, 0x00);					// Reset
	usleep(10 * 1000);																// Wait
	i2c_set_write(i2c_file, CAMERA_V1_3_IIC_ADDRESS, ov5647_sensor_common_10bit);   // Load common configuration
	i2c_set_write(i2c_file, CAMERA_V1_3_IIC_ADDRESS, ov5647_sensor_1296_968_30);	// Load specific configuration
	i2c_reg_write(i2c_file, CAMERA_V1_3_IIC_ADDRESS, 0x0100, 0x01);		  			// Enable
	printf("Camera init complete.\n");
*/
	u8 SendBuffer[IIC_BUF_SIZE];    /**< Buffer for Transmitting Data 	*/
//	u8 RecvBuffer[IIC_BUF_SIZE];    /**< Buffer for Receiving Data 		*/

	int Status;

	SendBuffer[0] = CSI_IIC_EN;

	// First Enable the I2C Mux to CSI.
	Status = XIicPs_MasterSendPolled(&Iic, SendBuffer, 1, MUX_ADDR);
	//xil_printf( "Mux addr: %x \t Enable: %x \t Status: %x \r\n", MUX_ADDR, SendBuffer[0], Status );

	while (XIicPs_BusIsBusy(&Iic)) {
		// NOP
		xil_printf("busy..");
	}

	SendBuffer[0] = 0x01;
	SendBuffer[1] = 0x00;
	SendBuffer[2] = 0x00;

	// Then send the rest
	Status = XIicPs_MasterSendPolled(&Iic, SendBuffer, IIC_BUF_SIZE, IIC_SLAVE_ADDR);
	//xil_printf( "I2C addr: %x \t First byte: %x \t Status: %x \r\n", IIC_SLAVE_ADDR, SendBuffer[0], Status );
	if (Status != XST_SUCCESS) {
		xil_printf( "I2C failed: %x \r\n", Status );
		return XST_FAILURE;
	}

	usleep(100 *1000);

	// Reset
	SendBuffer[0] = 0x01;
	SendBuffer[1] = 0x03;
	SendBuffer[2] = 0x01;
	Status = XIicPs_MasterSendPolled(&Iic, SendBuffer, IIC_BUF_SIZE, IIC_SLAVE_ADDR);
	if (Status != XST_SUCCESS) {
		xil_printf( "I2C failed: %x \r\n", Status );
		return XST_FAILURE;
	}

	// Reset
	SendBuffer[0] = 0x01;
	SendBuffer[1] = 0x03;
	SendBuffer[2] = 0x00;
	Status = XIicPs_MasterSendPolled(&Iic, SendBuffer, IIC_BUF_SIZE, IIC_SLAVE_ADDR);
	if (Status != XST_SUCCESS) {
		xil_printf( "I2C failed: %x \r\n", Status );
		return XST_FAILURE;
	}

	//vTaskDelay(pdMS_TO_TICKS(10));
	usleep(100 *1000);

	// Load common config
	Status = i2c_sendconf(ov5647_sensor_common_10bit);
	if (Status != XST_SUCCESS) {
		xil_printf( "I2C failed: %x \r\n", Status );
		return XST_FAILURE;
	}

	usleep(50 *1000);

	// Load specific config
	//Status = i2c_sendconf(ov5647_sensor_1296_968_30);
	Status = i2c_sendconf(ov5647_sensor_1936_1088_30);
	if (Status != XST_SUCCESS) {
		xil_printf( "I2C failed: %x \r\n", Status );
		return XST_FAILURE;
	}

	usleep(100 *1000);

	// Enable
	SendBuffer[0] = 0x01;
	SendBuffer[1] = 0x00;
	SendBuffer[2] = 0x01;
	Status = XIicPs_MasterSendPolled(&Iic, SendBuffer, IIC_BUF_SIZE, IIC_SLAVE_ADDR);
	if (Status != XST_SUCCESS) {
		xil_printf( "I2C failed: %x \r\n", Status );
		return XST_FAILURE;
	}

	xil_printf("Camera init done... \r\n");

	return (XST_SUCCESS);
}

int vdma_read_status(void) {
	return ( _get_vdma_read_status() );
}

int vdma_write_status(void) {
	return ( _get_vdma_write_status() );
}
