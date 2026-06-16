# ============================================================================
# build_bd.tcl — Phase 1b: ZYBO Audio BD with axi_i2s_adi IP
# ============================================================================
# Replaces deprecated d_axi_i2s_audio with Digilent axi_i2s_adi:1.2
# Standard AXI-Stream naming: S_AXIS (playback) / M_AXIS (capture)
# Linux driver: mainline sound/soc/adi/axi-i2s.c — zero custom code needed
# ============================================================================

puts "========================================"
puts " Phase 1b: Create Project (axi_i2s_adi)"
puts "========================================"
create_project zybo_audio_adi ./build_adi -part xc7z010clg400-1 -force
set_property board_part digilentinc.com:zybo:part0:2.0 [current_project]
set_property ip_repo_paths "[file normalize fpga/repo]" [current_project]
update_ip_catalog

puts "\nCreate BD + Ports"
puts "========================================"
create_bd_design "zybo_audio"

set DDR      [create_bd_intf_port -mode Master -vlnv xilinx.com:interface:ddrx_rtl:1.0 DDR]
set FIXED_IO [create_bd_intf_port -mode Master -vlnv xilinx.com:display_processing_system7:fixedio_rtl:1.0 FIXED_IO]
set IIC_port [create_bd_intf_port -mode Master -vlnv xilinx.com:interface:iic_rtl:1.0 IIC]
set btns     [create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gpio_rtl:1.0 btns_4bits]
set ac_bclk  [create_bd_port -dir O ac_bclk]
set ac_mclk  [create_bd_port -dir O ac_mclk]
set ac_muten [create_bd_port -dir O ac_muten]
set ac_pbdat [create_bd_port -dir O ac_pbdat]
set ac_pblrc [create_bd_port -dir O ac_pblrc]
set ac_recdat [create_bd_port -dir I ac_recdat]
set ac_reclrc [create_bd_port -dir O ac_reclrc]

puts "\nAdd IPs"
puts "========================================"
set ps7      [create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 processing_system7_0]
set dma      [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.1 axi_dma_0]
set i2s      [create_bd_cell -type ip -vlnv digilentinc.com:user:axi_i2s_adi:1.2 axi_i2s_adi_0]
set iic      [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_iic:2.0 axi_iic_0]
set gpio     [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_0]
set concat   [create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_0]
set const0   [create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0]
set mem_ic   [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_mem_ic]
set periph_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_periph_ic]
set rst      [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_rst_0]

puts "\nConfigure IPs"
puts "========================================"
set_property -dict [list \
    CONFIG.PCW_UART1_PERIPHERAL_ENABLE {1} CONFIG.PCW_UART1_UART1_IO {MIO 48 .. 49} \
    CONFIG.PCW_SD0_PERIPHERAL_ENABLE {1} CONFIG.PCW_ENET0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_USB0_PERIPHERAL_ENABLE {1} CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {64} \
    CONFIG.PCW_USE_M_AXI_GP0 {1} CONFIG.PCW_EN_CLK0_PORT {1} \
    CONFIG.PCW_FCLK0_PERIPHERAL_CLKSRC {IO PLL} CONFIG.PCW_FCLK0_PERIPHERAL_DIVISOR0 {5} \
    CONFIG.PCW_FCLK0_PERIPHERAL_DIVISOR1 {2} CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100} \
    CONFIG.PCW_IRQ_F2P_INTR {1} CONFIG.PCW_CRYSTAL_PERIPHERAL_FREQMHZ {50.000000} \
    CONFIG.PCW_DDR_RAM_HIGHADDR {0x1FFFFFFF} CONFIG.PCW_PACKAGE_NAME {clg400} \
] $ps7
set_property -dict [list \
    CONFIG.c_include_mm2s {1} CONFIG.c_include_s2mm {1} \
    CONFIG.c_m_axi_mm2s_data_width {64} CONFIG.c_m_axi_s2mm_data_width {64} \
] $dma
set_property -dict [list CONFIG.C_GPIO_WIDTH {4} CONFIG.C_ALL_INPUTS {1}] $gpio
set_property -dict [list CONFIG.NUM_MI {1} CONFIG.NUM_SI {2}] $mem_ic
set_property -dict [list CONFIG.NUM_MI {4} CONFIG.NUM_SI {1}] $periph_ic
set_property -dict [list CONFIG.NUM_PORTS {4}] $concat
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {0}] $const0

puts "\nConnect Wires"
puts "========================================"
set fclk [get_bd_pins $ps7/FCLK_CLK0]
set prst [get_bd_pins $rst/peripheral_aresetn]
set irst [get_bd_pins $rst/interconnect_aresetn]

# DDR + FIXED_IO
connect_bd_intf_net [get_bd_intf_pins $ps7/DDR] [get_bd_intf_ports DDR]
connect_bd_intf_net [get_bd_intf_pins $ps7/FIXED_IO] [get_bd_intf_ports FIXED_IO]
# IIC + GPIO
connect_bd_intf_net [get_bd_intf_pins $iic/IIC] [get_bd_intf_ports IIC]
connect_bd_intf_net [get_bd_intf_pins $gpio/GPIO] [get_bd_intf_ports btns_4bits]
# Mute
connect_bd_net [get_bd_pins $const0/dout] [get_bd_ports ac_muten]

# DMA memory → mem_ic → HP0
connect_bd_intf_net [get_bd_intf_pins $dma/M_AXI_MM2S] [get_bd_intf_pins $mem_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $dma/M_AXI_S2MM] [get_bd_intf_pins $mem_ic/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins $mem_ic/M00_AXI] [get_bd_intf_pins $ps7/S_AXI_HP0]

# DMA stream ⇄ I2S IP (standard S_AXIS/M_AXIS names)
connect_bd_intf_net [get_bd_intf_pins $dma/M_AXIS_MM2S] [get_bd_intf_pins $i2s/S_AXIS]
connect_bd_intf_net [get_bd_intf_pins $i2s/M_AXIS]       [get_bd_intf_pins $dma/S_AXIS_S2MM]

# I2S pins
connect_bd_net [get_bd_pins $i2s/bclk]  [get_bd_ports ac_bclk]
connect_bd_net [get_bd_pins $i2s/lrclk] [get_bd_ports ac_pblrc]
connect_bd_net [get_bd_pins $i2s/lrclk] [get_bd_ports ac_reclrc]
connect_bd_net [get_bd_pins $i2s/sdata_o] [get_bd_ports ac_pbdat]
connect_bd_net [get_bd_pins $i2s/sdata_i] [get_bd_ports ac_recdat]

# GP0 → periph_ic → DMA ctrl + IIC + GPIO + I2S ctrl
connect_bd_intf_net [get_bd_intf_pins $ps7/M_AXI_GP0] [get_bd_intf_pins $periph_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $periph_ic/M00_AXI] [get_bd_intf_pins $dma/S_AXI_LITE]
connect_bd_intf_net [get_bd_intf_pins $periph_ic/M01_AXI] [get_bd_intf_pins $iic/S_AXI]
connect_bd_intf_net [get_bd_intf_pins $periph_ic/M02_AXI] [get_bd_intf_pins $gpio/S_AXI]
connect_bd_intf_net [get_bd_intf_pins $periph_ic/M03_AXI] [get_bd_intf_pins $i2s/S_AXI]

# CLOCKS
connect_bd_net $fclk [get_bd_pins $rst/slowest_sync_clk]
connect_bd_net $fclk [get_bd_pins $dma/s_axi_lite_aclk] [get_bd_pins $dma/m_axi_mm2s_aclk] [get_bd_pins $dma/m_axi_s2mm_aclk]
connect_bd_net $fclk [get_bd_pins $iic/s_axi_aclk] [get_bd_pins $gpio/s_axi_aclk]
connect_bd_net $fclk [get_bd_pins $mem_ic/ACLK] [get_bd_pins $mem_ic/M00_ACLK] [get_bd_pins $mem_ic/S00_ACLK] [get_bd_pins $mem_ic/S01_ACLK]
connect_bd_net $fclk [get_bd_pins $periph_ic/ACLK] [get_bd_pins $periph_ic/S00_ACLK]
connect_bd_net $fclk [get_bd_pins $periph_ic/M00_ACLK] [get_bd_pins $periph_ic/M01_ACLK] [get_bd_pins $periph_ic/M02_ACLK] [get_bd_pins $periph_ic/M03_ACLK]
connect_bd_net $fclk [get_bd_pins $ps7/FCLK_CLK0] [get_bd_pins $ps7/M_AXI_GP0_ACLK] [get_bd_pins $ps7/S_AXI_HP0_ACLK]
connect_bd_net $fclk [get_bd_pins $i2s/s_axi_aclk] [get_bd_pins $i2s/aclk]
connect_bd_net $fclk [get_bd_pins $i2s/s_axis_aclk] [get_bd_pins $i2s/m_axis_aclk]

# RESETS
connect_bd_net [get_bd_pins $ps7/FCLK_RESET0_N] [get_bd_pins $rst/ext_reset_in]
connect_bd_net $prst [get_bd_pins $dma/axi_resetn] [get_bd_pins $iic/s_axi_aresetn]
connect_bd_net $prst [get_bd_pins $gpio/s_axi_aresetn]
connect_bd_net $prst [get_bd_pins $mem_ic/M00_ARESETN] [get_bd_pins $mem_ic/S00_ARESETN] [get_bd_pins $mem_ic/S01_ARESETN]
connect_bd_net $prst [get_bd_pins $periph_ic/M00_ARESETN] [get_bd_pins $periph_ic/M01_ARESETN]
connect_bd_net $prst [get_bd_pins $periph_ic/M02_ARESETN] [get_bd_pins $periph_ic/M03_ARESETN]
connect_bd_net $prst [get_bd_pins $periph_ic/S00_ARESETN]
connect_bd_net $prst [get_bd_pins $i2s/s_axi_aresetn] [get_bd_pins $i2s/s_axis_aresetn] [get_bd_pins $i2s/m_axis_aresetn]
connect_bd_net $irst [get_bd_pins $mem_ic/ARESETN] [get_bd_pins $periph_ic/ARESETN]

# IRQ
connect_bd_net [get_bd_pins $dma/mm2s_introut] [get_bd_pins $concat/In0]
connect_bd_net [get_bd_pins $dma/s2mm_introut] [get_bd_pins $concat/In1]
connect_bd_net [get_bd_pins $iic/iic2intc_irpt]  [get_bd_pins $concat/In2]
connect_bd_net [get_bd_pins $gpio/ip2intc_irpt]  [get_bd_pins $concat/In3]
connect_bd_net [get_bd_pins $concat/dout]          [get_bd_pins $ps7/IRQ_F2P]

puts "\nValidate + Save"
puts "========================================"
assign_bd_address
validate_bd_design
save_bd_design

set design_name [get_bd_designs]
make_wrapper -files [get_files ${design_name}.bd] -top
add_files -norecurse [glob ./build_adi/zybo_audio_adi.srcs/sources_1/bd/${design_name}/hdl/${design_name}_wrapper.v]
add_files -fileset constrs_1 fpga/src/constraints/ZYBO_Master.xdc
save_bd_design

puts "\n========================================"
puts " BD Created with axi_i2s_adi. Next: launch_runs synth_1"
puts "========================================"
