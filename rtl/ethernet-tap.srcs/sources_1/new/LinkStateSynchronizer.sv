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

/**
	@brief Synchronizer for link speed/state
 */
module LinkStateSynchronizer(
	input wire			clk_a,
	input wire			link_up_a,
	input wire[1:0]		link_speed_a,

	input wire			clk_b,
	output wire			link_up_b,
	output wire[1:0]	link_speed_b,
	output wire			updated_b
);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Register inputs in the source domain and detect changes

	logic		updated_a		= 0;

	logic		link_up_a_ff	= 0;
	logic[1:0]	link_speed_a_ff	= 0;

	always_ff @(posedge clk_a) begin
		link_up_a_ff	<= link_up_a;
		link_speed_a_ff	<= link_speed_a;

		updated_a		<= (link_up_a_ff != link_up_a) || (link_speed_a_ff != link_speed_a);
	end

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Actual CDC

	RegisterSynchronizer #(
		.WIDTH(3)
	) sync (
		.clk_a(clk_a),
		.en_a(updated_a),
		.ack_a(),
		.reg_a({link_up_a, link_speed_a}),

		.clk_b(clk_b),
		.updated_b(updated_b),
		.reset_b(1'b0),
		.reg_b({link_up_b, link_speed_b})
	);

endmodule
