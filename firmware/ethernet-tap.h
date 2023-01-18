/***********************************************************************************************************************
*                                                                                                                      *
* ethernet-tap v0.1                                                                                                    *
*                                                                                                                      *
* Copyright (c) 2023 Andrew D. Zonenberg and contributors                                                              *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#ifndef ethernet_tap_h
#define ethernet_tap_h

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stm32.h>

#include <peripheral/Flash.h>
#include <peripheral/GPIO.h>
#include <peripheral/OctoSPI.h>
#include <peripheral/OctoSPIManager.h>
#include <peripheral/Power.h>
#include <peripheral/RCC.h>
#include <peripheral/Timer.h>
#include <peripheral/UART.h>
#include <util/Logger.h>
#include <util/FIFO.h>
#include <cli/UARTOutputStream.h>

#include "TapCLISessionContext.h"

extern UART* g_cliUART;
extern Logger g_log;
extern UARTOutputStream g_uartStream;
extern OctoSPI* g_qspi;

extern TapCLISessionContext g_uartCliContext;

extern const char* g_portDescriptions[4];
extern const char* g_portLongDescriptions[4];
extern const int g_linkSpeeds[4];
extern uint16_t g_linkState;

//Register IDs for the FPGA
enum regids
{
	//Constants that aren't actual registers
	REG_ETH_OFFSET		= 0x1000,

	//Global registers
	REG_FPGA_IDCODE		= 0x0000,
	REG_FPGA_SERIAL		= 0x0001,
	REG_LINK_STATE		= 0x0002,

	//Port 0 (device A)
	REG_ETH0_RST		= 0x1000,
	REG_ETH0_MDIO_RADDR	= 0x1001,
	REG_ETH0_MDIO_RDATA	= 0x1002,
	REG_ETH0_MDIO_WR	= 0x1003,

	//Port 1 (device B)
	REG_ETH1_RST		= 0x2000,
	REG_ETH1_MDIO_RADDR	= 0x2001,
	REG_ETH1_MDIO_RDATA	= 0x2002,
	REG_ETH1_MDIO_WR	= 0x2003,

	//Port 2 (mon A)
	REG_ETH2_RST		= 0x3000,
	REG_ETH2_MDIO_RADDR	= 0x3001,
	REG_ETH2_MDIO_RDATA	= 0x3002,
	REG_ETH2_MDIO_WR	= 0x3003,

	//Port 3 (mon B)
	REG_ETH3_RST		= 0x4000,
	REG_ETH3_MDIO_RADDR	= 0x4001,
	REG_ETH3_MDIO_RDATA	= 0x4002,
	REG_ETH3_MDIO_WR	= 0x4003
};

enum phyregs
{
	//direct
	PHY_REG_BASIC_CONTROL	= 0x0000,
	PHY_REG_ID1				= 0x0002,
	PHY_REG_ID2				= 0x0003,
	PHY_REG_AN_NEXT_PAGE	= 0x0007,
	PHY_REG_MMD_CTRL		= 0x000d,
	PHY_REG_MMD_DATA		= 0x000e,

	//indirect mmd0
	PHY_REG_MMD0_FLP_LO		= 0x0003,
	PHY_REG_MMD0_FLP_HI		= 0x0004

	//indirect mmd1
};

#endif
