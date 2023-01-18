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

`include "MicrocontrollerInterface.svh"

module MicrocontrollerInterface(

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// System clocks

	input wire			clk_50mhz,
	input wire			clk_125mhz,

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Bus to MCU

	input wire			qspi_sck,
	input wire			qspi_cs_n,
	inout wire[3:0]		qspi_dq,
	output logic		irq = 0,

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Interface to internal FPGA blocks

	output cfgregs_t	cfgregs	= 0,

	output logic[3:0]	mdio_rd_en		= 0,
	output logic[4:0]	mdio_regaddr	= 0,
	input wire[15:0]	mdio_eth0_rd_data,
	input wire[15:0]	mdio_eth1_rd_data,
	input wire[15:0]	mdio_eth2_rd_data,
	input wire[15:0]	mdio_eth3_rd_data
);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// QSPI interface

	wire		start;
	wire		insn_valid;
	wire[15:0]	insn;
	wire		wr_valid;
	wire[7:0]	wr_data;
	logic		rd_mode		= 0;
	wire		rd_ready;
	logic		rd_valid	= 0;
	logic[7:0]	rd_data;

	QSPIDeviceInterface #(
		.INSN_BYTES(2)
	) qspi (
		.clk(clk_125mhz),
		.sck(qspi_sck),
		.cs_n(qspi_cs_n),
		.dq(qspi_dq),

		.start(start),
		.insn_valid(insn_valid),
		.insn(insn),
		.wr_valid(wr_valid),
		.wr_data(wr_data),
		.rd_mode(rd_mode),
		.rd_ready(rd_ready),
		.rd_valid(rd_valid),
		.rd_data(rd_data)
	);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Device info block

	wire[63:0] die_serial;
	wire[31:0] idcode;

	DeviceInfo_7series devinfo(
		.clk(clk_50mhz),
		.die_serial(die_serial),
		.die_serial_valid(),
		.idcode(idcode),
		.idcode_valid()
	);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// List of registers

	typedef enum logic[15:0]
	{
		REG_FPGA_IDCODE 	= 16'h0000,	//R: 4 byte JTAG IDCODE
		REG_FPGA_SERIAL 	= 16'h0001,	//R: 8 byte die serial number

		REG_ETH0_RST		= 16'h1000,	//W: [0] active low reset flag
		REG_ETH0_MDIO_RADDR	= 16'h1001,	//W: [4:0] read register access. Operation is dispatched on completion of write
		REG_ETH0_MDIO_RDATA	= 16'h1002, //R: 16 byte little endian read data

		REG_ETH1_RST		= 16'h2000,	//descriptions as in eth0
		REG_ETH1_MDIO_RADDR	= 16'h2001,
		REG_ETH1_MDIO_RDATA	= 16'h2002,

		REG_ETH2_RST		= 16'h3000,	//descriptions as in eth0
		REG_ETH2_MDIO_RADDR	= 16'h3001,
		REG_ETH2_MDIO_RDATA	= 16'h3002,

		REG_ETH3_RST		= 16'h4000,	//descriptions as in eth0
		REG_ETH3_MDIO_RADDR	= 16'h4001,
		REG_ETH3_MDIO_RDATA	= 16'h4002

	} opcode_t;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Main QSPI state machine

	logic[15:0] count = 0;

	always_ff @(posedge clk_125mhz) begin

		rd_valid					<= 0;
		cfgregs.mdio_rd_en			<= 0;
		cfgregs.mdio_wr_en			<= 0;

		//Output read data
		if(rd_ready) begin
			count		<= count + 1;
			rd_valid	<= 1;

			case(insn)

				REG_FPGA_IDCODE:		rd_data <= idcode[(3 - count[1:0])*8 +: 8];
				REG_FPGA_SERIAL:		rd_data <= die_serial[(7 - count[2:0])*8 +: 8];

				REG_ETH0_MDIO_RDATA:	rd_data <= mdio_eth0_rd_data[count[0]*8 +: 8];
				REG_ETH1_MDIO_RDATA:	rd_data <= mdio_eth1_rd_data[count[0]*8 +: 8];
				REG_ETH2_MDIO_RDATA:	rd_data <= mdio_eth2_rd_data[count[0]*8 +: 8];
				REG_ETH3_MDIO_RDATA:	rd_data <= mdio_eth3_rd_data[count[0]*8 +: 8];

				//default to no output
				default:
					rd_data	<= 8'h0;

			endcase

		end

		//Tristate inputs by default, except for instructions that read values from us
		if(insn_valid) begin
			case(insn)

				REG_FPGA_IDCODE:		rd_mode	<= 1;
				REG_FPGA_SERIAL:		rd_mode	<= 1;
				REG_ETH0_MDIO_RDATA:	rd_mode <= 1;
				REG_ETH1_MDIO_RDATA:	rd_mode <= 1;
				REG_ETH2_MDIO_RDATA:	rd_mode <= 1;
				REG_ETH3_MDIO_RDATA:	rd_mode <= 1;

				//Reset count during read operations
				default: begin
					rd_mode	<= 0;
					count	<= 0;
				end

			endcase
		end

		//Process incoming data
		if(wr_valid) begin
			count		<= count + 1;

			case(insn)

				REG_ETH0_RST: cfgregs.phy_rst_n[0] <= wr_data[0];
				REG_ETH1_RST: cfgregs.phy_rst_n[1] <= wr_data[0];
				REG_ETH2_RST: cfgregs.phy_rst_n[2] <= wr_data[0];
				REG_ETH3_RST: cfgregs.phy_rst_n[3] <= wr_data[0];

				REG_ETH0_MDIO_RADDR: begin
					cfgregs.mdio_regaddr	<= wr_data[4:0];
					cfgregs.mdio_rd_en[0]	<= 1;
				end
				REG_ETH1_MDIO_RADDR: begin
					cfgregs.mdio_regaddr	<= wr_data[4:0];
					cfgregs.mdio_rd_en[1]	<= 1;
				end
				REG_ETH2_MDIO_RADDR: begin
					cfgregs.mdio_regaddr	<= wr_data[4:0];
					cfgregs.mdio_rd_en[2]	<= 1;
				end
				REG_ETH3_MDIO_RADDR: begin
					cfgregs.mdio_regaddr	<= wr_data[4:0];
					cfgregs.mdio_rd_en[3]	<= 1;
				end

			endcase


		end

		//Reset on CS# falling edge
		if(start) begin
			count		<= 0;
		end

	end

endmodule
