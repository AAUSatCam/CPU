/*

	Author: 	Stephan Larsen
	Year: 		2022
	Based on: 	Trenz Zynqberry demo1 (MIT license)

	Purpose:
*/

#include "xil_printf.h"

#include "te_vdma.h"
#include "gpio_ctrl.h"
#include "Xscugic.h"

#include "xil_cache.h"

#include <FreeRTOS.h>

XAxiVdma OutVdma;
XAxiVdma InVdma;

XAxiVdma_DmaSetup VDMAOutCfg;
XAxiVdma_DmaSetup VDMAInCfg;

XScuGic Intc;
XScuGic_Config *IntcConfig; // XPAR_PS7_SCUGIC_0_DEVICE_ID

extern unsigned char VideoBuffer1[1080*1920*4];
extern XScuGic xInterruptController;

u32 vdma_version() {
	return XAxiVdma_GetVersion(&OutVdma);
}

int vdma_out_start() {
	   int Status;

	   // MM2S Startup
	   Status = XAxiVdma_DmaStart(&OutVdma, XAXIVDMA_READ);
	   if (Status != XST_SUCCESS)
	   {
	      xil_printf("Start read transfer failed %d\n\r", Status);
	      return XST_FAILURE;
	   }

	   return XST_SUCCESS;
}

int vdma_in_start() {
	   int Status;

	   // MM2S Startup
	   Status = XAxiVdma_DmaStart(&InVdma, XAXIVDMA_WRITE);
	   if (Status != XST_SUCCESS)
	   {
	      xil_printf("Start read transfer failed %d\n\r", Status);
	      return XST_FAILURE;
	   }

	   return XST_SUCCESS;
}

int vdma_stop() {
	   XAxiVdma_DmaStop(&OutVdma, XAXIVDMA_READ);
	   XAxiVdma_DmaStop(&InVdma, XAXIVDMA_WRITE);
	   return XST_SUCCESS;
}

int vdma_set_color(u32 color) {
	// Not implemented.
	return XST_FAILURE;
}

int vdma_out_init(short DeviceID, int base_address, int h_width, int v_width, int bpp)
{
	XAxiVdma_Config *Config;
	int Status;


	Config = XAxiVdma_LookupConfig(DeviceID);
	if (NULL == Config) {
		xil_printf("XAxiVdma_LookupConfig failure\r\n");
		return XST_FAILURE;
	}

	Status = XAxiVdma_CfgInitialize(&OutVdma, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiVdma_CfgInitialize failure\r\n");
		return XST_FAILURE;
	}

	VDMAOutCfg.EnableCircularBuf = 1;
	VDMAOutCfg.EnableFrameCounter = 0;
	VDMAOutCfg.FixedFrameStoreAddr = 0;

	VDMAOutCfg.EnableSync = 1;
	VDMAOutCfg.PointNum = 1;

	VDMAOutCfg.FrameDelay = 0;

	VDMAOutCfg.VertSizeInput = v_width;
	VDMAOutCfg.HoriSizeInput = h_width * bpp;
	VDMAOutCfg.Stride = VDMAOutCfg.HoriSizeInput;

	Status = XAxiVdma_DmaConfig(&OutVdma, XAXIVDMA_READ, &VDMAOutCfg);
	if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,
				"Read channel config failed %d\r\n", Status);

			return XST_FAILURE;
	}

	VDMAOutCfg.FrameStoreStartAddr[0] = base_address;

	Status = XAxiVdma_DmaSetBufferAddr(&OutVdma, XAXIVDMA_READ, VDMAOutCfg.FrameStoreStartAddr);
	if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,"Read channel set buffer address failed %d\r\n", Status);
			return XST_FAILURE;
	}


	Status = vdma_out_start();
	if (Status != XST_SUCCESS) {
		   xil_printf("error starting VDMA..!");
		   return Status;
	}
	return XST_SUCCESS;

}

#define OVERLAYSIZE 100

static void WriteCallBack(void *CallbackRef, u32 Mask)
{
	if (Mask & XAXIVDMA_IXR_COMPLETION_MASK) {
		gpio_set(70, 0);
		//memset( (void *)(0x1FC00000 + 0x151800), 0x00, OVERLAYSIZE);
	}

	if (Mask & XAXIVDMA_IXR_FRMCNT_MASK) {
		gpio_set(70, 1);
	}
}

static void ReadCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_COMPLETION_MASK) {
		gpio_set(66, 0);
	}

	if (Mask & XAXIVDMA_IXR_FRMCNT_MASK) {
		gpio_set(66, 1);
	}

}

static int SetupIntrReadSystem(XAxiVdma *AxiVdmaPtr, u16 ReadIntrId)
{
	int Status;
	XScuGic *IntcInstancePtr = &xInterruptController;

	// Initialize the interrupt controller and connect the ISRs

	XScuGic_Config *IntcConfig;
	IntcConfig = XScuGic_LookupConfig(XPAR_PS7_SCUGIC_0_DEVICE_ID);
	Status =  XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig, IntcConfig->CpuBaseAddress);
	if(Status != XST_SUCCESS){
		xil_printf("Interrupt controller initialization failed..");
		return -1;
	}

	Status = XScuGic_Connect(IntcInstancePtr,ReadIntrId,(Xil_InterruptHandler)XAxiVdma_ReadIntrHandler, (void *)AxiVdmaPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed read channel connect intc %d\r\n", Status);
		return XST_FAILURE;
	}

	XScuGic_Enable(IntcInstancePtr,ReadIntrId);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)XScuGic_InterruptHandler,(void *)IntcInstancePtr);
	Xil_ExceptionEnable();

	// Register call-back functions
	XAxiVdma_SetCallBack(AxiVdmaPtr, XAXIVDMA_HANDLER_GENERAL, ReadCallBack, (void *)AxiVdmaPtr, XAXIVDMA_READ);

	return XST_SUCCESS;
}

static int SetupIntrWriteSystem(XAxiVdma *AxiVdmaPtr, u16 WriteIntrId)
{
	int Status;
	XScuGic *IntcInstancePtr = &xInterruptController;

	// Initialize the interrupt controller and connect the ISRs

	XScuGic_Config *IntcConfig;
	IntcConfig = XScuGic_LookupConfig(XPAR_PS7_SCUGIC_0_DEVICE_ID);
	Status =  XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig, IntcConfig->CpuBaseAddress);
	if(Status != XST_SUCCESS){
		xil_printf("Interrupt controller initialization failed..");
		return -1;
	}

	Status = XScuGic_Connect(IntcInstancePtr,WriteIntrId,(Xil_InterruptHandler)XAxiVdma_WriteIntrHandler, (void *)AxiVdmaPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed write channel connect intc %d\r\n", Status);
		return XST_FAILURE;
	}

	XScuGic_Enable(IntcInstancePtr,WriteIntrId);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)XScuGic_InterruptHandler,(void *)IntcInstancePtr);
	Xil_ExceptionEnable();

	// Register call-back functions
	XAxiVdma_SetCallBack(AxiVdmaPtr, XAXIVDMA_HANDLER_GENERAL, WriteCallBack, (void *)AxiVdmaPtr, XAXIVDMA_WRITE);

	return XST_SUCCESS;
}

int vdma_in_init(short DeviceID, int base_address, int h_width, int v_width, int bpp, int base_address2)
{
	XAxiVdma_Config *Config;
	int Status;

	Config = XAxiVdma_LookupConfig(DeviceID);
	if (NULL == Config) {
		xil_printf("XAxiVdma_LookupConfig failure\r\n");
		return XST_FAILURE;
	}

	Status = XAxiVdma_CfgInitialize(&InVdma, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiVdma_CfgInitialize failure\r\n");
		return XST_FAILURE;
	}

	VDMAInCfg.EnableCircularBuf = 1;
	VDMAInCfg.EnableFrameCounter = 0;
	VDMAInCfg.FixedFrameStoreAddr = 0;

	VDMAInCfg.EnableSync = 1;
	VDMAInCfg.PointNum = 1;

	VDMAInCfg.FrameDelay = 0;

	VDMAInCfg.VertSizeInput = v_width;
	VDMAInCfg.HoriSizeInput = h_width * bpp;
	VDMAInCfg.Stride = VDMAInCfg.HoriSizeInput;

	Status = XAxiVdma_DmaConfig(&InVdma, XAXIVDMA_WRITE, &VDMAInCfg);
	if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,
				"Read channel config failed %d\r\n", Status);

			return XST_FAILURE;
	}

	VDMAInCfg.FrameStoreStartAddr[0] = base_address;
	//VDMAInCfg.FrameStoreStartAddr[1] = base_address2;

	Status = XAxiVdma_DmaSetBufferAddr(&InVdma, XAXIVDMA_WRITE, VDMAInCfg.FrameStoreStartAddr);
	if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,"Write channel set buffer address failed %d\r\n", Status);
			return XST_FAILURE;
	}

//////////////////////////// Configuring write interrupts ////////////////////////////////////
	XAxiVdma_IntrEnable( &InVdma, XAXIVDMA_IXR_COMPLETION_MASK, XAXIVDMA_WRITE );

	SetupIntrWriteSystem(&InVdma, XPAR_FABRIC_VIDEO_IN_AXI_VDMA_0_S2MM_INTROUT_INTR);
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////// Configuring write interrupts ////////////////////////////////////
	XAxiVdma_IntrEnable( &OutVdma, XAXIVDMA_IXR_COMPLETION_MASK, XAXIVDMA_READ );

	SetupIntrReadSystem(&OutVdma, XPAR_FABRIC_VIDEO_OUT_AXI_VDMA_0_MM2S_INTROUT_INTR);
//////////////////////////////////////////////////////////////////////////////////////////////////////



	Status = vdma_in_start();
	if (Status != XST_SUCCESS) {
		   xil_printf("error starting VDMA..!");
		   return Status;
	}

	return XST_SUCCESS;

}

int _get_vdma_read_status(void) {
	return ( XAxiVdma_IsBusy(&InVdma, XAXIVDMA_READ) );
}

int _get_vdma_write_status(void) {
	return ( XAxiVdma_IsBusy(&InVdma, XAXIVDMA_WRITE) );
}
