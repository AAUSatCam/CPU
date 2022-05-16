/* CSP includes*/
#include <csp/csp.h>
#include <csp/include/csp/csp_debug.h>
#include <csp/include/csp/interfaces/csp_if_can.h>

/* This function refferences the XIlinx boards setup for CAN0*/
int xil_setup_can(void);

/* Function that intializes the csp interface part of the CAN driver.
 * */
void csp_iface_can_init(int addr, int netmask, uint32_t bitrate, uint16_t logAddr);

int cspSender(uint8_t* dataToSend, uint8_t dataLength, uint16_t destination, uint8_t sourceP, uint8_t destP, uint8_t cspFlag);

void serviceToDo (csp_packet_t *receivedPacket);

void sendToLog(uint8_t logMessage);
