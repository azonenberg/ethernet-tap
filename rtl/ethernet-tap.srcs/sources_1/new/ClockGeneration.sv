`timescale 1ns / 1ps
`default_nettype none
/**
	@file
	@brief 	Clock synthesis
	@author	Andrew D. Zonenberg
 */
module ClockGeneration(
	input wire	clk_25mhz,

	output wire	clk_system,	//100 MHz main system clock
	output wire	clk_125mhz,	//125 MHz Ethernet clock
	output wire	clk_250mhz,	//250 MHz RGMII SERDES clock
	output wire	clk_200mhz,	//200 MHz IODELAY clock
	output wire	clk_50mhz,	//50 MHz clock for DNA_PORT

	output wire	pll_locked
);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Clock generation PLL

	wire		clk_unused;

	ReconfigurablePLL #(
		.OUTPUT_GATE(6'b011111),
		.OUTPUT_BUF_GLOBAL(6'b011111),
		.OUTPUT_BUF_LOCAL(6'b000000),
		.OUTPUT_BUF_IO(6'b000000),
		.IN0_PERIOD(40),
		.IN1_PERIOD(40),
		.OUT0_MIN_PERIOD(10),	//100 MHz for system core
		.OUT1_MIN_PERIOD(8),	//125 MHz for Ethernet
		.OUT2_MIN_PERIOD(4),	//250 MHz for RGMII oversampling (DDR)
		.OUT3_MIN_PERIOD(5),	//200 MHz IODELAY clock
		.OUT4_MIN_PERIOD(20),	//50 MHz DNA_PORT clock
		.OUT5_MIN_PERIOD(10),
		.OUT0_DEFAULT_PHASE(0),
		.OUT1_DEFAULT_PHASE(0),
		.OUT2_DEFAULT_PHASE(0),
		.OUT5_DEFAULT_PHASE(270),
		.ACTIVE_ON_START(1)
	) pll (
		.clkin({ clk_25mhz, clk_25mhz }),
		.clksel(1'b0),
		.clkout({clk_unused, clk_50mhz, clk_200mhz, clk_250mhz, clk_125mhz, clk_system}),
		.reset(1'b0),
		.locked(pll_locked),
		.busy(),
		.reconfig_clk(clk_25mhz),
		.reconfig_start(1'b0),
		.reconfig_finish(1'b0),
		.reconfig_cmd_done(),
		.reconfig_vco_en(1'b0),
		.reconfig_vco_mult(7'h0),
		.reconfig_vco_indiv(7'h0),
		.reconfig_vco_bandwidth(1'h0),
		.reconfig_output_en(1'h0),
		.reconfig_output_idx(3'h0),
		.reconfig_output_div(8'h0),
		.reconfig_output_phase(9'h0)
	);

endmodule
