/* CSP includes*/
#include <csp/csp.h>
#include <csp/include/csp/csp_debug.h>
#include <csp/include/csp/interfaces/csp_if_can.h>

/* This function refferences the XIlinx boards setup for CAN0*/
int xil_setup_can(void);

/* Function that intializes the csp interface part of the CAN driver.
 * */
void csp_iface_can_init(int addr, int netmask, uint32_t bitrate, uint16_t logAddr);

/* Function that takes data and destination information, and packages it to send with csp_send
* */
int cspSender(uint8_t* dataToSend, uint8_t dataLength, uint16_t destination, uint8_t sourceP, uint8_t destP, uint8_t cspFlag);

/* Function that refers main to csp_servicehandler*/
void serviceToDo (csp_packet_t *receivedPacket);

/* Function that takes logmessage and sends CSP packet, with that information to log address*/
void sendToLog(uint8_t logMessage);

/* The function called from csp_send() to send with the CAN interface*/
void canHopper(csp_iface_t *iface, uint32_t CFPID, uint8_t* frameBuf, uint8_t frameBufInp);