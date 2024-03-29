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

`include "EthernetBus.svh"

module PacketDatapath(

	//Core clock, also used for transmit side
	input wire					clk_125mhz,
	input wire					rst,

	//Inputs from each of the thru path ports
	input wire					portA_rx_clk,
	input wire EthernetRxBus	portA_mac_rx_bus,
	input wire					portB_rx_clk,
	input wire EthernetRxBus	portB_mac_rx_bus,

	//Outputs for the thru path ports
	input wire					portA_tx_ready,
	output EthernetTxBus		portA_tx_bus,
	input wire					portB_tx_ready,
	output EthernetTxBus		portB_tx_bus,

	//Outputs for the monitor path ports
	input wire					monA_tx_ready,
	output EthernetTxBus		monA_tx_bus,
	input wire					monB_tx_ready,
	output EthernetTxBus		monB_tx_bus
	);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Resets

	//Polarity inversion b/c synchronizer expects active low resets
	wire	rst_portA_rx_n;
	wire	rst_portB_rx_n;

	wire	rst_portA_rx;
	wire	rst_portB_rx;
	assign	rst_portA_rx = !rst_portA_rx_n;
	assign	rst_portB_rx = !rst_portB_rx_n;

	ResetSynchronizer sync_rst_a(
		.rst_in_n(!rst),
		.clk(portA_rx_clk),
		.rst_out_n(rst_portA_rx_n));

	ResetSynchronizer sync_rst_b(
		.rst_in_n(!rst),
		.clk(portB_rx_clk),
		.rst_out_n(rst_portB_rx_n));

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Forwarding path

	EthernetCrossoverClockCrossing_x8 a_to_b(
		.rx_clk(portA_rx_clk),
		.rx_bus(portA_mac_rx_bus),
		.rx_rst(rst_portA_rx),

		.tx_clk(clk_125mhz),
		.tx_rst(rst),
		.tx_ready(portB_tx_ready),
		.tx_bus(portB_tx_bus)
		);

	EthernetCrossoverClockCrossing_x8 b_to_a(
		.rx_clk(portB_rx_clk),
		.rx_bus(portB_mac_rx_bus),
		.rx_rst(rst_portB_rx),

		.tx_clk(clk_125mhz),
		.tx_rst(rst),
		.tx_ready(portA_tx_ready),
		.tx_bus(portA_tx_bus)
		);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Monitor path

	EthernetCrossoverClockCrossing_x8 a_to_mon(
		.rx_clk(portA_rx_clk),
		.rx_bus(portA_mac_rx_bus),
		.rx_rst(rst_portA_rx),

		.tx_clk(clk_125mhz),
		.tx_rst(rst),
		.tx_ready(monA_tx_ready),
		.tx_bus(monA_tx_bus)
		);

	EthernetCrossoverClockCrossing_x8 b_to_mon(
		.rx_clk(portB_rx_clk),
		.rx_bus(portB_mac_rx_bus),
		.rx_rst(rst_portB_rx),

		.tx_clk(clk_125mhz),
		.tx_rst(rst),
		.tx_ready(monB_tx_ready),
		.tx_bus(monB_tx_bus)
		);

endmodule
