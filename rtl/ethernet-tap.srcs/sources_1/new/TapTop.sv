`timescale 1ns / 1ps
`default_nettype none
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
module TapTop(

	//Passthrough port RGMII buses
	//eth0/eth1
	input wire			portA_rx_clk,
	input wire[3:0]		portA_rx_data,
	input wire			portA_rx_en,

	output wire			portA_tx_clk,
	output wire[3:0]	portA_tx_data,
	output wire			portA_tx_en,

	input wire			portB_rx_clk,
	input wire[3:0]		portB_rx_data,
	input wire			portB_rx_en,

	output wire			portB_tx_clk,
	output wire[3:0]	portB_tx_data,
	output wire			portB_tx_en,

	//Monitor port RGMII buses
	//eth2/eth3
	//Need to hook up TX and RX because we may need to have strap pullups/downs on the RX bus
	//(despite never actually listening to traffic on these interfaces)
	input wire			monA_rx_clk,
	input wire[3:0]		monA_rx_data,
	input wire			monA_rx_en,

	output wire			monA_tx_clk,
	output wire[3:0]	monA_tx_data,
	output wire			monA_tx_en,

	input wire			monB_rx_clk,
	input wire[3:0]		monB_rx_data,
	input wire			monB_rx_en,

	output wire			monB_tx_clk,
	output wire[3:0]	monB_tx_data,
	output wire			monB_tx_en,

	//PHY reset signals
	output wire			portA_rst_n,
	output wire			portB_rst_n,
	output wire			monA_rst_n,
	output wire			monB_rst_n,

	//MDIO buses
	inout wire[3:0]		mdio,
	output wire[3:0]	mdc,

	//Interface to MCU
	input wire			qspi_sck,
	input wire			qspi_cs_n,
	inout wire[3:0]		qspi_dq,
	output wire			mcu_irq,

	//Top level FPGA clock input
	input wire			clk_25mhz,

	//GPIO LEDs for whatever
	output logic[3:0]	gpio_led = 0
);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Clock generation

	wire	clk_system;
	wire	clk_125mhz;
	wire	clk_250mhz;
	wire	clk_200mhz;
	wire	clk_50mhz;

	wire	pll_locked;

	ClockGeneration clkgen(
		.clk_25mhz(clk_25mhz),

		.clk_system(clk_system),
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),
		.clk_200mhz(clk_200mhz),
		.clk_50mhz(clk_50mhz),
		.pll_locked(pll_locked)
		);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// I/O delay calibration

	IODelayCalibration cal(.refclk(clk_200mhz));

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Interface to MCU for management

	cfgregs_t cfgregs;
	wire[15:0]	mdio_rd_data[3:0];

	wire[3:0]		link_up_sync;
	lspeed_t[3:0]	link_speed_sync;
	wire[3:0]		link_updated_sync;

	MicrocontrollerInterface mgmt(
		.clk_50mhz(clk_50mhz),
		.clk_125mhz(clk_125mhz),
		.qspi_sck(qspi_sck),
		.qspi_cs_n(qspi_cs_n),
		.qspi_dq(qspi_dq),
		.irq(mcu_irq),

		.cfgregs(cfgregs),
		.mdio_eth0_rd_data(mdio_rd_data[0]),
		.mdio_eth1_rd_data(mdio_rd_data[1]),
		.mdio_eth2_rd_data(mdio_rd_data[2]),
		.mdio_eth3_rd_data(mdio_rd_data[3]),
		.link_up(link_up_sync),
		.link_speed(link_speed_sync),
		.link_updated(link_updated_sync)
	);

	//Hook up PHY resets (under software control)
	assign portA_rst_n = cfgregs.phy_rst_n[0];
	assign portB_rst_n = cfgregs.phy_rst_n[1];
	assign monA_rst_n = cfgregs.phy_rst_n[2];
	assign monB_rst_n = cfgregs.phy_rst_n[3];

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Debug LED blinkies

	logic[24:0] count = 0;
	always_ff @(posedge clk_125mhz) begin
		count <= count + 1;
		if(count == 0)
			gpio_led <= gpio_led + 1;
	end

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// MACs

	`include "GmiiBus.svh"
	`include "EthernetBus.svh"

	wire			link_up[3:0];
	wire[1:0]		link_speed[3:0];

	wire			mac_rx_clk[3:0];
	EthernetRxBus	portA_mac_rx_bus;

	EthernetTxBus	portA_mac_tx_bus = 0;
	wire			portA_mac_tx_ready;

	RGMIIMACWrapper mac_portA(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(portA_rx_clk),
		.rgmii_rxd(portA_rx_data),
		.rgmii_rx_ctl(portA_rx_en),
		.rgmii_txc(portA_tx_clk),
		.rgmii_txd(portA_tx_data),
		.rgmii_tx_ctl(portA_tx_en),

		.mac_rx_clk(mac_rx_clk[0]),
		.mac_rx_bus(portA_mac_rx_bus),
		.mac_tx_bus(portA_mac_tx_bus),
		.mac_tx_ready(portA_mac_tx_ready),

		.link_up(link_up[0]),
		.link_speed(link_speed[0])
	);

	wire			portB_mac_rx_clk;
	EthernetRxBus	portB_mac_rx_bus;

	EthernetTxBus	portB_mac_tx_bus = 0;
	wire			portB_mac_tx_ready;

	RGMIIMACWrapper mac_portB(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(portB_rx_clk),
		.rgmii_rxd(portB_rx_data),
		.rgmii_rx_ctl(portB_rx_en),
		.rgmii_txc(portB_tx_clk),
		.rgmii_txd(portB_tx_data),
		.rgmii_tx_ctl(portB_tx_en),

		.mac_rx_clk(mac_rx_clk[1]),
		.mac_rx_bus(portB_mac_rx_bus),
		.mac_tx_bus(portB_mac_tx_bus),
		.mac_tx_ready(portB_mac_tx_ready),

		.link_up(link_up[1]),
		.link_speed(link_speed[1])
	);

	wire			monA_mac_rx_clk;
	EthernetRxBus	monA_mac_rx_bus;

	EthernetTxBus	monA_mac_tx_bus = 0;
	wire			monA_mac_tx_ready;

	RGMIIMACWrapper mac_monA(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(monA_rx_clk),
		.rgmii_rxd(monA_rx_data),
		.rgmii_rx_ctl(monA_rx_en),
		.rgmii_txc(monA_tx_clk),
		.rgmii_txd(monA_tx_data),
		.rgmii_tx_ctl(monA_tx_en),

		.mac_rx_clk(mac_rx_clk[2]),
		.mac_rx_bus(monA_mac_rx_bus),
		.mac_tx_bus(monA_mac_tx_bus),
		.mac_tx_ready(monA_mac_tx_ready),

		.link_up(link_up[2]),
		.link_speed(link_speed[2])
	);

	wire			monB_mac_rx_clk;
	EthernetRxBus	monB_mac_rx_bus;

	EthernetTxBus	monB_mac_tx_bus = 0;
	wire			monB_mac_tx_ready;

	RGMIIMACWrapper mac_monB(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(monB_rx_clk),
		.rgmii_rxd(monB_rx_data),
		.rgmii_rx_ctl(monB_rx_en),
		.rgmii_txc(monB_tx_clk),
		.rgmii_txd(monB_tx_data),
		.rgmii_tx_ctl(monB_tx_en),

		.mac_rx_clk(mac_rx_clk[3]),
		.mac_rx_bus(monB_mac_rx_bus),
		.mac_tx_bus(monB_mac_tx_bus),
		.mac_tx_ready(monB_mac_tx_ready),

		.link_up(link_up[3]),
		.link_speed(link_speed[3])
	);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Clock domain crossing for link state flags

	for(genvar i=0; i<4; i=i+1) begin : lcdc
		LinkStateSynchronizer lss(
			.clk_a(mac_rx_clk[i]),
			.link_up_a(link_up[i]),
			.link_speed_a(link_speed[i]),

			.clk_b(clk_125mhz),
			.link_up_b(link_up_sync[i]),
			.link_speed_b(link_speed_sync[i]),
			.updated_b(link_updated_sync[i])
		);
	end

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// MDIO interfaces

	for(genvar i=0; i<4; i=i+1) begin : mtxvrs
		wire	mdio_tx_en;
		wire	mdio_tx_data;
		wire	mdio_rx_data;
		wire	busy;

		BidirectionalBuffer iobuf(
			.fabric_in(mdio_rx_data),
			.fabric_out(mdio_tx_data),
			.pad(mdio[i]),
			.oe(mdio_tx_en)
		);

		EthernetMDIOTransceiver txvr(
			.clk_125mhz(clk_125mhz),
			.phy_md_addr(5'b0),
			.mdio_tx_data(mdio_tx_data),
			.mdio_tx_en(mdio_tx_en),
			.mdio_rx_data(mdio_rx_data),
			.mdc(mdc[i]),

			.mgmt_busy_fwd(busy),
			.phy_reg_addr(cfgregs.mdio_regaddr),
			.phy_wr_data(cfgregs.mdio_wdata),
			.phy_rd_data(mdio_rd_data[i]),
			.phy_reg_wr(cfgregs.mdio_wr_en[i]),
			.phy_reg_rd(cfgregs.mdio_rd_en[i])
		);
	end

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// MCU interface

endmodule
