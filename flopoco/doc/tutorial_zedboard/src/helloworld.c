/*
 * Copyright (c) 2009-2012 Xilinx, Inc.  All rights reserved.
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

//insert values here 0/1 for debug level
#define DEBUG_

//as 16-bit test take about 20 minutes to complete, you can define your test-size here
#define BIT_TEST_NUMBER 8

#ifdef DEBUG_1
	#define _DELAY 50000 //delay between two results display, in microseconds
	#define DEBUG_0
#endif

#include <stdio.h>
#include "platform.h"
#include <xparameters.h>
#include <xgpiops.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

//void print(char *str);

//register offsets
#define XGPIOPS_DATA_2 0x48 //GPIO input register (DATA_2, GPIO Bank 2, EMIO)
#define XGPIOPS_DATA_2_RO 0x68 //GPIO output register (DATA_RO, GPIO Bank 2, EMIO)
#define XGPIOPS_DIRM_2 0x284 //GPIO direction mode register (DRIM_2, GPIO Bank 2, EMIO)

int main()
{
	init_platform();

#ifdef DEBUG_1
	print("Trace\n\r");
#endif

	//result=number of right additions; maxcount=number of addition done
	char *result, *maxcount;

	result=malloc(5*sizeof(char));
	maxcount=malloc(5*sizeof(char));

#ifdef DEBUG_0
	//debug display section
	char *InputToPrint, *OutputToPrint, *baseone, *basetwo;
	InputToPrint=malloc(4*sizeof(char));
	OutputToPrint=malloc(4*sizeof(char));
	baseone=malloc(4*sizeof(char));
	basetwo=malloc(4*sizeof(char));
#endif

	//unmask all bits to be able to read output
	XGpioPs_WriteReg( XPAR_PS7_GPIO_0_BASEADDR, XGPIOPS_DIRM_2 , 0xFFFFFFFF );

	uint32_t Xinput;
	uint16_t base1=0;
	uint16_t base2=0;
	uint32_t success=0;
	uint32_t testnumber=0;

	while ( base1 < ( (2<<BIT_TEST_NUMBER) - 1 ) ) {

		while ( base2 < ( (2<<BIT_TEST_NUMBER) - 1 ) ){

			Xinput=(base1<<16)+base2;
			//writing into the XY input
			XGpioPs_WriteReg( XPAR_PS7_GPIO_0_BASEADDR, XGPIOPS_DATA_2 , (Xinput) );

#ifdef DEBUG_0
			sprintf(InputToPrint, "%u\n\r", Xinput);
			sprintf(baseone,"%i", base1);
			sprintf(basetwo,"%i\n\r", base2);
			print("input=");
			print(InputToPrint);
			print("as=");
			print(baseone);
			print("+");
			print(basetwo);
#endif

			//reading R output
			int Xoutput=XGpioPs_ReadReg( XPAR_PS7_GPIO_0_BASEADDR, XGPIOPS_DATA_2_RO  );

#ifdef DEBUG_0
			sprintf(OutputToPrint, "%i\n\r", Xoutput);
			print("output=");
			print(OutputToPrint);
#endif
			//check sum (we are adding on 8 bits so check is on 8 bits)
			if (Xoutput==(uint16_t)(base1+base2)) {

#ifdef DEBUG_1
				print("Addition ok!\n\r");
#endif
				success++;


			}
#ifdef DEBUG_1
				else {
					print("You failed, moron!\n\r");
				}
#endif
				testnumber++;

#ifdef DEBUG_1
				usleep(_DELAY);
#endif


			base2++;
		}
		base1++;
		base2=0;
	}

	sprintf(result,"%u", success);
	sprintf(maxcount,"%u", testnumber);
	print("number of tests passed= ");
	print(result);
	print(" on ");
	print(maxcount);
	print("\n\r");

	return 0;
}
