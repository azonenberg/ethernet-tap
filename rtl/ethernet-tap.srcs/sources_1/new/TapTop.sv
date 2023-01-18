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
	/*output logic		portA_rst_n = 0,
	output logic		portB_rst_n = 0,
	output logic		monA_rst_n = 0,
	output logic		monB_rst_n = 0,*/

	output wire			portA_rst_n,
	output wire			portB_rst_n,
	output wire			monA_rst_n,
	output wire			monB_rst_n,

	/*
	//MDIO buses
	inout wire			portA_mdio,
	output wire			portA_mdc,
	inout wire			portB_mdio,
	output wire			portB_mdc,
	inout wire			monA_mdio,
	output wire			monA_mdc,
	inout wire			monB_mdio,
	output wire			monB_mdc,
	*/
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

	MicrocontrollerInterface mgmt(
		.clk_50mhz(clk_50mhz),
		.clk_125mhz(clk_125mhz),
		.qspi_sck(qspi_sck),
		.qspi_cs_n(qspi_cs_n),
		.qspi_dq(qspi_dq),
		.irq(mcu_irq)

		//TODO: config registers
	);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PHY resets

	/*
	//Bring up the PHY after a little while
	logic[15:0] eth_rst_count = 1;
	always_ff @(posedge clk_25mhz) begin
		if(eth_rst_count == 0) begin
			portA_rst_n		<= 1;
			portB_rst_n		<= 1;
			monA_rst_n		<= 1;
			monB_rst_n		<= 1;
		end
		else
			eth_rst_count	<= eth_rst_count + 1'h1;
	end
	*/

	vio_1 vio_reset(
		.clk(clk_125mhz),
		.probe_out0(portA_rst_n),
		.probe_out1(portB_rst_n),
		.probe_out2(monA_rst_n),
		.probe_out3(monB_rst_n)
	);

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

	wire			portA_mac_rx_clk;
	EthernetRxBus	portA_mac_rx_bus;

	EthernetTxBus	portA_mac_tx_bus = 0;
	wire			portA_mac_tx_ready;

	wire			portA_link_up;
	lspeed_t		portA_link_speed;

	RGMIIMACWrapper mac_portA(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(portA_rx_clk),
		.rgmii_rxd(portA_rx_data),
		.rgmii_rx_ctl(portA_rx_en),
		.rgmii_txc(portA_tx_clk),
		.rgmii_txd(portA_tx_data),
		.rgmii_tx_ctl(portA_tx_en),

		.mac_rx_clk(portA_mac_rx_clk),
		.mac_rx_bus(portA_mac_rx_bus),
		.mac_tx_bus(portA_mac_tx_bus),
		.mac_tx_ready(portA_mac_tx_ready),

		.link_up(portA_link_up),
		.link_speed(portA_link_speed)
	);

	wire			portB_mac_rx_clk;
	EthernetRxBus	portB_mac_rx_bus;

	EthernetTxBus	portB_mac_tx_bus = 0;
	wire			portB_mac_tx_ready;

	wire			portB_link_up;
	lspeed_t		portB_link_speed;

	RGMIIMACWrapper mac_portB(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(portB_rx_clk),
		.rgmii_rxd(portB_rx_data),
		.rgmii_rx_ctl(portB_rx_en),
		.rgmii_txc(portB_tx_clk),
		.rgmii_txd(portB_tx_data),
		.rgmii_tx_ctl(portB_tx_en),

		.mac_rx_clk(portB_mac_rx_clk),
		.mac_rx_bus(portB_mac_rx_bus),
		.mac_tx_bus(portB_mac_tx_bus),
		.mac_tx_ready(portB_mac_tx_ready),

		.link_up(portB_link_up),
		.link_speed(portB_link_speed)
	);

	wire			monA_mac_rx_clk;
	EthernetRxBus	monA_mac_rx_bus;

	EthernetTxBus	monA_mac_tx_bus = 0;
	wire			monA_mac_tx_ready;

	wire			monA_link_up;
	lspeed_t		monA_link_speed;

	RGMIIMACWrapper mac_monA(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(monA_rx_clk),
		.rgmii_rxd(monA_rx_data),
		.rgmii_rx_ctl(monA_rx_en),
		.rgmii_txc(monA_tx_clk),
		.rgmii_txd(monA_tx_data),
		.rgmii_tx_ctl(monA_tx_en),

		.mac_rx_clk(monA_mac_rx_clk),
		.mac_rx_bus(monA_mac_rx_bus),
		.mac_tx_bus(monA_mac_tx_bus),
		.mac_tx_ready(monA_mac_tx_ready),

		.link_up(monA_link_up),
		.link_speed(monA_link_speed)
	);

	wire			monB_mac_rx_clk;
	EthernetRxBus	monB_mac_rx_bus;

	EthernetTxBus	monB_mac_tx_bus = 0;
	wire			monB_mac_tx_ready;

	wire			monB_link_up;
	lspeed_t		monB_link_speed;

	RGMIIMACWrapper mac_monB(
		.clk_125mhz(clk_125mhz),
		.clk_250mhz(clk_250mhz),

		.rgmii_rxc(monB_rx_clk),
		.rgmii_rxd(monB_rx_data),
		.rgmii_rx_ctl(monB_rx_en),
		.rgmii_txc(monB_tx_clk),
		.rgmii_txd(monB_tx_data),
		.rgmii_tx_ctl(monB_tx_en),

		.mac_rx_clk(monB_mac_rx_clk),
		.mac_rx_bus(monB_mac_rx_bus),
		.mac_tx_bus(monB_mac_tx_bus),
		.mac_tx_ready(monB_mac_tx_ready),

		.link_up(monB_link_up),
		.link_speed(monB_link_speed)
	);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// VIOs and ILAs for debug

	vio_0 vioA(
		.clk(portA_mac_rx_clk),
		.probe_in0(portA_link_up),
		.probe_in1(portA_link_speed)
		);

	vio_0 vioB(
		.clk(portB_mac_rx_clk),
		.probe_in0(portB_link_up),
		.probe_in1(portB_link_speed)
		);

	vio_0 vioMonA(
		.clk(monA_mac_rx_clk),
		.probe_in0(monA_link_up),
		.probe_in1(monA_link_speed)
		);

	vio_0 vioMonB(
		.clk(monB_mac_rx_clk),
		.probe_in0(monB_link_up),
		.probe_in1(monB_link_speed)
		);

	ila_0 ilaA(
		.clk(portA_mac_rx_clk),
		.probe0(portA_mac_rx_bus));

	ila_0 ilaB(
		.clk(portB_mac_rx_clk),
		.probe0(portB_mac_rx_bus));

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// MDIO interfaces

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// MCU interface

endmodule
