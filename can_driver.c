/* Xil libraries*/
#include "xparameters.h"
#include "xcanps.h"
#include "xil_exception.h"
#include "xil_printf.h"

/* xilCanDriverInclude*/
#include "xscugic.h"

/* FreeRTOS includes*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

/* CSP includes*/
#include <csp/csp.h>
#include <csp/include/csp/csp_debug.h>
#include <csp/include/csp/interfaces/csp_if_can.h>
#include <csp/src/interfaces/csp_if_can_pbuf.h>

#include <machine/endian.h>

/* Message ID*/
// #define TEST_MESSAGE_ID		2000 Deprecated

/* Can instance ID*/
#define INTC				XScuGic
#define CAN_DEVICE_ID		XPAR_XCANPS_0_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define CAN_INTR_VEC_ID		XPAR_XCANPS_0_INTR

/* Length of the frame and data*/
#define XCANPS_MAX_FRAME_SIZE_IN_WORDS (XCANPS_MAX_FRAME_SIZE / sizeof(u32))
#define FRAME_DATA_LENGTH	8 /* Frame Data field length */

XCanPs_Config *ConfigPtr;

/* Bit timing register*/
#define TEST_BTR_SYNCJUMPWIDTH		0
#define TEST_BTR_FIRST_TIMESEGMENT	7
#define TEST_BTR_SECOND_TIMESEGMENT	2
#define TEST_BRPR_BAUD_PRESCALAR	3

/* Instances of CAN*/
static XCanPs CanInstance;    			/* Instance of the Can driver */
//static INTC IntcInstance; 				/* Instance of the Interrupt Controller driver */
extern XScuGic xInterruptController;

// Defines for err
#define CSP_DBG_CAN_ERR_TX_OVF 7
#define CSP_DBG_CAN_PBUF_NO_FIND 8

/************************** Function Prototypes ******************************/

int CanPsIntrExample(INTC *IntcInstPtr, XCanPs *CanInstPtr,
			u16 CanDeviceId, u16 CanIntrId);
static void Config(XCanPs *InstancePtr);
//static void SendFrame(XCanPs *InstancePtr);

static void SendHandler(void *CallBackRef);
static void RecvHandler(void *CallBackRef);
static void ErrorHandler(void *CallBackRef, u32 ErrorMask);
static void EventHandler(void *CallBackRef, u32 Mask);

static int SetupInterruptSystem(INTC *IntcInstancePtr, XCanPs *CanInstancePtr, u16 CanIntrId);

/************************** Variable Definitions *****************************/

static XCanPs CanInstance;    /* Instance of the Can driver */
extern XScuGic xInterruptController;

/*
 * Buffers to hold frames to send and receive. These are declared as global so
 * that they are not on the stack.
 * These buffers need to be 32-bit aligned
 */
static u32 TxFrame[XCANPS_MAX_FRAME_SIZE_IN_WORDS];
static u32 RxFrame[XCANPS_MAX_FRAME_SIZE_IN_WORDS];

/*
 * Shared variables used to test the callbacks.
 */
volatile static int LoopbackError;	/* Asynchronous error occurred */
volatile static int RecvDone;		/* Received a frame */
volatile static int SendDone;		/* Frame was sent successfully */

/* Log address and log counter*/
uint16_t logAddress;
uint32_t logCounter = 0;

/* Interface definition and addresses*/
struct xilCan_d{
	uint32_t id;
	csp_can_interface_data_t ifdata;
	SemaphoreHandle_t lock;
	StaticSemaphore_t lock_buf;
	csp_iface_t interface;
};

typedef struct xilCan_d xilCan_d;

xilCan_d xilCanInt;



/*****************************************************************************/
/**
*
* The main entry point for showing the XCanPs driver in interrupt mode.
* The example configures the device for internal loop back mode, then
* sends a CAN frame and receives the same CAN frame.
*
* @param	IntcInstPtr is a pointer to the instance of the INTC driver.
* @param	CanInstPtr is a pointer to the instance of the CAN driver which
*		is going to be connected to the interrupt controller.
* @param	CanDeviceId is the device Id of the CAN device and is typically
*		XPAR_<CANPS_instance>_DEVICE_ID value from xparameters.h.
* @param	CanIntrId is the interrupt Id and is typically
*		XPAR_<CANPS_instance>_INTR value from xparameters.h.
*
* @return	XST_SUCCESS if successful, otherwise driver-specific error code.
*
* @note		If the device is not working correctly, this function may enter
*		an infinite loop and will never return to the caller.
*
******************************************************************************/
int CanPsIntrSetup(INTC *IntcInstPtr, XCanPs *CanInstPtr, u16 CanDeviceId, u16 CanIntrId)
{
	int Status;


	/*
	 * Initialize the Can device.
	 */
	xil_printf("Init can device \r\n");

	ConfigPtr = XCanPs_LookupConfig(CanDeviceId);
	if (ConfigPtr == NULL) {
		return XST_FAILURE;
	}

	XCanPs_CfgInitialize(CanInstPtr,
				ConfigPtr,
				ConfigPtr->BaseAddr);

	/*
	 * Run self-test on the device, which verifies basic sanity of the
	 * device and the driver.
	 */
	xil_printf("Run self-test \r\n");
	Status = XCanPs_SelfTest(CanInstPtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Configure CAN device.
	 */
	Config(CanInstPtr);



	/*
	 * Set interrupt handlers.
	 */
	XCanPs_SetHandler(CanInstPtr, XCANPS_HANDLER_SEND,
			(void *)SendHandler, (void *)CanInstPtr);
	XCanPs_SetHandler(CanInstPtr, XCANPS_HANDLER_RECV,
			(void *)RecvHandler, (void *)CanInstPtr);
	XCanPs_SetHandler(CanInstPtr, XCANPS_HANDLER_ERROR,
			(void *)ErrorHandler, (void *)CanInstPtr);
	XCanPs_SetHandler(CanInstPtr, XCANPS_HANDLER_EVENT,
			(void *)EventHandler, (void *)CanInstPtr);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Setup the Can filter in accordance with CSP.
	XCanPs_AcceptFilterSet(CanInstPtr, 1, 0x3fe7c000, 0x1c07c000);
	XCanPs_AcceptFilterEnable(CanInstPtr, 1);



	/*
	 * Connect to the interrupt controller.
	 */
	xil_printf("Connect interrupt controller \r\n");
	Status =  SetupInterruptSystem(IntcInstPtr,
					CanInstPtr,
					CanIntrId);

	/*
	 * Enable all interrupts in CAN device.
	 */
	XCanPs_IntrEnable(CanInstPtr, XCANPS_IXR_ALL);

	/*
	 * Enter Normal Mode.
	 */
	XCanPs_EnterMode(CanInstPtr, XCANPS_MODE_NORMAL);
	while(XCanPs_GetMode(CanInstPtr) != XCANPS_MODE_NORMAL);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function configures CAN device. Baud Rate Prescaler Register (BRPR) and
* Bit Timing Register (BTR) are set in this function.
*
* @param	InstancePtr is a pointer to the driver instance.
*
* @return	None.
*
* @note		If the CAN device is not working correctly, this function may
*		enter an infinite loop and will never return to the caller.
*
******************************************************************************/
static void Config(XCanPs *InstancePtr)
{
	/*
	 * Enter Configuration Mode if the device is not currently in
	 * Configuration Mode.
	 */
	XCanPs_EnterMode(InstancePtr, XCANPS_MODE_CONFIG);
	while(XCanPs_GetMode(InstancePtr) != XCANPS_MODE_CONFIG);

	/*
	 * Setup Baud Rate Prescaler Register (BRPR) and
	 * Bit Timing Register (BTR).
	 */
	XCanPs_SetBaudRatePrescaler(InstancePtr, TEST_BRPR_BAUD_PRESCALAR);
	XCanPs_SetBitTiming(InstancePtr, TEST_BTR_SYNCJUMPWIDTH,
					TEST_BTR_SECOND_TIMESEGMENT,
					TEST_BTR_FIRST_TIMESEGMENT);

}


/*****************************************************************************/
/**
*
* Callback function (called from interrupt handler) to handle confirmation of
* transmit events when in interrupt mode.
*
* @param	CallBackRef is the callback reference passed from the interrupt
*		handler, which in our case is a pointer to the driver instance.
*
* @return	None.
*
* @note		This function is called by the driver within interrupt context.
*
******************************************************************************/
static void SendHandler(void *CallBackRef)
{
	/*
	 * The frame was sent successfully. Notify the task context.
	 */

	// Add that the interface has sent a frame succesfully.
	xilCanInt.interface.tx++;
}


/*****************************************************************************/
/**
*
* Callback function (called from interrupt handler) to handle frames received in
* interrupt mode.  This function is called once per frame received.
* The driver's receive function is called to read the frame from RX FIFO.
*
* @param	CallBackRef is the callback reference passed from the interrupt
*		handler, which in our case is a pointer to the device instance.
*
* @return	None.
*
* @note		This function is called by the driver within interrupt context.
*
******************************************************************************/
static void RecvHandler(void *CallBackRef)
{
	// Read the data out of the register and into the RxFrame
	XCanPs_Recv(&CanInstance, RxFrame);

	xil_printf("%x %x %x %x \n \r", RxFrame[0], RxFrame[1], RxFrame[2], RxFrame[3]);

	// The RxFrame needs to have the RTR and ID ext removed.
	RxFrame[0] = ((RxFrame[0] & 0xffe00000)>>3)|((RxFrame[0] &0xffffe)>>1);

	xil_printf("%x \n \r", RxFrame[0]);

	// Map the data from the CAN frame into an 8bit array.
	u8 (*messageData)[8] = (void*) &RxFrame[2];

	xil_printf("%x %x %x %x \n \r",(*messageData)[4], (*messageData)[5], (*messageData)[6], (*messageData)[7]);


	xil_printf("%x %x %x %x \n \r",RxFrame[1] >> XCANPS_DLCR_DLC_SHIFT);

	// Indicate that the task is being called through ISR.
	int TaskWokenFromISR = 1;

	// Call the csp_can_rx that parse the CAN frame into CSP packets.
	csp_can_rx(&xilCanInt.interface, RxFrame[0], *messageData, RxFrame[1] >> XCANPS_DLCR_DLC_SHIFT, &TaskWokenFromISR); //

	// Indicate that the interface has received more data.
	xilCanInt.interface.rx++;
}


/*****************************************************************************/
/**
*
* Callback function (called from interrupt handler) to handle error interrupt.
* Error code read from Error Status register is passed into this function.
*
* @param	CallBackRef is the callback reference passed from the interrupt
*		handler, which in our case is a pointer to the driver instance.
* @param	ErrorMask is a bit mask indicating the cause of the error.
*		Its value equals 'OR'ing one or more XCANPS_ESR_* defined in
*		xcanps_hw.h.
*
* @return	None.
*
* @note		This function is called by the driver within interrupt context.
*
******************************************************************************/
static void ErrorHandler(void *CallBackRef, u32 ErrorMask)
{

	if(ErrorMask & XCANPS_ESR_ACKER_MASK) {
		/*
		 * ACK Error handling code should be put here.
		 */
		xil_printf("? \r\n");

	}

	if(ErrorMask & XCANPS_ESR_BERR_MASK) {
		/*
		 * Bit Error handling code should be put here.
		 */
		xil_printf("! \r\n");

	}

	if(ErrorMask & XCANPS_ESR_STER_MASK) {
		/*
		 * Stuff Error handling code should be put here.
		 */
		xil_printf("XCANPS_ESR_STER_MASK \r\n");

	}

	if(ErrorMask & XCANPS_ESR_FMER_MASK) {
		/*
		 * Form Error handling code should be put here.
		 */
		xil_printf("XCANPS_ESR_FMER_MASK \r\n");

	}

	if(ErrorMask & XCANPS_ESR_CRCER_MASK) {
		/*
		 * CRC Error handling code should be put here.
		 */
		xil_printf("XCANPS_ESR_CRCER_MASK \r\n");

	}

	/*
	 * Set the shared variables.
	 */
	LoopbackError = TRUE;
	RecvDone = TRUE;
	SendDone = TRUE;
}


/*****************************************************************************/
/**
*
* Callback function (called from interrupt handler) to handle the following
* interrupts:
*   - XCANPS_IXR_BSOFF_MASK:	Bus Off Interrupt
*   - XCANPS_IXR_RXOFLW_MASK:	RX FIFO Overflow Interrupt
*   - XCANPS_IXR_RXUFLW_MASK:	RX FIFO Underflow Interrupt
*   - XCANPS_IXR_TXBFLL_MASK:	TX High Priority Buffer Full Interrupt
*   - XCANPS_IXR_TXFLL_MASK:	TX FIFO Full Interrupt
*   - XCANPS_IXR_WKUP_MASK:	Wake up Interrupt
*   - XCANPS_IXR_SLP_MASK:	Sleep Interrupt
*   - XCANPS_IXR_ARBLST_MASK:	Arbitration Lost Interrupt
*
*
* @param	CallBackRef is the callback reference passed from the
*		interrupt Handler, which in our case is a pointer to the
*		driver instance.
* @param	IntrMask is a bit mask indicating pending interrupts.
*		Its value equals 'OR'ing one or more of the XCANPS_IXR_*_MASK
*		value(s) mentioned above.
*
* @return	None.
*
* @note		This function is called by the driver within interrupt context.
* 		This function should be changed to meet specific application
*		needs.
*
******************************************************************************/
static void EventHandler(void *CallBackRef, u32 IntrMask)
{
	//XCanPs *CanPtr = (XCanPs *)CallBackRef;

	if (IntrMask & XCANPS_IXR_BSOFF_MASK) {
		/*
		 * Entering Bus off status interrupt requires
		 * the CAN device be reset and reconfigured.
		 */
		xil_printf("BO \r\n");
		//XCanPs_Reset(CanPtr);
		//Config(CanPtr);
		//return;
	}

	if(IntrMask & XCANPS_IXR_RXOFLW_MASK) {
		/*
		 * Code to handle RX FIFO Overflow Interrupt should be put here.
		 */
	}

	if(IntrMask & XCANPS_IXR_RXUFLW_MASK) {
		/*
		 * Code to handle RX FIFO Underflow Interrupt
		 * should be put here.
		 */
	}

	if(IntrMask & XCANPS_IXR_TXBFLL_MASK) {
		/*
		 * Code to handle TX High Priority Buffer Full
		 * Interrupt should be put here.
		 */
	}

	if(IntrMask & XCANPS_IXR_TXFLL_MASK) {
		/*
		 * Code to handle TX FIFO Full Interrupt should be put here.
		 */
	}

	if (IntrMask & XCANPS_IXR_WKUP_MASK) {
		/*
		 * Code to handle Wake up from sleep mode Interrupt
		 * should be put here.
		 */
	}

	if (IntrMask & XCANPS_IXR_SLP_MASK) {
		/*
		 * Code to handle Enter sleep mode Interrupt should be put here.
		 */
	}

	if (IntrMask & XCANPS_IXR_ARBLST_MASK) {
		/*
		 * Code to handle Lost bus arbitration Interrupt
		 * should be put here.
		 */
	}
}

void canHopper(csp_iface_t *iface, uint32_t CFPID, uint8_t* frameBuf, uint8_t frameBufInp){

	// The CFP ID has to be changed so that it fits with the registers, therefor shifts are done.
	TxFrame[0] = ((CFPID & 0x1FFC0000)<<3)|(0x1<<19)|((CFPID & 0x3FFFF)<<1);

	// The data length has to be so that it fits the registers.
	TxFrame[1] = XCanPs_CreateDlcValue(frameBufInp);

	// Put the first four bytes of the frameBuf into the Dataword 1 register frame
	TxFrame[2] = (frameBuf[3]<<24)|(frameBuf[2]<<16)|(frameBuf[1]<<8)|(frameBuf[0]);


	// Put the last four bytes of the frameBuf into the Dataword 2 register frame
	int i = 0;
	for (i = 0; i  < frameBufInp-4; i++ ){
		TxFrame[3] |= frameBuf[i + 4]<<(i*8);
	}

	TxFrame[3] = (frameBuf[7]<<24)|(frameBuf[6]<<16)|(frameBuf[5]<<8)|(frameBuf[4]);

	XCanPs_Send(&CanInstance, TxFrame);
}


/*****************************************************************************/
/**
*
* This function sets up the interrupt system so interrupts can occur for the
* CAN. This function is application-specific since the actual system may or
* may not have an interrupt controller. The CAN could be directly connected
* to a processor without an interrupt controller. The user should modify this
* function to fit the application.
*
* @param	IntcInstancePtr is a pointer to the instance of the ScuGic.
* @param	CanInstancePtr contains a pointer to the instance of the CAN
*		which is going to be connected to the interrupt
*		controller.
* @param	CanIntrId is the interrupt Id and is typically
*		XPAR_<CANPS_instance>_INTR value from xparameters.h.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None.
*
****************************************************************************/
static int SetupInterruptSystem(INTC *IntcInstancePtr,
				XCanPs *CanInstancePtr,
				u16 CanIntrId)
{
	int Status;

	XScuGic_Config *IntcConfig; /* Instance of the interrupt controller */

	Xil_ExceptionInit();

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
				(Xil_ExceptionHandler)XScuGic_InterruptHandler,
				IntcInstancePtr);

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, CanIntrId,
				(Xil_InterruptHandler)XCanPs_IntrHandler,
				(void *)CanInstancePtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	/*
	 * Enable the interrupt for the CAN device.
	 */
	XScuGic_Enable(IntcInstancePtr, CanIntrId);

	/*
	 * Enable interrupts in the Processor.
	 */
	Xil_ExceptionEnable();
	return XST_SUCCESS;
}

int xil_setup_can(void) {
	return ( CanPsIntrSetup(&xInterruptController, &CanInstance, CAN_DEVICE_ID, CAN_INTR_VEC_ID) );
}

void csp_iface_can_init(int addr, int netmask, uint32_t bitrate, uint16_t logAddr) {


	/*  Define the log address*/
	logAddress = logAddr;

	xilCanInt.interface.name = "CAN0";


	/* Create a mutex type semaphore. */
	 xilCanInt.lock = xSemaphoreCreateBinary();

	//const csp_conf_t *csp_conf = csp_get_conf();

	xilCanInt.ifdata.tx_func = (void *) canHopper;

	xilCanInt.interface.nexthop = csp_can2_tx;

	xilCanInt.interface.netmask = netmask;

	/* The MTU is configured run-time, since the buffer size can be configured externally
	 * however, it must not exceed 2042 due to the CFP_REMAIN field limitation
	 * CFP_REMAIN gives possibility of 255 * 8 bytes = 2040
	 * CSP_BEGIN frame, has two additional bytes, in total 2042 */
     xilCanInt.interface.mtu = CSP_BUFFER_SIZE; // Changed to 2042, instead of meson build from ubuntu 256.
	if ( xilCanInt.interface.mtu > 116) {
		 xilCanInt.interface.mtu = 116;
	}

	 xilCanInt.ifdata.pbufs = NULL;
	 xilCanInt.interface.interface_data = & xilCanInt.ifdata;

	 xilCanInt.interface.addr = addr;
	 xilCanInt.interface.netmask = netmask;

	/* Regsiter interface */
	csp_can_add_interface(&xilCanInt.interface);
}

void serviceToDo (csp_packet_t *receivedPacket){

	// When receiving a request to do a CSP service check if these are defined.
	if (receivedPacket->id.dport == 0){
		// Check if the request is to change the route
		if (receivedPacket->data[1] == 2 || receivedPacket->data[1] == 7){
					// do squat, maybe reply not avaiable.
		} else {// If not actually do somethign
			csp_service_handler(receivedPacket);
		}
	} else { // Do the service handler.
		csp_service_handler(receivedPacket);
	}

}


int cspSender(uint8_t* dataToSend, uint8_t dataLength, uint16_t destination, uint8_t sourceP, uint8_t destP,
		uint8_t cspFlag){



		// Initiate packet
		csp_packet_t * Packet;

		// Create it in pbuf
		Packet = csp_can_pbuf_new(&xilCanInt.ifdata, 5, 0);

		// If it did not create a pbuf return with error.
		if (Packet == NULL){
			return CSP_DBG_CAN_PBUF_NO_FIND;
		}

		// Check for overflow and return error should it happen
		if (dataLength > xilCanInt.interface.mtu){
			return CSP_DBG_CAN_ERR_TX_OVF;
		}

		// Packet data length.
		Packet->length = dataLength;


		// Append the data to the packet.
		int i;

		for (i = 0 ; i < dataLength ; i++){
			Packet->data[i] = dataToSend[i];
		}

		// ID and neccesary headers.
		Packet->id.dport = destP;
		Packet->id.sport = sourceP;
		Packet->id.dst = destination;
		Packet->id.src = xilCanInt.interface.addr;
		Packet->id.flags = cspFlag;
		Packet->id.pri = 0x0;

		csp_conn_t * CSPConnect = csp_connect(0x00, destination, destP, CSP_MAX_TIMEOUT,0);


		// Call the csp tx function through return.
		csp_send(CSPConnect, Packet);

		return 1;
}

void sendToLog(uint8_t logMessage){
	// Make data buffer that the message and counter can be put into
	uint8_t dataTransformed[4];

	// Put the log message into the last bit
	dataTransformed[3] = logMessage;

	// Put the counter into the three first bytes, the counter is meant to be 24 bit but is stored as a 32 bit locally.
	// This is done be masking and shifting to the right.
	dataTransformed[2] = logCounter&0xFF;

	dataTransformed[1] = (logCounter&0xFF00)>>8;

	dataTransformed[0] = (logCounter&0xFF0000)>>16;

	// Send it through CSP
	cspSender(dataTransformed, 4, logAddress,0x0A,0x0A,0x00);

	// count of the log counter by one.
	logCounter++;
}

