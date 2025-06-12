configuration management
port0 = BR
port1 = BR
port4 = Eth

sysFS nodes are created in: /sys/bus/spi/devices/spi1.0/

	port0/
		- loopback
			register 0x090FFFC0 mask 0x4000
			R: return current loopback mode, one of {ON = 0x4000, OFF = 0x0000}
			W: set loopback mode {ON, OFF}

		- power
			register 0x090FFFC0 mask 0x0800
			R: return current powermode, one of {ON = 0x0800, OFF = 0x0000}
			W: set powermode {ON, OFF}

		- control_reg
			register 0x090FFFC0
			R: return port0 control register value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- rmt_loopback
			register 0x090F2058 mask 0x8000
			R: return current remote_loopback mode, one of {ON = 0x8000, OFF = 0x0000}
			W: set remote_loopback mode {ON, OFF}

		- speed
			register 0x090FFFC0 mask 0x03C0
			R: return current port speed, one of {100 = 0x0200, DEF = 0x0000}
			W: set current port speed {100 = 100MBPS, DEF = default}

		- error_cn
			sum of registers 0x0B002080 + 0x0B002084 + 0x0B0020AC + 0x0B0020B0 + 0x0B0020B4
			R: return port0 RX error counter value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- packet_cnt_mode
			register 0x090F205E mask 0x0800
			R: return current packet counter source, one of {TX = 0x0800, RX = 0x0000}
			W: set current packet counter source, one of {TX, RX}

		- packet_cnt
			register 0x090F2060
			R: return current packet counter value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- link_state_pass
			register 0x090F201C mask 0x1000
			R: return current link state force pass mode, one of {ON = 0x1000, OFF = 0x0000}
			W: set  link state force pass mode {ON, OFF}

		- auto_mdix
			register 0x0A0F205E mask 0x1000
			R: return current 100BASE-TX Forced Mode Auto-MDIX, one of {ON = 0x0200, OFF = 0x0000}
			W: set 100BASE-TX Forced Mode Auto-MDIX, one of {ON, OFF}

		- cable_diag
			register 0x090F2540 mask 0X0800
			R: return current status of cable diagnostic, one of {COMPLETE = 0X0000, IN_PROGRESS = 0X0800}
			W: start cable diagnostic {START = 0x8000}

		- cable_diag_fault
			register 0x090F2542
			R: return Cable Diagnostics Fault Type value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- cable_diag_len
			register 0x090F2544
			R: return Cable Lenght or fault location value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- SQI_read_mode
			register 0x090F2050 mask 0x0800
			R: return current Signal quality indicator read from register mode, one of {ON = 0x0800, OFF = 0x0000}
			W: set Signal quality indicator read from register mode, one of {ON, OFF}

		- SQI
			register 0x090F2210
			R: return Signal Quality Indicator data for calculation value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- RX_good_packet_cnt
			sum of registers 0x0B002094 + 0x0B002098 + 0x0B00209C
			R: return number of non error received packets
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

	port1/
		- loopback
			register 0x094FFFC0 mask 0x1000
			R: return current loopback mode, one of {ON = 0x4000, OFF = 0x0000}
			W: set loopback mode {ON, OFF}

		- power
			register 0x094FFFC0 mask 0x0800
			R: return current powermode, one of {ON = 0x0000, OFF = 0x0800}
			W: set powermode {ON, OFF}

		- control_reg
			register 0x094FFFC0
			R: return port1 control register value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

	port4/
		- loopback
			register 0x0A0FFFC0 mask 0x1000
			R: return current loopback mode, one of {ON = 0x4000, OFF = 0x0000}
			W: set loopback mode {ON, OFF}

		- power
			register 0x0A0FFFC0 mask 0x1000
			R: return current powermode, one of {ON = 0x0000, OFF = 0x0800}
			W: set powermode {ON, OFF}

		- control_reg
			register 0x0A0FFFC0
			R: return port4 control register value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- auto_neg
			register 0x0A0FFFC0 mask 0x1000
			R: return current autonegotiation mode, one of {ON = 0x1000, OFF = 0x0000}
			W: set autonegotiation mode {ON, OFF}

		- rmt_loopback
			register 0x0A0F2058 mask 0x1000
			R: return current remote_loopback mode, one of {ON = 0x8000, OFF = 0x0000}
			W: set remote_loopback mode {ON, OFF}

		- speed register
			0x0A0F2058 mask 0x2040
			R: return current port speed, one of {100 = 0x2000, DEF = 0x0000}
			W: set current port speed {100 = 100MBPS, DEF = default}

		- error_cnt
			sum of registers 0x0B002480 + 0x0B002484 + 0x0B0024AC + 0x0B0024B0 + 0x0B0024B4
			R: return port4 RX error counter value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- packet_cnt_mode
			register 0x0A0F205E mask 0x0800
			R: return current packet counter source, one of {TX = 0x0000, RX = 0x0800}
			W: set current packet counter source, one of {TX, RX}

		- packet_cnt
			register 0x0A0F2060
			R: return current packet counter value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- link_state_pass
			register 0x0A0F201C mask 0x1000
			R: return current link state force pass mode, one of {ON = 0x1000, OFF = 0x0000}
			W: set  link state force pass mode {ON, OFF}

		- SQI_read_mode
			register 0x0A0F2050 mask 0x0800
			R: return current Signal quality indicator read from register mode, one of {ON = 0x0800, OFF = 0x0000}
			W: set Signal quality indicator read from register mode, one of {ON, OFF}

		- SQI register
			0x0A0F2210
			R: return Signal Quality Indicator data for calculation value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- RX_good_packet_cnt
			sum of registers 0x0B002494 + 0x0B002498 + 0x0B00249C
			R: return number of non error received packets
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

	common/
		- link_stat
			register 0x0B000100
			R: return link status for all ports value
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- model
			register 0x0A800000
			R: return Model Number and revision
				(uint64_t in "llx" format string not including the trailing '\0')
			W:

		- test_mode
			register 0x090FFFCA mask 0xE000
			R: return current testmode, one of:
				- NORMAL = Normal operation = 0x0000
				- TEST_1 = Test mode 1: Transmit droop test mode = 0x2000
				- TEST_2 = Test mode 2: Transmit jitter test in MASTER mode = 0x4000
				- TEST_3 = Test mode 3: Transmit jitter test in SLAVE mode = 0x6000
				- TEST_4 = Test mode 4: Transmitter distortion test = 0x8000
				- TEST_5 = Test mode 5: Normal operation at full power = 0xA000
			W: set testmode one of: {NORMAL, TEST_1, TEST_2, TEST_3, TEST_4, TEST_5}

		- br_phy_strap
			register 0x090FFFC0 mask 0x0208
			R: return Override BRPHY Master/Slave Stap mode , one of {MASTER = 0x0208, SLAVE = 0x0200}
			W: set Override BRPHY Master/Slave Stap mode {MASTER, SLAVE}
	ee_debug/
		NOTICE:This interface is dedicated only for debugging and should not be used by any application
		It allows access to all switch registers, first "bits" and "address" should be set.
		- address
			R: return stored register address
			W: set register address
		- bits
			R: return stored register size
			W: set register sizeone of 8,16,32,64
		- reg_value
			R: return register value (register address and bits must be set before)
			W: set reister value (register address and bits must be set before)
		Example of use return value of 0x0A0F2060 register:
			echo "16" > bits
			echo "0x0A0F2060" > address
			cat reg_value
