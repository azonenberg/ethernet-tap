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

#include "ethernet-tap.h"
#include "TapCLISessionContext.h"
#include <ctype.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command table

//List of all valid command tokens
enum cmdid_t
{
	CMD_EXIT,
	CMD_INTERFACE,
	CMD_MMD,
	CMD_MONA,
	CMD_MONB,
	CMD_PORTA,
	CMD_PORTB,
	CMD_REGISTER,
	CMD_RELOAD,
	CMD_SHOW,
	CMD_STATUS
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "interface"

static const clikeyword_t g_interfaceCommands[] =
{
	{"mona",			CMD_MONA,				NULL,						"Monitor of port A"},
	{"monb",			CMD_MONB,				NULL,						"Monitor of port B"},
	{"porta",			CMD_PORTA,				NULL,						"Left tap port"},
	{"portb",			CMD_PORTB,				NULL,						"Right tap port"},

	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "show" (top level)

static const clikeyword_t g_showInterfaceCommands[] =
{
	{"status",			CMD_STATUS,				NULL,						"Print status of interfaces"},

	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

static const clikeyword_t g_showCommands[] =
{
	{"interface",		CMD_INTERFACE,			g_showInterfaceCommands,	"Print interface information"},
	//{"hardware",		CMD_HARDWARE,			NULL,						"Print hardware information"},

	{NULL,				INVALID_COMMAND,		NULL,	NULL}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "show" (interface mode)

static const clikeyword_t g_showRegisterCommands[] =
{
	{"<regid>",			FREEFORM_TOKEN,			NULL,						"Hexadecimal register address"},

	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

static const clikeyword_t g_showMmdRegisterCommands[] =
{
	{"register",		CMD_REGISTER,			g_showRegisterCommands,		"Register within the MMD"},

	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

static const clikeyword_t g_showMmdCommands[] =
{
	{"<mmdid>",			FREEFORM_TOKEN,			g_showMmdRegisterCommands,	"Hexadecimal MMD index"},

	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

static const clikeyword_t g_interfaceShowCommands[] =
{
	{"mmd",				CMD_MMD,				g_showMmdCommands,			"Read MMD registers"},
	{"register",		CMD_REGISTER,			g_showRegisterCommands,		"Read PHY registers"},

	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level command list

static const clikeyword_t g_rootCommands[] =
{
	{"interface",		CMD_INTERFACE,			g_interfaceCommands,		"Interface properties"},
	{"reload",			CMD_RELOAD,				NULL,						"Restart the system"},
	{"show",			CMD_SHOW,				g_showCommands,				"Print information"},
	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level command list for interface mode

static const clikeyword_t g_interfaceRootCommands[] =
{
	{"exit",			CMD_EXIT,				NULL,						"Exit to the main menu"},
	{"interface",		CMD_INTERFACE,			g_interfaceCommands,		"Interface properties"},
	{"show",			CMD_SHOW,				g_interfaceShowCommands,	"Print information"},
	{NULL,				INVALID_COMMAND,		NULL,						NULL}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TapCLISessionContext::TapCLISessionContext()
	: CLISessionContext(g_rootCommands)
	, m_stream(NULL)
	, m_activeInterface(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Prompt

void TapCLISessionContext::PrintPrompt()
{
	if(m_rootCommands == g_interfaceRootCommands)
		m_stream->Printf("tap(%s)$ ", g_portDescriptions[m_activeInterface]);
	else
		m_stream->Printf("tap$ ");
	m_stream->Flush();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level command dispatch

void TapCLISessionContext::OnExecute()
{
	switch(m_command[0].m_commandID)
	{
		case CMD_EXIT:
			m_rootCommands = g_rootCommands;
			break;

		case CMD_INTERFACE:
			OnInterfaceCommand();
			break;

		case CMD_RELOAD:
			OnReload();
			break;

		case CMD_SHOW:
			OnShowCommand();
			break;

		/*case CMD_ZEROIZE:
			if(m_command[1].m_commandID == CMD_ALL)
				OnZeroize();
			break;*/

		default:
			break;
	}
	m_stream->Flush();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "interface"

void TapCLISessionContext::OnInterfaceCommand()
{
	switch(m_command[1].m_commandID)
	{
		case CMD_PORTA:
			m_activeInterface = 0;
			break;

		case CMD_PORTB:
			m_activeInterface = 1;
			break;

		case CMD_MONA:
			m_activeInterface = 2;
			break;

		case CMD_MONB:
			m_activeInterface = 3;
			break;
	}

	m_rootCommands = g_interfaceRootCommands;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "reload"

void TapCLISessionContext::OnReload()
{
	g_log("Reload requested\n");
	SCB.AIRCR = 0x05fa0004;
	while(1)
	{}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "show"

void TapCLISessionContext::OnShowCommand()
{
	switch(m_command[1].m_commandID)
	{
		case CMD_INTERFACE:
			switch(m_command[2].m_commandID)
			{
				case CMD_STATUS:
					OnShowInterfaceStatus();
					break;

				default:
					break;
			}
			break;

		case CMD_MMD:
			OnShowMmdRegister();
			break;

		case CMD_REGISTER:
			OnShowRegister();
			break;

		/*
		case CMD_HARDWARE:
			ShowHardware();
			break;
		*/
	}
}

void TapCLISessionContext::OnShowInterfaceStatus()
{
	m_stream->Printf("----------------------------------------------------------------------------------\n");
	m_stream->Printf("Port     Name                 Status          Duplex    Speed    Type\n");
	m_stream->Printf("----------------------------------------------------------------------------------\n");
	for(int i=0; i<4; i++)
	{
		int state = g_linkState >> (i*4);
		auto up = state & 0x8;
		auto speed = state & 3;

		m_stream->Printf("%-5s    %-20s %-15s %-10s %4d    10/100/1000baseT\n",
			g_portDescriptions[i],
			g_portLongDescriptions[i],
			up ? "connected" : "notconnect",
			"full",
			g_linkSpeeds[speed]);
	}
}

void TapCLISessionContext::OnShowMmdRegister()
{
	int mmd = strtol(m_command[2].m_text, nullptr, 16);
	int regid = strtol(m_command[4].m_text, nullptr, 16);
	auto value = PhyRegisterIndirectRead(m_activeInterface, mmd, regid);

	m_stream->Printf("MMD %02x register 0x%04x = 0x%04x\n", mmd, regid, value);
}

void TapCLISessionContext::OnShowRegister()
{
	int regid = strtol(m_command[2].m_text, nullptr, 16);
	auto value = PhyRegisterRead(m_activeInterface, regid);

	m_stream->Printf("Register 0x%02x = 0x%04x\n", regid, value);
}

/*
void TapCLISessionContext::ShowHardware()
{
	uint16_t rev = DBGMCU.IDCODE >> 16;
	uint16_t device = DBGMCU.IDCODE & 0xfff;

	m_stream->Printf("MCU:\n");
	if(device == 0x451)
	{
		//Look up the stepping number
		const char* srev = NULL;
		switch(rev)
		{
			case 0x1000:
				srev = "A";
				break;

			case 0x1001:
				srev = "Z";
				break;

			default:
				srev = "(unknown)";
		}

		uint8_t pkg = (PKG_ID >> 8) & 0x7;
		switch(pkg)
		{
			case 7:
				m_stream->Printf("    STM32F767 / 777 LQFP208/TFBGA216 rev %s (0x%04x)\n", srev, rev);
				break;
			case 6:
				m_stream->Printf("    STM32F769 / 779 LQFP208/TFBGA216 rev %s (0x%04x)\n", srev, rev);
				break;
			case 5:
				m_stream->Printf("    STM32F767 / 777 LQFP176 rev %s (0x%04x)\n", srev, rev);
				break;
			case 4:
				m_stream->Printf("    STM32F769 / 779 LQFP176 rev %s (0x%04x)\n", srev, rev);
				break;
			case 3:
				m_stream->Printf("    STM32F778 / 779 WLCSP180 rev %s (0x%04x)\n", srev, rev);
				break;
			case 2:
				m_stream->Printf("    STM32F767 / 777 LQFP144 rev %s (0x%04x)\n", srev, rev);
				break;
			case 1:
				m_stream->Printf("    STM32F767 / 777 LQFP100 rev %s (0x%04x)\n", srev, rev);
				break;
			default:
				m_stream->Printf("    Unknown/reserved STM32F76x/F77x rev %s (0x%04x)\n", srev, rev);
				break;
		}
		m_stream->Printf("    512 kB total SRAM, 128 kB DTCM, 16 kB ITCM, 4 kB backup SRAM\n");
		m_stream->Printf("    %d kB Flash\n", F_ID);

		//U_ID fields documented in 45.1 of STM32 programming manual
		uint16_t waferX = U_ID[0] >> 16;
		uint16_t waferY = U_ID[0] & 0xffff;
		uint8_t waferNum = U_ID[1] & 0xff;
		char waferLot[8] =
		{
			static_cast<char>((U_ID[1] >> 24) & 0xff),
			static_cast<char>((U_ID[1] >> 16) & 0xff),
			static_cast<char>((U_ID[1] >> 8) & 0xff),
			static_cast<char>((U_ID[2] >> 24) & 0xff),
			static_cast<char>((U_ID[2] >> 16) & 0xff),
			static_cast<char>((U_ID[2] >> 8) & 0xff),
			static_cast<char>((U_ID[2] >> 0) & 0xff),
			'\0'
		};
		m_stream->Printf("    Lot %s, wafer %d, die (%d, %d)\n", waferLot, waferNum, waferX, waferY);

		if(g_hasRmiiErrata)
			m_stream->Printf("    RMII RXD0 errata present\n");
	}
	else
		m_stream->Printf("Unknown device (0x%06x)\n", device);

	//Print CPU info
	if( (SCB.CPUID & 0xff00fff0) == 0x4100c270 )
	{
		m_stream->Printf("ARM Cortex-M7 r%dp%d\n", (SCB.CPUID >> 20) & 0xf, (SCB.CPUID & 0xf));
		if(CPUID.CLIDR & 2)
		{
			m_stream->Printf("    L1 data cache present\n");
			CPUID.CCSELR = 0;

			int sets = ((CPUID.CCSIDR >> 13) & 0x7fff) + 1;
			int ways = ((CPUID.CCSIDR >> 3) & 0x3ff) + 1;
			int words = 1 << ((CPUID.CCSIDR & 3) + 2);
			int total = (sets * ways * words * 4) / 1024;
			m_stream->Printf("        %d sets, %d ways, %d words per line, %d kB total\n",
				sets, ways, words, total);
		}
		if(CPUID.CLIDR & 1)
		{
			m_stream->Printf("    L1 instruction cache present\n");
			CPUID.CCSELR = 1;

			int sets = ((CPUID.CCSIDR >> 13) & 0x7fff) + 1;
			int ways = ((CPUID.CCSIDR >> 3) & 0x3ff) + 1;
			int words = 1 << ((CPUID.CCSIDR & 3) + 2);
			int total = (sets * ways * words * 4) / 1024;
			m_stream->Printf("        %d sets, %d ways, %d words per line, %d kB total\n",
				sets, ways, words, total);
		}
	}
	else
		m_stream->Printf("Unknown CPU (0x%08x)\n", SCB.CPUID);

	m_stream->Printf("Ethernet MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n",
		g_macAddress[0], g_macAddress[1], g_macAddress[2], g_macAddress[3], g_macAddress[4], g_macAddress[5]);
}
*/
