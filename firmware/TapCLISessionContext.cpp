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
	CMD_AUTONEGOTIATION,
	CMD_10,
	CMD_100,
	CMD_1000,
	CMD_DETAIL,
	CMD_EXIT,
	CMD_HARDWARE,
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
	CMD_STATUS,
	CMD_TEST,
	CMD_VERSION,
	CMD_VOLATILITY
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
	{"hardware",		CMD_HARDWARE,			nullptr,					"Print hardware information"},
	{"version",			CMD_VERSION,			nullptr,					"Print firmware version information"},
	{"volatility",		CMD_VOLATILITY,			nullptr,					"Print Statement of Volatility"},

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

static const clikeyword_t g_setMmdRegisterCommands[] =
{
	{"register",		CMD_REGISTER,			g_setRegisterCommands,		"Register within the MMD"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_setMmdCommands[] =
{
	{"<mmdid>",			FREEFORM_TOKEN,			g_setMmdRegisterCommands,	"Hexadecimal MMD index"},

	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

static const clikeyword_t g_interfaceSetCommands[] =
{
	{"mmd",				CMD_MMD,				g_setMmdCommands,			"Set MMD registers"},
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
	{"detail",			CMD_DETAIL,				nullptr,					"Print detailed interface information"},
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
	{"autonegotiation",	CMD_AUTONEGOTIATION,	nullptr,					"Disable autonegotiation"},
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
	{"test",			CMD_TEST,				g_interfaceCommands,		"Run a cable test"},
	{nullptr,			INVALID_COMMAND,		nullptr,					nullptr}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level command list for interface mode

static const clikeyword_t g_interfaceRootCommands[] =
{
	//TODO: master/slave state
	//TODO: test modes
	//TODO: mdi-x
	{"autonegotiation",	CMD_AUTONEGOTIATION,	nullptr,					"Enable autonegotiation"},
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
		case CMD_AUTONEGOTIATION:
			OnAutonegotiation();
			break;

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

		case CMD_SPEED:
			OnSpeed();
			break;

		case CMD_SHOW:
			OnShowCommand();
			break;

		case CMD_TEST:
			OnTest();
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
		case CMD_AUTONEGOTIATION:
			OnNoAutonegotiation();
			break;

		case CMD_SPEED:
			OnNoSpeed();
			break;

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
		case CMD_MMD:
			OnSetMmdRegister();
			break;

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

void TapCLISessionContext::OnSetMmdRegister()
{
	int mmd = strtol(m_command[2].m_text, nullptr, 16);
	int regid = strtol(m_command[4].m_text, nullptr, 16);
	auto value = strtol(m_command[5].m_text, nullptr, 16);
	PhyRegisterIndirectWrite(m_activeInterface, mmd, regid, value);
	m_stream->Printf("Set MMD %02x register 0x%04x to 0x%04x\n", mmd, regid, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "show"

void TapCLISessionContext::OnShowCommand()
{
	switch(m_command[1].m_commandID)
	{
		case CMD_DETAIL:
			OnShowDetail();
			break;

		case CMD_HARDWARE:
			OnShowHardware();
			break;

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

		case CMD_VERSION:
			OnShowVersion();
			break;

		case CMD_VOLATILITY:
			OnShowVolatility();
			break;
	}
}

void TapCLISessionContext::OnShowDetail()
{
	auto base = PhyRegisterRead(m_activeInterface, PHY_REG_BASIC_CONTROL);
	auto status = PhyRegisterRead(m_activeInterface, PHY_REG_BASIC_STATUS);
	bool aneg = (base & 0x1000) == 0x1000;

	//RGMII in-band status
	int rgmiiState = g_linkState >> (m_activeInterface*4);
	bool rgmiiUp = (rgmiiState & 0x8) == 0x8;
	int rgmiiSpeed = rgmiiState & 3;

	m_stream->Printf("RGMII In-Band Status\n");
	if(rgmiiUp)
		m_stream->Printf("    Link up\n");
	else
		m_stream->Printf("    Link down\n");
	switch(rgmiiSpeed)
	{
		case 0:
			m_stream->Printf("    10baseT\n");
			break;
		case 1:
			m_stream->Printf("    100baseTX\n");
			break;
		case 2:
			m_stream->Printf("    1000baseT\n");
			break;
		default:
			m_stream->Printf("    Invalid (reserved) speed\n");
			break;
	}

	//Basic control register
	m_stream->Printf("Basic Control = 0x%04x\n", base);
	if(base & 0x4000)
		m_stream->Printf("    Loopback enabled\n");
	else
		m_stream->Printf("    Loopback disabled\n");

	if(base & 0x1000)
		m_stream->Printf("    Autonegotiation enabled\n");
	else
		m_stream->Printf("    Autonegotiation disabled\n");

	if(base & 0x0800)
		m_stream->Printf("    Powered down\n");
	else
		m_stream->Printf("    Powered up\n");

	if(base & 0x0400)
		m_stream->Printf("    RGMII isolation\n");
	else
		m_stream->Printf("    RGMII operating normally\n");

	if(base & 0x0100)
		m_stream->Printf("    Full duplex\n");
	else
		m_stream->Printf("    Half duplex (not tested or supported in FPGA firmware, may cause problems)\n");

	auto speed = base & 0x2040;
	if(speed == 0)
		m_stream->Printf("    Speed select: 10baseT\n");
	else if(speed == 0x2000)
		m_stream->Printf("    Speed select: 100baseTX\n");
	else if(speed == 0x0040)
		m_stream->Printf("    Speed select: 1000baseT\n");
	else
		m_stream->Printf("    Invalid speed\n");

	//Basic status register
	m_stream->Printf("Basic Status = 0x%04x\n", status);
	if(status & 0x0020)
		m_stream->Printf("    Autonegotiation completed\n");
	else
		m_stream->Printf("    Autonegotiation not completed\n");
	if(status & 0x0010)
		m_stream->Printf("    Remote fault\n");
	else
		m_stream->Printf("    No remote fault detected\n");
	if(status & 0x4)
		m_stream->Printf("    Link up\n");
	else
		m_stream->Printf("    Link down\n");
	if(status & 0x2)
		m_stream->Printf("    Jabber detected\n");
	else
		m_stream->Printf("    No jabber detected\n");

	//Autonegotiation advertisement
	auto ad = PhyRegisterRead(m_activeInterface, PHY_REG_AN_ADVERT);
	m_stream->Printf("Autonegotiation Advertisement = 0x%04x\n", ad);
	if(ad & 0x8000)
		m_stream->Printf("    Next page\n");
	else
		m_stream->Printf("    No next page\n");
	if(ad & 0x2000)
		m_stream->Printf("    Remote fault\n");
	else
		m_stream->Printf("    No remote fault\n");
	switch( (ad >> 10) & 3)
	{
		case 0:
			m_stream->Printf("    No pause\n");
			break;

		case 1:
			m_stream->Printf("    Asymmetric pause\n");
			break;

		case 2:
			m_stream->Printf("    Symmetric pause\n");
			break;

		case 3:
			m_stream->Printf("    Symmetric or asymmetric pause\n");
			break;
	}
	if(ad & 0x100)
		m_stream->Printf("    100baseTX full duplex capable\n");
	else
		m_stream->Printf("    Not 100baseTX full duplex capable\n");
	if(ad & 0x80)
		m_stream->Printf("    100baseTX half duplex capable\n");
	else
		m_stream->Printf("    Not 100baseTX half duplex capable\n");
	if(ad & 0x40)
		m_stream->Printf("    10baseT full duplex capable\n");
	else
		m_stream->Printf("    Not 10baseT full duplex capable\n");
	if(ad & 0x20)
		m_stream->Printf("    10baseT half duplex capable\n");
	else
		m_stream->Printf("    Not 10baseT half duplex capable\n");
	auto sel = (ad & 0x1f);
	if(sel == 0x1)
		m_stream->Printf("    Selector: 0x01 (Ethernet)\n");
	else
		m_stream->Printf("    Selector: 0x%02x (invalid)\n", sel);

	//Autonegotiation advertisement
	ad = PhyRegisterRead(m_activeInterface, PHY_REG_AN_PARTNER);
	m_stream->Printf("Autonegotiation Link Partner Ability = 0x%04x\n", ad);
	if(ad & 0x8000)
		m_stream->Printf("    Next page\n");
	else
		m_stream->Printf("    No next page\n");
	if(ad & 0x4000)
		m_stream->Printf("    ACK\n");
	else
		m_stream->Printf("    No ACK\n");
	if(ad & 0x2000)
		m_stream->Printf("    Remote fault\n");
	else
		m_stream->Printf("    No remote fault\n");
	switch( (ad >> 10) & 3)
	{
		case 0:
			m_stream->Printf("    No pause\n");
			break;

		case 1:
			m_stream->Printf("    Asymmetric pause\n");
			break;

		case 2:
			m_stream->Printf("    Symmetric pause\n");
			break;

		case 3:
			m_stream->Printf("    Symmetric or asymmetric pause\n");
			break;
	}
	if(ad & 0x200)
		m_stream->Printf("    100baseT4x capable\n");
	else
		m_stream->Printf("    Not 100baseT4 capable\n");
	if(ad & 0x100)
		m_stream->Printf("    100baseTX full duplex capable\n");
	else
		m_stream->Printf("    Not 100baseTX full duplex capable\n");
	if(ad & 0x80)
		m_stream->Printf("    100baseTX half duplex capable\n");
	else
		m_stream->Printf("    Not 100baseTX half duplex capable\n");
	if(ad & 0x40)
		m_stream->Printf("    10baseT full duplex capable\n");
	else
		m_stream->Printf("    Not 10baseT full duplex capable\n");
	if(ad & 0x20)
		m_stream->Printf("    10baseT half duplex capable\n");
	else
		m_stream->Printf("    Not 10baseT half duplex capable\n");
	sel = (ad & 0x1f);
	if(sel == 0x1)
		m_stream->Printf("    Selector: 0x01 (Ethernet)\n");
	else
		m_stream->Printf("    Selector: 0x%02x (invalid)\n", sel);

	//Block to avoid spamming the screen with more content than can fit
	More();

	//AN expansion
	auto exp = PhyRegisterRead(m_activeInterface, PHY_REG_AN_EXPANSION);
	m_stream->Printf("Autonegotiation Expansion = 0x%04x\n", exp);
	if(exp & 0x10)
		m_stream->Printf("    Parallel detection: fault\n");
	else
		m_stream->Printf("    Parallel detection: no fault\n");
	if(exp & 0x8)
		m_stream->Printf("    Link partner has next page capability\n");
	else
		m_stream->Printf("    Link partner lacks next page capability\n");
	if(exp & 0x1)
		m_stream->Printf("    Link partner has autonegotiation capability\n");
	else
		m_stream->Printf("    Link partner lacks autonegotiation capability\n");

	//Don't dump AN_NEXT_PAGE as that's outbound only

	//Link partner next page
	auto pnp = PhyRegisterRead(m_activeInterface, PHY_REG_AN_PARTNER_NEXT_PAGE);
	m_stream->Printf("Link Partner Next Page = 0x%04x\n", pnp);
	if(pnp & 0x8000)
		m_stream->Printf("    Additional pages to follow\n");
	else
		m_stream->Printf("    Last page\n");
	if(pnp & 0x4000)
		m_stream->Printf("    ACK\n");
	else
		m_stream->Printf("    NAK\n");
	if(pnp & 0x2000)
		m_stream->Printf("    Message page\n");
	else
		m_stream->Printf("    Unformatted page\n");
	if(pnp & 0x1000)
		m_stream->Printf("    ACK2\n");
	else
		m_stream->Printf("    NAK2\n");
	m_stream->Printf("    Toggle = %d\n", (pnp >> 11) & 1);
	m_stream->Printf("    Message = 0x%03x\n", pnp & 0x3ff);

	//1000base-T Control
	auto gig = PhyRegisterRead(m_activeInterface, PHY_REG_GIG_CONTROL);
	m_stream->Printf("1000Base-T Control = 0x%04x\n", gig);
	int test = (gig >> 13) & 7;
	switch(test)
	{
		case 0:
			m_stream->Printf("    Normal operation\n");
			break;
		case 1:
			m_stream->Printf("    Waveform test\n");
			break;
		case 2:
			m_stream->Printf("    Jitter test (master mode)\n");
			break;
		case 3:
			m_stream->Printf("    Jitter test (slave mode)\n");
			break;
		case 4:
			m_stream->Printf("    Distortion test\n");
			break;

		default:
			m_stream->Printf("    Test mode %d (reserved)\n", test);
			break;
	}

	if(gig & 0x1000)
	{
		if(gig & 0x0800)
			m_stream->Printf("    Master mode only\n");
		else
			m_stream->Printf("    Slave mode only\n");
	}
	else
	{
		if(gig & 0x0400)
			m_stream->Printf("    Negotiate mode, prefer master\n");
		else
			m_stream->Printf("    Negotiate mode, prefer slave\n");
	}

	if(gig & 0x0200)
		m_stream->Printf("    Advertise 1000baseT full duplex\n");
	else
		m_stream->Printf("    Do not advertise 1000baseT full duplex\n");

	if(gig & 0x0100)
		m_stream->Printf("    Advertise 1000baseT half duplex\n");
	else
		m_stream->Printf("    Do not advertise 1000baseT half duplex\n");

	//1000base-T Status
	auto gstat = PhyRegisterRead(m_activeInterface, PHY_REG_GIG_STATUS);
	m_stream->Printf("1000Base-T Status = 0x%04x\n", gstat);
	if(gstat & 0x8000)
		m_stream->Printf("    Master-slave configuration fault\n");
	else
		m_stream->Printf("    No configuration fault\n");
	if(gstat & 0x4000)
		m_stream->Printf("    Operating in master mode\n");
	else
		m_stream->Printf("    Operating in slave mode\n");
	if(gstat & 0x2000)
		m_stream->Printf("    Local receiver OK\n");
	else
		m_stream->Printf("    Local receiver not OK\n");
	if(gstat & 0x1000)
		m_stream->Printf("    Remote receiver OK\n");
	else
		m_stream->Printf("    Remote receiver not OK\n");
	if(gstat & 0x0800)
		m_stream->Printf("    Link partner capable of 1000baseT full duplex\n");
	else
		m_stream->Printf("    Link partner not capable of 1000baseT full duplex\n");
	if(gstat & 0x0400)
		m_stream->Printf("    Link partner capable of 1000baseT half duplex\n");
	else
		m_stream->Printf("    Link partner not capable of 1000baseT half duplex\n");
	m_stream->Printf("    Idle error count: %d\n", gstat & 0xff);

	m_stream->Printf("-- Vendor specific registers after this point --\n");

	//no register at 0x0b, 0x0c

	//registers 0x0d, 0x0e used for MMD access

	//register 0x0f is just capabilities, nothing interesting / changeable there

	//no register at 0x10

	//Remote Loopback
	auto rloop = PhyRegisterRead(m_activeInterface, PHY_REG_REMOTE_LOOPBACK);
	m_stream->Printf("Remote Loopback = 0x%04x\n", rloop);
	if(rloop & 0x100)
		m_stream->Printf("    Remote loopback enabled\n");
	else
		m_stream->Printf("    Remote loopback disabled\n");

	//ignore linkmd, we have separate commands for that

	auto pcspma = PhyRegisterRead(m_activeInterface, PHY_REG_DIGITAL_PCS_PMA);
	m_stream->Printf("Digital PCS / PMA Status = 0x%04x\n", pcspma);
	if(pcspma & 0x4)
		m_stream->Printf("    1000baseT link OK\n");
	else
		m_stream->Printf("    1000baseT link not OK\n");

	if(pcspma & 0x2)
		m_stream->Printf("    100baseTX link OK\n");
	else
		m_stream->Printf("    100baseTX link not OK\n");

	//no register at 0x14

	auto rxer = PhyRegisterRead(m_activeInterface, PHY_REG_RX_ER);
	m_stream->Printf("RX Error Count = %d\n", rxer);

	//No registers at 0x16 - 0x1a

	//Register 0x1B is interrupt enables, ignore: interrupt pin isn't even connected

	auto mdix = PhyRegisterRead(m_activeInterface, PHY_REG_MDIX);
	m_stream->Printf("Auto MDI-X Control = 0x%04x\n", mdix);
	if(mdix & 0x40)
	{
		if(mdix & 0x80)
			m_stream->Printf("    MDI mode only\n");
		else
			m_stream->Printf("    MDI-X mode only\n");
	}
	else
		m_stream->Printf("    Auto MDI-X\n");

	//No register at 0x1d - 0x1e

	More();

	//PHY Control
	auto ctrl = PhyRegisterRead(m_activeInterface, PHY_REG_CTRL);
	m_stream->Printf("PHY Control = 0x%04x\n", ctrl);
	if(ctrl & 0x200)
		m_stream->Printf("    Jabber counter enabled\n");
	else
		m_stream->Printf("    Jabber counter disabled\n");
	if(ctrl & 0x40)
		m_stream->Printf("    Resolved speed to 1000base-T\n");
	if(ctrl & 0x20)
		m_stream->Printf("    Resolved speed to 100base-TX\n");
	if(ctrl & 0x10)
		m_stream->Printf("    Resolved speed to 10base-T\n");
	if(ctrl & 0x8)
		m_stream->Printf("    Full duplex mode\n");
	else
		m_stream->Printf("    Half duplex mode\n");
	if(ctrl & 0x4)
		m_stream->Printf("    1000base-T master mode\n");
	else
		m_stream->Printf("    1000base-T slave mode\n");
	if(ctrl & 0x1)
		m_stream->Printf("    Link failed\n");
	else
		m_stream->Printf("    Link not failed\n");

	//TODO: AN FLP interval? Not something we ever tweak
	//TODO: link up time setting?

	//Ignore strap overrides

	//Ignore pad skew control

	//Ignore WoL config

	//Ignore analog control for 10baseTe mode

	//Ignore EDPD mode
}

/**
	@brief Block until the user pushes a button, but still update I/O
 */
void TapCLISessionContext::More()
{
	m_stream->Printf("---- More ----\n");
	m_stream->Flush();
	while(!g_cliUART->HasInput())
		PollIO();
	g_cliUART->BlockingRead();
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
	{
		m_stream->Printf("Autonegotiation disabled\n");

		auto speed = bc & 0x2040;
		if(speed == 0)
			m_stream->Printf("Speed forced to 10baseT\n");
		else if(speed == 0x2000)
			m_stream->Printf("Speed forced to 100baseTX\n");
		else if(speed == 0x0040)
			m_stream->Printf("Speed forced to 1000baseT\n");
		else
			m_stream->Printf("Invalid speed\n");
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

void TapCLISessionContext::OnShowVersion()
{
	m_stream->Printf("Active Ethernet tap v0.1\n");
	m_stream->Printf("by Andrew D. Zonenberg\n");
	m_stream->Printf("\n");
	m_stream->Printf("This system is open hardware! Board design files and firmware/gateware source code are at:\n");
	m_stream->Printf("https://github.com/azonenberg/ethernet-tap\n");
	m_stream->Printf("\n");
	m_stream->Printf("Firmware compiled at %s on %s\n", __TIME__, __DATE__);
	#ifdef __GNUC__
	m_stream->Printf("Compiler: g++ %s\n", __VERSION__);
	m_stream->Printf("CLI source code last modified: %s\n", __TIMESTAMP__);
	#endif
}

void TapCLISessionContext::OnShowVolatility()
{
	m_stream->Printf("STATEMENT OF VOLATILITY\n");
	m_stream->Printf("\n");
	m_stream->Printf("This system is designed so that all packet content and configuration is destroyed when power is removed.\n");
	m_stream->Printf("\n");
	m_stream->Printf("As an additional security measure, the management microcontroller is incapable of writing to the FPGA\n");
	m_stream->Printf("boot ROM and the register interface between the MCU and FPGA does not provide access to packet data.\n");
	m_stream->Printf("\n");
	m_stream->Printf("The following components on the board store packet content transiently in SRAM or registers during forwarding.\n");
	m_stream->Printf("This data is overwritten in FIFO order by subsequent packets (4 kB buffer size), or when power is removed.\n");
	m_stream->Printf("* Ethernet PHYs (U2, U6, U5, U9)\n");
	m_stream->Printf("* FPGA (U4)\n");
	m_stream->Printf("\n");
	m_stream->Printf("The internal logic analyzers on the GMII buses (if used) store the content of a single packet into SRAM\n");
	m_stream->Printf("on the FPGA (U9) prior to readout. This data is overwritten the next time the internal logic analyzer\n");
	m_stream->Printf("triggers, or when power is removed.\n");
	m_stream->Printf("\n");
	m_stream->Printf("The following components contain internal Flash memory, but no packet content or configuration:\n");
	m_stream->Printf("* FPGA boot ROM (U14)\n");
	m_stream->Printf("* Management microcontroller (U13)\n");
	m_stream->Printf("\n");
	m_stream->Printf("These components may be erased via JTAG if complete zeroization of the board is desired, however doing so\n");
	m_stream->Printf("will render the board unusable until new MCU and FPGA firmware are loaded via JTAG.\n");
	m_stream->Printf("\n");
}

void TapCLISessionContext::OnShowHardware()
{
	//Print main MCU information
	//A lot of this is duplicated by DetectHardware() during boot, but different print formatting
	uint16_t rev = DBGMCU.IDCODE >> 16;
	uint16_t device = DBGMCU.IDCODE & 0xfff;

	if(device == 0x483)
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

		uint8_t pkg = SYSCFG.PKGR;
		const char* package = "";
		switch(pkg)
		{
			case 0:
				package = "VQFPN68 (industrial)";
				break;
			case 1:
				package = "LQFP100/TFBGA100 (legacy)";
				break;
			case 2:
				package = "LQFP100 (industrial)";
				break;
			case 3:
				package = "TFBGA100 (industrial)";
				break;
			case 4:
				package = "WLCSP115 (industrial)";
				break;
			case 5:
				package = "LQFP144 (legacy)";
				break;
			case 6:
				package = "UFBGA144 (legacy)";
				break;
			case 7:
				package = "LQFP144 (industrial)";
				break;
			case 8:
				package = "UFBGA169 (industrial)";
				break;
			case 9:
				package = "UFBGA176+25 (industrial)";
				break;
			case 10:
				package = "LQFP176 (industrial)";
				break;
			default:
				package = "unknown package";
				break;
		}

		m_stream->Printf("CPU: STM32%c%c%c%c stepping %s, %s\n",
			(L_ID >> 24) & 0xff,
			(L_ID >> 16) & 0xff,
			(L_ID >> 8) & 0xff,
			(L_ID >> 0) & 0xff,
			srev,
			package
			);
		m_stream->Printf("    564 kB total SRAM, 128 kB DTCM, up to 256 kB ITCM, 4 kB backup SRAM\n");
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
	}
	else
		g_log(Logger::WARNING, "Unknown device (0x%06x)\n", device);

	//Read the FPGA IDCODE and serial number
	uint8_t buf[8];
	g_qspi->BlockingRead(REG_FPGA_IDCODE, 0, buf, 4);
	uint32_t idcode = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	g_qspi->BlockingRead(REG_FPGA_SERIAL, 0, buf, 8);

	//Print status
	switch(idcode & 0x0fffffff)
	{
		case 0x3620093:
			m_stream->Printf("FPGA IDCODE: %08x (XC7S15 rev %d)\n", idcode, idcode >> 28);
			break;

		default:
			m_stream->Printf("FPGA IDCODE: %08x (unknown device, rev %d)\n", idcode, idcode >> 28);
			break;
	}
	m_stream->Printf("FPGA serial: %02x%02x%02x%02x%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	//Identify each PHY
	for(int port = 0; port < 4; port ++)
	{
		m_stream->Printf("Ethernet interface %d (%s):\n", port, g_portDescriptions[port]);

		//Identify the PHY and make sure it's what we expect
		auto id1 = PhyRegisterRead(port, PHY_REG_ID1);
		auto id2 = PhyRegisterRead(port, PHY_REG_ID2);
		if( (id1 != 0x0022) || ( (id2 >> 4) != 0x162) )
			m_stream->Printf("    Unexpected PHY identifier (ID1=%04x, ID2=%04x)\n", id1, id2);
		else
			m_stream->Printf("    KSZ9031RNX rev %d\n", id2 & 0xf);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "speed"

void TapCLISessionContext::OnSpeed()
{
	//Negotiation on, add this speed to the list of advertised speeds and restart
	auto basic = PhyRegisterRead(m_activeInterface, PHY_REG_BASIC_CONTROL);
	if( (basic & 0x1000) == 0x1000)
	{
		//10/100 speeds are in the AN base page advertisement register
		if( (m_command[1].m_commandID == CMD_10) || (m_command[1].m_commandID == CMD_100) )
		{
			auto adv = PhyRegisterRead(m_activeInterface, PHY_REG_AN_ADVERT);
			if(m_command[2].m_commandID == CMD_100)
				adv |= 0x100;
			else
				adv |= 0x40;
			PhyRegisterWrite(m_activeInterface, PHY_REG_AN_ADVERT, adv);
		}

		//Gigabit speeds are in the 1000baseT control register
		else
		{
			auto mode = PhyRegisterRead(m_activeInterface, PHY_REG_GIG_CONTROL);
			mode |= 0x200;
			PhyRegisterWrite(m_activeInterface, PHY_REG_GIG_CONTROL, mode);
		}

		//Restart negotiation
		RestartNegotiation(m_activeInterface);
	}

	//Negotiation off, force to exactly this speed
	else
	{
		//Mask off speed select
		basic &= 0xdfbf;

		switch(m_command[1].m_commandID)
		{
			case CMD_10:
				//nothing to do, code 0 is 10M
				break;

			case CMD_100:
				basic |= 0x2000;
				break;

			case CMD_1000:
				basic |= 0x0040;
				break;
		}

		PhyRegisterWrite(m_activeInterface, PHY_REG_BASIC_CONTROL, basic);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "speed"

void TapCLISessionContext::OnTest()
{
	//Figure out what interface we're testing
	int iface = 0;
	switch(m_command[1].m_commandID)
	{
		case CMD_PORTA:
			iface = 0;
			break;

		case CMD_PORTB:
			iface = 1;
			break;

		case CMD_MONA:
			iface = 2;
			break;

		case CMD_MONB:
			iface = 3;
			break;

		default:
			break;
	}

	m_stream->Printf("Running TDR cable test on interface %s\n", g_portDescriptions[iface]);

	//Save the old values for a few registers and configure for testing
	//Need to have speed forced to 1000baseT, no negotiation, slave mode, no auto mdix
	//Reference: https://microchipsupport.force.com/s/article/How-to-test-the-4-differential-pairs-between-KSZ9031-Gigabit-Ethernet-PHY-and-RJ-45-connector-for-opens-and-shorts
	auto oldBase = PhyRegisterRead(iface, PHY_REG_BASIC_CONTROL);
	auto oldMdix = PhyRegisterRead(iface, PHY_REG_MDIX);
	auto oldCtrl = PhyRegisterRead(iface, PHY_REG_GIG_CONTROL);
	PhyRegisterWrite(iface, PHY_REG_BASIC_CONTROL, 0x0140);
	PhyRegisterWrite(iface, PHY_REG_MDIX, 0x0040);
	PhyRegisterWrite(iface, PHY_REG_GIG_CONTROL, 0x1000);

	m_stream->Printf("Uncertainty: +/- 4ns or 0.85m\n");
	m_stream->Printf("Local pair     Status      Length (ns)        Length (m)\n");
	for(int pair = 0; pair < 4; pair ++)
	{
		//Run ten tests and average.
		//Report fault if any of them are failures
		int faulttype = 0;
		int faultdist = 0;
		int navg = 10;
		for(int j=0; j<navg; j ++)
		{
			//Request a test of this pair
			PhyRegisterWrite(iface, PHY_REG_LINKMD, 0x8000 | (pair << 12) );

			//Poll until test completes. Time out if no reply after 50 ms
			uint16_t result = 0;
			for(int i=0; i<50; i++)
			{
				result = PhyRegisterRead(iface, PHY_REG_LINKMD);
				g_logTimer->Sleep(10);

				if( (result & 0x8000) == 0)
					break;
			}

			int faultcode = (result >> 8) & 3;
			int rawDistance = (result & 0xff) - 13;
			if(rawDistance < 0)
				rawDistance = 0;

			faultdist += rawDistance;

			if(faultcode != 0)
				faulttype = faultcode;
		}

		if(faulttype == 0)
			m_stream->Printf("%c              Normal              N/A               N/A\n", 'A' + pair);
		else
		{
			int oneWayNs = (faultdist * 4 / navg);
			int cableCm = 21 * oneWayNs;

			m_stream->Printf("%c              %-5s               %3d            %3d.%02d\n",
				'A' + pair,
				(faulttype == 1) ? "Open" : "Short",
				oneWayNs,
				cableCm / 100,
				cableCm % 100
				);
		}
	}

	//Restore original mode register values
	PhyRegisterWrite(iface, PHY_REG_BASIC_CONTROL, oldBase);
	PhyRegisterWrite(iface, PHY_REG_MDIX, oldMdix);
	PhyRegisterWrite(iface, PHY_REG_GIG_CONTROL, oldCtrl);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "autonegotiation"

void TapCLISessionContext::OnAutonegotiation()
{
	auto basic = PhyRegisterRead(m_activeInterface, PHY_REG_BASIC_CONTROL);
	PhyRegisterWrite(m_activeInterface, PHY_REG_BASIC_CONTROL, basic | 0x1000);

	RestartNegotiation(m_activeInterface);
}

void TapCLISessionContext::OnNoAutonegotiation()
{
	auto basic = PhyRegisterRead(m_activeInterface, PHY_REG_BASIC_CONTROL);
	auto gig = PhyRegisterRead(m_activeInterface, PHY_REG_GIG_CONTROL);
	auto adv = PhyRegisterRead(m_activeInterface, PHY_REG_AN_ADVERT);

	//Turn off AN flag
	basic &= ~0x1000;

	//Force speed to highest currently advertised speed
	basic &= 0xdfbf;
	if(gig & 0x200)
		basic |= 0x0040;
	else if(adv & 0x100)
		basic |= 0x2000;

	PhyRegisterWrite(m_activeInterface, PHY_REG_BASIC_CONTROL, basic);

	RestartNegotiation(m_activeInterface);
}
