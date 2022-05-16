/*

	Author: 	Stephan Larsen
	Year: 		2022
	Based on: 	Trenz Zynqberry demo1 (MIT license)

	Purpose:
*/

#pragma once
#include "xaxivdma.h"

int vdma_out_init(short DeviceID, int base_address, int h_width, int v_width, int bpp);
int vdma_in_init(short DeviceID, int base_address, int h_width, int v_width, int bpp, int base_address2);

int vdma_stop();
int vdma_in_start();
int vdma_out_start();

u32 vdma_version();

int vdma_set_color(u32 color);

int _get_vdma_read_status(void);
int _get_vdma_write_status(void);
