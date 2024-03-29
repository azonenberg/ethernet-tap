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

//UART console
UART* g_cliUART = nullptr;
Logger g_log;
UARTOutputStream g_uartStream;
TapCLISessionContext g_uartCliContext;
Timer* g_logTimer = nullptr;

GPIOPin* g_irq = nullptr;

//QSPI interface to FPGA
OctoSPI* g_qspi;

void InitClocks();
void InitUART();
void InitLog();
void DetectHardware();
void InitQSPI();
void InitFPGA();
void InitPHYs();
void InitCLI();

void UpdateSpeedLEDs();
void CheckButtons();

void OnFPGAInterrupt();

uint16_t g_linkState = 0;

const char* g_portDescriptions[4] =
{
	"porta",
	"portb",
	"mona",
	"monb"
};

const char* g_portLongDescriptions[4] =
{
	"Left tap port",
	"Right tap port",
	"Monitor of port A",
	"Monitor of port B"
};

const int g_linkSpeeds[4] =
{
	10,
	100,
	1000,
	0		//reserved / invalid
};

int main()
{
	//Initialize power (must be the very first thing done after reset)
	Power::ConfigureSMPSToLDOCascade(Power::VOLTAGE_1V8, RANGE_VOS0);

	//Copy .data from flash to SRAM (for some reason the default newlib startup won't do this??)
	memcpy(&__data_start, &__data_romstart, &__data_end - &__data_start + 1);

	//Enable SYSCFG before changing any settings on it
	RCCHelper::EnableSyscfg();

	//Hardware setup
	InitClocks();
	InitUART();
	InitLog();
	DetectHardware();
	InitQSPI();
	InitFPGA();
	InitPHYs();
	InitCLI();

	//Set up the GPIO LEDs and turn them all off for now
	GPIOPin led0(&GPIOD, 3, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	GPIOPin led1(&GPIOD, 2, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	GPIOPin led2(&GPIOD, 1, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	GPIOPin led3(&GPIOC, 12, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	led0 = 0;
	led1 = 0;
	led2 = 0;
	led3 = 0;

	//Set up the QSPI IRQ GPIO pin
	//(not actually used as an interrupt for now, just polling)
	GPIOPin irq(&GPIOE, 12, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);
	g_irq = &irq;

	//Enable interrupts only after all setup work is done
	EnableInterrupts();

	//Show the initial prompt
	g_uartCliContext.PrintPrompt();

	//Main event loop
	while(1)
	{
		//Wait for an interrupt
		//asm("wfi");

		//Poll for UART input
		if(g_cliUART->HasInput())
			g_uartCliContext.OnKeystroke(g_cliUART->BlockingRead());

		PollIO();
	}

	return 0;
}

/**
	Process I/O other than the UART
 */
void PollIO()
{
	//Poll for interrupts from the FPGA
	if(*g_irq)
		OnFPGAInterrupt();

	//Run LEDs and buttons
	UpdateSpeedLEDs();
	CheckButtons();
}

void InitClocks()
{
	//Configure the flash with wait states and prefetching before making any changes to the clock setup.
	//A bit of extra latency is fine, the CPU being faster than flash is not.
	Flash::SetConfiguration(64, RANGE_VOS0);

	//Start the high speed internal clock
	RCCHelper::EnableHighSpeedInternalClock(64);

	//Don't use the PLL, it's overkill for a simple CLI.
	//HSI is already selected as default out of reset, so no need to change clock source. Just poke dividers.

	//Set up main system clock tree
	//Super simple, everything runs on the same 64 MHz clock!
	RCCHelper::InitializeSystemClocks(
		1,		//sysclk = 64 MHz
		1,		//AHB
		1,		//APB1
		1,		//APB2
		1,		//APB3
		1		//APB4
	);
}

void InitUART()
{
	//Initialize the UART for local console: 115.2 Kbps using PA12 for UART4 transmit and PA11 for UART2 receive
	//TODO: nice interface for enabling UART interrupts
	GPIOPin uart_tx(&GPIOA, 12, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_SLOW, 6);
	GPIOPin uart_rx(&GPIOA, 11, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_SLOW, 6);

	//Default after reset is for UART4 to be clocked by PCLK1 (APB1 clock) which is 64 MHz
	//So we need a divisor of 556
	static UART uart(&UART4, 556);
	g_cliUART = &uart;

	//Enable the UART RX interrupt
	//TODO: Make an RCC method for this
	volatile uint32_t* NVIC_ISER1 = (volatile uint32_t*)(0xe000e104);
	*NVIC_ISER1 = 0x100000;

	//Clear screen and move cursor to X0Y0
	uart.Printf("\x1b[2J\x1b[0;0H");
}

void InitLog()
{
	//APB1 is 64 MHz
	//Divide down to get 10 kHz ticks
	static Timer logtim(&TIM2, Timer::FEATURE_GENERAL_PURPOSE, 6400);
	g_logTimer = &logtim;

	g_log.Initialize(g_cliUART, &logtim);
	g_log("UART logging ready\n");
	g_log("Firmware compiled at %s on %s\n", __TIME__, __DATE__);
}

void DetectHardware()
{
	g_log("Identifying hardware\n");
	LogIndenter li(g_log);

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

		g_log("STM32%c%c%c%c stepping %s, %s\n",
			(L_ID >> 24) & 0xff,
			(L_ID >> 16) & 0xff,
			(L_ID >> 8) & 0xff,
			(L_ID >> 0) & 0xff,
			srev,
			package
			);
		g_log("564 kB total SRAM, 128 kB DTCM, up to 256 kB ITCM, 4 kB backup SRAM\n");
		g_log("%d kB Flash\n", F_ID);

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
		g_log("Lot %s, wafer %d, die (%d, %d)\n", waferLot, waferNum, waferX, waferY);
	}
	else
		g_log(Logger::WARNING, "Unknown device (0x%06x)\n", device);
}

void InitCLI()
{
	g_log("Initializing CLI\n");

	//Initialize the CLI on the console UART interface
	g_uartStream.Initialize(g_cliUART);
	g_uartCliContext.Initialize(&g_uartStream);
}

void InitQSPI()
{
	g_log("Initializing QSPI interface\n");

	//Configure the I/O manager
	OctoSPIManager::ConfigureMux(false);
	OctoSPIManager::ConfigurePort(
		1,							//Configuring port 1
		false,						//DQ[7:4] disabled
		OctoSPIManager::C1_HIGH,
		true,						//DQ[3:0] enabled
		OctoSPIManager::C1_LOW,		//DQ[3:0] from OCTOSPI1 DQ[3:0]
		true,						//CS# enabled
		OctoSPIManager::PORT_1,		//CS# from OCTOSPI1
		false,						//DQS disabled
		OctoSPIManager::PORT_1,
		true,						//Clock enabled
		OctoSPIManager::PORT_1);	//Clock from OCTOSPI1

	//Configure the I/O pins
	static GPIOPin qspi_cs_n(&GPIOE, 11, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_VERYFAST, 11);
	static GPIOPin qspi_sck(&GPIOB, 2, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_VERYFAST, 9);
	static GPIOPin qspi_dq0(&GPIOA, 2, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_VERYFAST, 6);
	static GPIOPin qspi_dq1(&GPIOB, 0, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_VERYFAST, 4);
	static GPIOPin qspi_dq2(&GPIOC, 2, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_VERYFAST, 9);
	static GPIOPin qspi_dq3(&GPIOA, 1, GPIOPin::MODE_PERIPHERAL, GPIOPin::SLEW_VERYFAST, 9);

	//Clock divider value
	//Default is for AHB3 bus clock to be used as kernel clock (64 MHz for us)
	//With 3.3V Vdd, we can go up to 140 MHz.
	//Dividing by 2 gives 32 MHz and a transfer rate of 128 Mbps. Plenty for PHy configuration
	uint8_t prescale = 2;

	//Configure the OCTOSPI itself
	static OctoSPI qspi(&OCTOSPI1, 0x02000000, prescale);
	qspi.SetDoubleRateMode(false);
	qspi.SetInstructionMode(OctoSPI::MODE_QUAD, 2);
	qspi.SetAddressMode(OctoSPI::MODE_NONE);
	qspi.SetAltBytesMode(OctoSPI::MODE_NONE);
	qspi.SetDataMode(OctoSPI::MODE_QUAD);
	qspi.SetDummyCycleCount(1);
	qspi.SetDQSEnable(false);
	qspi.SetDeselectTime(1);
	qspi.SetSampleDelay(true);

	g_qspi = &qspi;
}

void InitFPGA()
{
	g_log("Initializing FPGA\n");
	LogIndenter li(g_log);

	//Wait 500ms to make sure the FPGA is booted
	g_log("Waiting for boot\n");
	g_logTimer->Sleep(5000);

	//Read the FPGA IDCODE and serial number
	uint8_t buf[8];
	g_qspi->BlockingRead(REG_FPGA_IDCODE, 0, buf, 4);
	uint32_t idcode = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	g_qspi->BlockingRead(REG_FPGA_SERIAL, 0, buf, 8);

	//Print status
	switch(idcode & 0x0fffffff)
	{
		case 0x3620093:
			g_log("IDCODE: %08x (XC7S15 rev %d)\n", idcode, idcode >> 28);
			break;

		default:
			g_log("IDCODE: %08x (unknown device, rev %d)\n", idcode, idcode >> 28);
			break;
	}
	g_log("Serial: %02x%02x%02x%02x%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
}

void InitPHYs()
{
	for(int port = 0; port < 4; port ++)
	{
		g_log("Initializing Ethernet port %d (%s)\n", port, g_portDescriptions[port]);
		LogIndenter li(g_log);

		auto base = (port * REG_ETH_OFFSET);

		//KSZ9031 datasheet does not mention a minimum reset pulse width
		//Do 1 ms to be safe. Timer is 10 kHz so that's 10 ticks.
		//We then need a min of 100us before poking any registers; do 1ms
		g_qspi->BlockingWrite8(base + REG_ETH0_RST, 0, 0);
		g_logTimer->Sleep(10);
		g_qspi->BlockingWrite8(base + REG_ETH0_RST, 0, 1);
		g_logTimer->Sleep(10);

		//Identify the PHY and make sure it's what we expect
		auto id1 = PhyRegisterRead(port, PHY_REG_ID1);
		auto id2 = PhyRegisterRead(port, PHY_REG_ID2);
		if( (id1 != 0x0022) || ( (id2 >> 4) != 0x162) )
			g_log("Unexpected PHY identifier (ID1=%04x, ID2=%04x)\n", id1, id2);
		else
			g_log("Detected KSZ9031RNX rev %d\n", id2 & 0xf);

		//Select 16ms AN FLP interval (default is 8 but this doesn't work with some PHYs)
		g_log("Selecting 16ms AN burst interval\n");
		PhyRegisterIndirectWrite(port, 0, PHY_REG_MMD0_FLP_LO, 0x1a80);
		PhyRegisterIndirectWrite(port, 0, PHY_REG_MMD0_FLP_HI, 0x0006);

		//Change AN advertisement to exclude half duplex modes
		g_log("Advertising all speeds in full duplex only\n");
		PhyRegisterWrite(port, PHY_REG_AN_ADVERT, 0x141);
	}
}

/**
	@brief Reads a single PHY register
 */
uint16_t PhyRegisterRead(int port, uint8_t regid)
{
	auto base = (port * REG_ETH_OFFSET);
	g_qspi->BlockingWrite8(base + REG_ETH0_MDIO_RADDR, 0, regid);

	//Wait for read to complete. Deterministic run time, no need for polling
	//MDIO clock runs at about 2 MHz.
	//An entire transaction is 32 + 2 + 2 + 5 + 5 + 1 + 16 + 16 = 79 UIs including IFG (so 39 us)
	//Two ticks (nominal 200us) is plenty even if we don't reset the timer (so worst case 100us).
	g_logTimer->Sleep(2);

	return g_qspi->BlockingRead16(base + REG_ETH0_MDIO_RDATA, 0);
}

/**
	@brief Writes a single PHY register
 */
void PhyRegisterWrite(int port, uint8_t regid, uint16_t regval)
{
	uint8_t msg[3] =
	{
		regid,
		static_cast<uint8_t>(regval & 0xff),
		static_cast<uint8_t>(regval >> 8)
	};

	auto base = (port * REG_ETH_OFFSET);
	g_qspi->BlockingWrite(base + REG_ETH0_MDIO_WR, 0, msg, sizeof(msg));

	//Wait for write to complete
	g_logTimer->Sleep(2);
}

/**
	@brief Reads an indirect PHY register
 */
uint16_t PhyRegisterIndirectRead(int port, uint8_t mmd, uint16_t regid)
{
	PhyRegisterWrite(port, PHY_REG_MMD_CTRL, mmd);
	PhyRegisterWrite(port, PHY_REG_MMD_DATA, regid);
	PhyRegisterWrite(port, PHY_REG_MMD_CTRL, mmd | 0x4000);
	return PhyRegisterRead(port, PHY_REG_MMD_DATA);
}

/**
	@brief Writes an indirect PHY register
 */
void PhyRegisterIndirectWrite(int port, uint8_t mmd, uint16_t regid, uint16_t regval)
{
	PhyRegisterWrite(port, PHY_REG_MMD_CTRL, mmd);
	PhyRegisterWrite(port, PHY_REG_MMD_DATA, regid);
	PhyRegisterWrite(port, PHY_REG_MMD_CTRL, mmd | 0x4000);
	PhyRegisterWrite(port, PHY_REG_MMD_DATA, regval);
}

void OnFPGAInterrupt()
{
	uint16_t status = g_qspi->BlockingRead16(REG_LINK_STATE, 0);
	uint16_t delta = status ^ g_linkState;

	//Look for changes to each interface and report status
	for(int nport = 0; nport < 4; nport ++)
	{
		//Skip any ports with no change
		int shift = (nport * 4);
		if( ((delta >> shift) & 0xf) == 0)
			continue;

		//If speed changed while link is down, ignore that
		int state = (status >> shift) & 0xf;
		auto up = state & 0x8;
		if(!up && ( ( (delta >> shift) & 0x8) == 0) )
			continue;

		//Report link state change
		auto speed = state & 3;
		if(up)
			g_log("Interface %s: link up, %d Mbps\n", g_portDescriptions[nport], g_linkSpeeds[speed]);
		else
			g_log("Interface %s: link down\n", g_portDescriptions[nport]);
	}

	g_linkState = status;
}

void UpdateSpeedLEDs()
{
	//LEDs
	static GPIOPin left_aneg_en(&GPIOC, 4, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin left_10m_en(&GPIOB, 11, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin left_100m_en(&GPIOF, 3, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin left_1000m_en(&GPIOB, 9, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);

	static GPIOPin right_aneg_en(&GPIOA, 5, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin right_10m_en(&GPIOE, 10, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin right_100m_en(&GPIOG, 15, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin right_1000m_en(&GPIOB, 8, GPIOPin::MODE_OUTPUT, GPIOPin::SLEW_SLOW);

	//Get advertised speeds/modes for each port
	auto leftAd = PhyRegisterRead(0, PHY_REG_AN_ADVERT);
	auto leftGig = PhyRegisterRead(0, PHY_REG_GIG_CONTROL);
	bool left10 = (leftAd & 0x40) == 0x40;
	bool left100 = (leftAd & 0x100) == 0x100;
	bool left1000 = (leftGig & 0x200) == 0x200;

	auto rightAd = PhyRegisterRead(1, PHY_REG_AN_ADVERT);
	auto rightGig = PhyRegisterRead(1, PHY_REG_GIG_CONTROL);
	bool right10 = (rightAd & 0x40) == 0x40;
	bool right100 = (rightAd & 0x100) == 0x100;
	bool right1000 = (rightGig & 0x200) == 0x200;

	//Get actual operating speeds
	auto leftCtrl = PhyRegisterRead(0, PHY_REG_CTRL);
	auto rightCtrl = PhyRegisterRead(1, PHY_REG_CTRL);
	int leftSpeed = 0;
	int rightSpeed = 0;
	if(leftCtrl & 0x10)
		leftSpeed = 10;
	if(leftCtrl & 0x20)
		leftSpeed = 100;
	if(leftCtrl & 0x40)
		leftSpeed = 1000;
	if(rightCtrl & 0x10)
		rightSpeed = 10;
	if(rightCtrl & 0x20)
		rightSpeed = 100;
	if(rightCtrl & 0x40)
		rightSpeed = 1000;

	//Get AN enable status
	auto leftBasic = PhyRegisterRead(0, PHY_REG_BASIC_CONTROL);
	auto rightBasic = PhyRegisterRead(1, PHY_REG_BASIC_CONTROL);
	bool leftAneg = (leftBasic & 0x1000) == 0x1000;
	bool rightAneg = (rightBasic & 0x1000) == 0x1000;

	//Blink at ~1 Hz (500ms per cycle)
	bool blink = (g_logTimer->GetCount() / 5000) % 2;

	//LEDs on if advertised/forced off if not
	//Blink if currently running at that speed
	if( (leftSpeed == 10) && blink)
		left10 = false;
	if( (leftSpeed == 100) && blink)
		left100 = false;
	if( (leftSpeed == 1000) && blink)
		left1000 = false;
	if( (rightSpeed == 10) && blink)
		right10 = false;
	if( (rightSpeed == 100) && blink)
		right100 = false;
	if( (rightSpeed == 1000) && blink)
		right1000 = false;

	//Write final outputs
	left_10m_en = left10;
	left_100m_en = left100;
	left_1000m_en = left1000;
	left_aneg_en = leftAneg;

	right_10m_en = right10;
	right_100m_en = right100;
	right_1000m_en = right1000;
	right_aneg_en = rightAneg;
}

void CheckButtons()
{
	static bool changedLastTime = false;

	static GPIOPin left_aneg_sw(&GPIOF, 14, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin left_10m_sw(&GPIOH, 2, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin left_100m_sw(&GPIOC, 5, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin left_1000m_sw(&GPIOE, 3, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);

	static GPIOPin right_aneg_sw(&GPIOF, 12, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin right_10m_sw(&GPIOH, 3, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin right_100m_sw(&GPIOF, 0, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);
	static GPIOPin right_1000m_sw(&GPIOE, 4, GPIOPin::MODE_INPUT, GPIOPin::SLEW_SLOW);

	//Read switches
	bool left_aneg = left_aneg_sw;
	bool left_10m = left_10m_sw;
	bool left_100m = left_100m_sw;
	bool left_1000m = left_1000m_sw;
	bool right_aneg = right_aneg_sw;
	bool right_10m = right_10m_sw;
	bool right_100m = right_100m_sw;
	bool right_1000m = right_1000m_sw;

	//See which port (if any) was hit
	int nport = 0;
	bool changed = false;
	if(left_aneg || left_10m || left_100m || left_1000m)
		changed = true;
	if(right_aneg || right_10m || right_100m || right_1000m)
	{
		changed = true;
		nport = 1;
	}

	//Ignore any button presses if we just executed one
	if(changedLastTime && changed)
		return;

	changedLastTime = changed;

	//Process the changes
	if(changed)
	{
		auto basic = PhyRegisterRead(nport, PHY_REG_BASIC_CONTROL);
		auto an_on = (basic & 0x1000) == 0x1000;
		auto gig = PhyRegisterRead(nport, PHY_REG_GIG_CONTROL);
		auto adv = PhyRegisterRead(nport, PHY_REG_AN_ADVERT);

		//Toggle AN flag
		if(left_aneg || right_aneg)
		{
			basic ^= 0x1000;
			an_on = !an_on;

			//If turning AN off, force to highest currently advertised speed
			if(!an_on)
			{
				basic &= 0xdfbf;
				if(gig & 0x200)
					basic |= 0x0040;
				else if(adv & 0x100)
					basic |= 0x2000;
			}

			PhyRegisterWrite(nport, PHY_REG_BASIC_CONTROL, basic);
		}

		//If AN is on, toggle advertised speeds
		if(an_on)
		{
			if(left_10m || right_10m || left_100m || right_100m )
			{
				if(left_10m || right_10m)
					adv ^= 0x40;
				if(left_100m || right_100m)
					adv ^= 0x100;

				PhyRegisterWrite(nport, PHY_REG_AN_ADVERT, adv);
			}

			if(left_1000m || right_1000m)
				PhyRegisterWrite(nport, PHY_REG_GIG_CONTROL, gig ^ 0x200);

			RestartNegotiation(nport);
		}

		//If AN is off, force the clicked speed
		else if(left_10m || right_10m || left_100m || right_100m || left_1000m || right_1000m)
		{
			//Mask off speed select
			basic &= 0xdfbf;

			if(left_10m || right_10m)
			{
				//nothing to do, code 0 is 10M
			}

			if(left_100m || right_100m)
				basic |= 0x2000;
			if(left_1000m || right_1000m)
				basic |= 0x0040;

			PhyRegisterWrite(nport, PHY_REG_BASIC_CONTROL, basic);
		}

		//1ms debounce timer
		g_logTimer->Sleep(10);
	}
}

void RestartNegotiation(int nport)
{
	auto base = PhyRegisterRead(nport, PHY_REG_BASIC_CONTROL);
	PhyRegisterWrite(nport, PHY_REG_BASIC_CONTROL, base | 0x0200);
}
