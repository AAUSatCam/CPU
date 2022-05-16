/*

	Author: 	Stephan Larsen
	Year: 		2022
	Based on: 	Trenz Zynqberry demo1 (MIT license)

	Purpose:
*/
#pragma once

u32 EnableVideoTimingController(void);
u32 initIIC(void);
u32 configRPI(void);

int vdma_read_status(void);
int vdma_write_status(void);
