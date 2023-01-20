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
	CMD_AUTO,
	CMD_10,
	CMD_100,
	CMD_1000,
	CMD_EXIT,
	CMD_INTERFACE,
	CMD_MMD,
	CMD_MONA,
	CMD_MONB,
	CMD_NO,
	CMD_PORTA,
	CMD_PORTB,
	CMD_REGISTER,
	CMD_RELOAD,
	CMD_SET,
	CMD_SHOW,
	CMD_SPEED,
	CMD_STATUS
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "interface"

static const clikeyword_t g_interfaceCommands[] =
{
	{"mona",			CMD_MONA,				nullptr,					"Monitor of port A"},
	{"monb",			CMD_MONB,				nullptr,					"Monitor of port B"},
	{"porta",			CMD_PORTA,				nullptr,					"Left tap port"},
	{"portb",			CMD_PORTB,				nullptr,					"Right tap port"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "show" (top level)

static const clikeyword_t g_showInterfaceCommands[] =
{
	{"status",			CMD_STATUS,				nullptr,					"Print status of interfaces"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_showCommands[] =
{
	{"interface",		CMD_INTERFACE,			g_showInterfaceCommands,	"Print interface information"},
	//{"hardware",		CMD_HARDWARE,			nullptr,					"Print hardware information"},

	{nullptr,			INVALID_COMMAND,		nullptr,	nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "set"

static const clikeyword_t g_setRegisterValues[] =
{
	{"<value>",			FREEFORM_TOKEN,			nullptr,					"Hexadecimal register value"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_setRegisterCommands[] =
{
	{"<regid>",			FREEFORM_TOKEN,			g_setRegisterValues,		"Hexadecimal register address"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_interfaceSetCommands[] =
{
	//{"mmd",			CMD_MMD,				g_showMmdCommands,			"Set MMD registers"},
	{"register",		CMD_REGISTER,			g_setRegisterCommands,		"Set PHY registers"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "show" (interface mode)

static const clikeyword_t g_showRegisterCommands[] =
{
	{"<regid>",			FREEFORM_TOKEN,			nullptr,					"Hexadecimal register address"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_showMmdRegisterCommands[] =
{
	{"register",		CMD_REGISTER,			g_showRegisterCommands,		"Register within the MMD"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_showMmdCommands[] =
{
	{"<mmdid>",			FREEFORM_TOKEN,			g_showMmdRegisterCommands,	"Hexadecimal MMD index"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_interfaceShowCommands[] =
{
	{"mmd",				CMD_MMD,				g_showMmdCommands,			"Read MMD registers"},
	{"register",		CMD_REGISTER,			g_showRegisterCommands,		"Read PHY registers"},
	{"speed",			CMD_SPEED,				nullptr,					"Display interface speed options"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "speed"

static const clikeyword_t g_interfaceSpeedCommands[] =
{
	{"10",				CMD_10,					nullptr,					"10baseT"},
	{"100",				CMD_100,				nullptr,					"100baseTX"},
	{"1000",			CMD_1000,				nullptr,					"1000baseT"},
	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "no" (interface mode)

static const clikeyword_t g_interfaceNoCommands[] =
{
	{"speed",			CMD_SPEED,				g_interfaceSpeedCommands,	"Turn off advertisement of a specific speed"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level command list

static const clikeyword_t g_rootCommands[] =
{
	{"interface",		CMD_INTERFACE,			g_interfaceCommands,		"Interface properties"},
	{"reload",			CMD_RELOAD,				nullptr,					"Restart the system"},
	{"show",			CMD_SHOW,				g_showCommands,				"Print information"},
	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level command list for interface mode

static const clikeyword_t g_interfaceRootCommands[] =
{
	{"end",				CMD_EXIT,				nullptr,					"Exit to the main menu"},
	{"exit",			CMD_EXIT,				nullptr,					"Exit to the main menu"},
	{"interface",		CMD_INTERFACE,			g_interfaceCommands,		"Interface properties"},
	{"no",				CMD_NO,					g_interfaceNoCommands,		"Turn settings off"},
	{"set",				CMD_SET,				g_interfaceSetCommands,		"Set raw hardware registers"},
	{"show",			CMD_SHOW,				g_interfaceShowCommands,	"Print information"},
	{"speed",			CMD_SPEED,				g_interfaceSpeedCommands,	"Set port operating speed"},
	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TapCLISessionContext::TapCLISessionContext()
	: CLISessionContext(g_rootCommands)
	, m_stream(nullptr)
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

		case CMD_NO:
			OnNoCommand();
			break;

		case CMD_RELOAD:
			OnReload();
			break;

		case CMD_SET:
			OnSetCommand();
			break;

		case CMD_SHOW:
			OnShowCommand();
			break;

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

		default:
			break;
	}

	m_rootCommands = g_interfaceRootCommands;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "no"

void TapCLISessionContext::OnNoCommand()
{
	switch(m_command[1].m_commandID)
	{
		case CMD_SPEED:
			OnNoSpeed();

		default:
			break;
	}
}

void TapCLISessionContext::OnNoSpeed()
{
	//10/100 speeds are in the AN base page advertisement register
	if( (m_command[2].m_commandID == CMD_10) || (m_command[2].m_commandID == CMD_100) )
	{
		auto adv = PhyRegisterRead(m_activeInterface, PHY_REG_AN_ADVERT);
		if(m_command[2].m_commandID == CMD_100)
			adv &= ~0x100;
		else
			adv &= ~0x40;
		PhyRegisterWrite(m_activeInterface, PHY_REG_AN_ADVERT, adv);
	}

	//Gigabit speeds are in the 1000baseT control register
	else
	{
		auto mode = PhyRegisterRead(m_activeInterface, PHY_REG_GIG_CONTROL);
		mode &= ~0x200;
		PhyRegisterWrite(m_activeInterface, PHY_REG_GIG_CONTROL, mode);
	}

	//Restart negotiation
	RestartNegotiation(m_activeInterface);
}

void TapCLISessionContext::RestartNegotiation(int nport)
{
	auto base = PhyRegisterRead(m_activeInterface, PHY_REG_BASIC_CONTROL);
	PhyRegisterWrite(m_activeInterface, PHY_REG_BASIC_CONTROL, base | 0x0200);
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
// "set"

void TapCLISessionContext::OnSetCommand()
{
	switch(m_command[1].m_commandID)
	{
		/*
		case CMD_MMD:
			OnShowMmdRegister();
			break;
		*/

		case CMD_REGISTER:
			OnSetRegister();
			break;
	}
}

void TapCLISessionContext::OnSetRegister()
{
	int regid = strtol(m_command[2].m_text, nullptr, 16);
	int value = strtol(m_command[3].m_text, nullptr, 16);
	PhyRegisterWrite(m_activeInterface, regid, value);
	m_stream->Printf("Set register 0x%02x to 0x%04x\n", regid, value);
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

		case CMD_SPEED:
			OnShowSpeed();
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

void TapCLISessionContext::OnShowSpeed()
{
	auto bc = PhyRegisterRead(m_activeInterface, PHY_REG_BASIC_CONTROL);
	if(bc & 0x1000)
	{
		m_stream->Printf("Autonegotiation enabled\n");
		m_stream->Printf("Advertising:\n");

		auto gig = PhyRegisterRead(m_activeInterface, PHY_REG_GIG_CONTROL);
		if(gig & 0x0200)
			m_stream->Printf("    1000baseT full duplex\n");

		auto ad = PhyRegisterRead(m_activeInterface, PHY_REG_AN_ADVERT);
		if(ad & 0x100)
			m_stream->Printf("    100baseTX full duplex\n");
		if(ad & 0x40)
			m_stream->Printf("    10baseT full duplex\n");
	}
	else
		m_stream->Printf("Autonegotiation disabled\n");
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
		const char* srev = nullptr;
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
