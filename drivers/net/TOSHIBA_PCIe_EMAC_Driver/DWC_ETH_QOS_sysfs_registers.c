#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_yapphdr.h"
#include "DWC_ETH_QOS_drv.h"
#include "DWC_ETH_QOS_yregacc.h"

struct range_def {
	int start, end;
};
struct decode_def {
	int idx;
	char *n;
};
struct field_def {
	int regidx;
	char *n;
	int start, cnt;
	int decstart, deccnt, decisar;
	char *desc;
	struct {
		unsigned int valid;
		unsigned int v;
		char *n;
	} dec;
};
struct reg_def {
	char *n;
	int fstart, fcnt;
	unsigned int call; unsigned int arg[3];
	char *desc;
	struct {
		unsigned int valid;
		unsigned int regval;
	} dec;
};

struct group_def {
	char *n;
	int start, cnt;
};

typedef unsigned int (*acc_fn)(int a, int b, int c);


/* Ranges: */
#define RANGE_DEF_CNT 50
static struct range_def range_def[RANGE_DEF_CNT] = {
	{ 0,0 } /* --- ranges for mac_config.31-arpen */,
	{ 31,31 } /* mac_config.31-arpen */,
	{ 0,0 } /* --- ranges for mac_config.30-sarc */,
	{ 28,30 } /* mac_config.30-sarc */,
	{ 0,0 } /* --- ranges for mac_config.27-ipc */,
	{ 27,27 } /* mac_config.27-ipc */,
	{ 0,0 } /* --- ranges for mac_config.26-ipg */,
	{ 24,26 } /* mac_config.26-ipg */,
	{ 0,0 } /* --- ranges for mac_config.23-gpslce */,
	{ 23,23 } /* mac_config.23-gpslce */,
	{ 0,0 } /* --- ranges for mac_config.22-s2kp */,
	{ 22,22 } /* mac_config.22-s2kp */,
	{ 0,0 } /* --- ranges for mac_config.21-cst */,
	{ 21,21 } /* mac_config.21-cst */,
	{ 0,0 } /* --- ranges for mac_config.20-acs */,
	{ 20,20 } /* mac_config.20-acs */,
	{ 0,0 } /* --- ranges for mac_config.19-wd */,
	{ 19,19 } /* mac_config.19-wd */,
	{ 0,0 } /* --- ranges for mac_config.18-be */,
	{ 18,18 } /* mac_config.18-be */,
	{ 0,0 } /* --- ranges for mac_config.17-jd */,
	{ 17,17 } /* mac_config.17-jd */,
	{ 0,0 } /* --- ranges for mac_config.16-je */,
	{ 16,16 } /* mac_config.16-je */,
	{ 0,0 } /* --- ranges for mac_config.15-ps */,
	{ 15,15 } /* mac_config.15-ps */,
	{ 0,0 } /* --- ranges for mac_config.14-fes */,
	{ 14,14 } /* mac_config.14-fes */,
	{ 0,0 } /* --- ranges for mac_config.13-dm */,
	{ 13,13 } /* mac_config.13-dm */,
	{ 0,0 } /* --- ranges for mac_config.12-lm */,
	{ 12,12 } /* mac_config.12-lm */,
	{ 0,0 } /* --- ranges for mac_config.11-ecrsfd */,
	{ 11,11 } /* mac_config.11-ecrsfd */,
	{ 0,0 } /* --- ranges for mac_config.10-do */,
	{ 10,10 } /* mac_config.10-do */,
	{ 0,0 } /* --- ranges for mac_config.09-dcrs */,
	{ 9,9 } /* mac_config.09-dcrs */,
	{ 0,0 } /* --- ranges for mac_config.08-dr */,
	{ 8,8 } /* mac_config.08-dr */,
	{ 0,0 } /* --- ranges for mac_config.06-bl */,
	{ 5,6 } /* mac_config.06-bl */,
	{ 0,0 } /* --- ranges for mac_config.04-dc */,
	{ 4,4 } /* mac_config.04-dc */,
	{ 0,0 } /* --- ranges for mac_config.03-prelen */,
	{ 2,3 } /* mac_config.03-prelen */,
	{ 0,0 } /* --- ranges for mac_config.01-te */,
	{ 1,1 } /* mac_config.01-te */,
	{ 0,0 } /* --- ranges for mac_config.00-re */,
	{ 0,0 } /* mac_config.00-re */};

/* Decodes: */
#define DECODE_DEF_CNT 0
static struct decode_def decode_def[DECODE_DEF_CNT] = {
};

/* fields: */
#define FIELD_DEF_CNT 25
static struct field_def field_def[FIELD_DEF_CNT] = {
	{ 0, "31-arpen", /* range: */ 1, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"ARP Offload Enable\n          When this bit is set, the MAC can recognize an incoming ARP request packet and\n          schedules the ARP response packet for transmission. It will forward the ARP\n          packet to the application and also indicate the events in the RxStatus.\n          When this bit is reset, the MAC receiver does not recognize any ARP packet and\n          indicates them as Type frame in the RxStatus.\n          This bit is available only when the Enable IPv4 ARP Offload is selected.\",,,,,, " } /*  */,
	{ 0, "30-sarc", /* range: */ 3, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Source Address Insertion or Replacement Control\n          This field controls the source address insertion or replacement for all transmitted\n          packets. Bit 30 specifies which MAC Address register (0 or 1) is used for source\n          address insertion or replacement based on the values of Bits[29:28]:\n          2'b0x: The mti_sa_ctrl_i and ati_sa_ctrl_i input signals control the SA field\n          generation.\n          2'b10:\n          - If Bit 30 is set to 0, the MAC inserts the content of the MAC Address 0 registers\n          (MAC registers 192 and 193) in the SA field of all transmitted packets.\n          - If Bit 30 is set to 1 and the Enable MAC Address Register 1 option is selected\n          while configuring the core, the MAC inserts the content of the MAC Address 1\n          registers (MAC registers 194 and 195) in the SA field of all transmitted packets.\n          2'b11:\n          - If Bit 30 is set to 0, the MAC replaces the content of the MAC Address 0\n          registers (MAC registers 192 and 193) in the SA field of all transmitted packets.\n          - If Bit 30 is set to 1 and the MAC Address Register 1 is enabled, the MAC\n          replaces the content of the MAC Address 1 registers (MAC registers 194 and\n          195) in the SA field of all transmitted packets.\n          Note:\n          Changes to this field take effect only on the start of a packet. If you write to this\n          register field when a packet is being transmitted, only the subsequent packet can\n          use the updated value, that is, the current packet does not use the updated value.\n          These bits are reserved and R when the Enable SA and VLAN Insertion on Tx\n          feature is not selected while configuring the core.\",,,,,, " } /*  */,
	{ 0, "27-ipc", /* range: */ 5, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Checksum Offload\n          When set, this bit enables the IPv4 header checksum checking and IPv4 or IPv6\n          TCP, UDP, or ICMP payload checksum checking. When this bit is reset, the COE\n          function in the receiver is disabled.\n          If the IP Checksum Offload feature is not enabled while configuring the core, this bit\n          is reserved and R (with default value).\n          The Layer 3 and Layer 4 Packet Filter and Enable Split Header features\n          automatically selects the IPC Full Checksum Offload Engine on the Receive side.\n          When any of these features are enabled, you must set the IPC bit.\",,,,,, " } /*  */,
	{ 0, "26-ipg", /* range: */ 7, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Inter-Packet Gap\n          These bits control the minimum IPG between packets during transmission.\n          000: 96 bit times\n          001: 88 bit times\n          010: 80 bit times\n          ...\n          111: 40 bit times\n          This range of minimum IPG is valid in full-duplex mode.\n          In the half-duplex mode, the minimum IPG can be configured only for 64- bit times\n          (IPG = 100). Lower values are not considered.\n          When a JAM pattern is being transmitted because of backpressure activation, the\n          MAC does not consider the minimum IPG.\n          The above function (IPG less than 96 bit times) is valid only when EIPGEN bit in\n          MAC_Ext_Configuration register is reset. When EIPGEN is set, then the minimum\n          IPG (greater than 96 bit times) is controlled as per the description given in EIPG\n          field in MAC_Ext_Configuration register.\",,,,,, " } /*  */,
	{ 0, "23-gpslce", /* range: */ 9, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Giant Packet Size Limit Control Enable\n          When this bit is set, the MAC considers the value in GPSL field in\n          MAC_Ext_Configuration register to declare a received packet as Giant packet. This\n          field must be programmed to more than 1,518 bytes.\n          Otherwise, the MAC considers 1,518 bytes as giant packet limit.\n          When this bit is reset, the MAC considers a received packet as Giant packet when\n          its size is greater than 1,518 bytes (1522 bytes for tagged packet).\n          The watchdog timeout limit, Jumbo Packet Enable and 2K Packet Enable have\n          higher precedence over this bit, that is the MAC considers a received packet as\n          Giant packet when its size is greater than 9,018 bytes (9,022 bytes for tagged\n          packet) with Jumbo Packet Enabled and greater than 2,000 bytes with 2K Packet\n          Enabled. The watchdog timeout, if enabled, terminates the received packet when\n          watchdog limit is reached. Therefore, the programmed giant packet limit should be\n          less than the watchdog limit to get the giant packet status.\",,,,,, " } /*  */,
	{ 0, "22-s2kp", /* range: */ 11, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"IEEE 802.3as Support for 2K Packets\n          When this bit is set, the MAC considers all packets with up to 2,000 bytes length as\n          normal packets. When the JE bit is not set, the MAC considers all received packets\n          of size more than 2K bytes as Giant packets.\n          When this bit is reset and the JE bit is not set, the MAC considers all received\n          packets of size more than 1,518 bytes (1,522 bytes for tagged) as giant packets.\n          For more information about how the setting of this bit and the JE bit impact the\n          Giant packet status, see Table 9-67.\n          Note: When the JE bit is set, setting this bit has no effect on the giant packet\n          status.\",,,,,, " } /*  */,
	{ 0, "21-cst", /* range: */ 13, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"CRC stripping for Type packets\n          When this bit is set, the last four bytes (FCS) of all packets of Ether type (type field\n          greater than 1,536) are stripped and dropped before forwarding the packet to the\n          application. This function is not valid when the IP Checksum Engine (Type 1) is\n          enabled in the MAC receiver. This function is valid when Type 2 Checksum Offload\n          Engine is enabled.\n          Note: For information about how the settings of the ACS bit and this bit impact the\n          packet length, see Table 9-68.\",,,,,, " } /*  */,
	{ 0, "20-acs", /* range: */ 15, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Automatic Pad or CRC Stripping\n          When this bit is set, the MAC strips the Pad or FCS field on the incoming packets\n          only if the value of the length field is less than 1,536 bytes. All received packets\n          with length field greater than or equal to 1,536 bytes are passed to the application\n          without stripping the Pad or FCS field.\n          When this bit is reset, the MAC passes all incoming packets to the application,\n          without any modification.\n          Note: For information about how the settings of CST bit and this bit impact the\n          packet length, see Table 9-68.\",,,,,, " } /*  */,
	{ 0, "19-wd", /* range: */ 17, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Watchdog Disable\n          When this bit is set, the MAC disables the watchdog timer on the receiver. The\n          MAC can receive packets of up to 16,383 bytes.\n          When this bit is reset, the MAC does not allow more than 2,048 bytes (10,240 if JE\n          is set high) of the packet being received. The MAC cuts off any bytes received after\n          2,048 bytes.\",,,,,, " } /*  */,
	{ 0, "18-be", /* range: */ 19, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Packet Burst Enable\n          When this bit is set, the MAC allows packet bursting during transmission in the\n          GMII half-duplex mode. This bit is reserved and read-only (R) in the 10/100 Mbps-\n          only or full-duplex-only configurations.\",,,,,, " } /*  */,
	{ 0, "17-jd", /* range: */ 21, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Jabber Disable\n          When this bit is set, the MAC disables the jabber timer on the transmitter. The MAC\n          can transfer packets of up to 16,383 bytes.\n          When this bit is reset, if the application sends more than 2,048 bytes of data\n          (10,240 if JE is set high) during transmission, the MAC does not send rest of the\n          bytes in that packet.\",,,,,, " } /*  */,
	{ 0, "16-je", /* range: */ 23, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Jumbo Packet Enable\n          When this bit is set, the MAC allows jumbo packets of 9,018 bytes (9,022 bytes for\n          VLAN tagged packets) without reporting a giant packet error in the Rx packet\n          status.\",,,,,, " } /*  */,
	{ 0, "15-ps", /* range: */ 25, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Port Select\n          This bit selects the Ethernet line speed.\n          0: For 1000 Mbps operations\n          1: For 10 or 100 Mbps operations\n          In 10 or 100 Mbps operations, this bit, along with Bit 14, selects the exact line\n          speed. In the 10/100 Mbps-only (always 1) or 1000 Mbps-only (always\n          0) configurations, this bit is read-only (R) with appropriate value. In default\n          10/100/1000 Mbps configurations, this bit is read-write (R/W). The mac_speed_o[1]\n          signal reflects the value of this bit.\",,,,,, " } /*  */,
	{ 0, "14-fes", /* range: */ 27, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Speed\n          This bit selects the speed in the 10/100 Mbps mode:\n          0: 10 Mbps\n          1: 100 Mbps\n          In 1000 Mbps-only configurations, this bit is read-only with the reset value. In the\n          10 or 100 Mbps-only or default 10/100/1000 Mbps configurations, this bit is read-\n          write. The mac_speed_o[0] signal reflects the value of this bit.\",,,,,, " } /*  */,
	{ 0, "13-dm", /* range: */ 29, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Duplex Mode\n          When this bit is set, the MAC operates in the full-duplex mode in which it can\n          transmit and receive simultaneously. This bit is R with default value of 1'b1 in the\n          full-duplex-only configurations.\",,,,,, " } /*  */,
	{ 0, "12-lm", /* range: */ 31, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Loopback Mode\n          When this bit is set, the MAC operates in the loopback mode at GMII or MII. The\n          (G)MII Rx clock input (clk_rx_i) is required for the loopback to work properly. This is\n          because the Tx clock is not internally looped back.\",,,,,, " } /*  */,
	{ 0, "11-ecrsfd", /* range: */ 33, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Enable Carrier Sense Before Transmission in Full-Duplex Mode\n          When this bit is set, the MAC transmitter checks the CRS signal before packet\n          transmission in the full-duplex mode. The MAC starts the transmission only when\n          the CRS signal is low.\n          When this bit is reset, the MAC transmitter ignores the status of the CRS signal.\",,,,,, " } /*  */,
	{ 0, "10-do", /* range: */ 35, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Disable Receive Own\n          When this bit is set, the MAC disables the reception of packets when the\n          gmii_txen_o is asserted in the half-duplex mode. When this bit is reset, the MAC\n          receives all packets given by the PHY.\n          This bit is not applicable in the full-duplex mode. This bit is reserved and read-only\n          (R) with default value in the full-duplex-only configurations.\",,,,,, " } /*  */,
	{ 0, "09-dcrs", /* range: */ 37, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Disable Carrier Sense During Transmission\n          When this bit is set, the MAC transmitter ignores the (G)MII CRS signal during\n          packet transmission in the half-duplex mode. As a result, no errors are generated\n          because of Loss of Carrier or No Carrier during transmission.\n          When this bit is reset, the MAC transmitter generates errors because of Carrier\n          Sense. The MAC can even abort the transmission.\n          This bit is reserved and read-only (R) in the full-duplex-only configurations.\",,,,,, " } /*  */,
	{ 0, "08-dr", /* range: */ 39, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Disable Retry\n          When this bit is set, the MAC attempts only one transmission. When a collision\n          occurs on the GMII or MII interface, the MAC ignores the current packet\n          transmission and reports a Packet Abort with excessive collision error in the Tx\n          packet status.\n          When this bit is reset, the MAC retries based on the settings of the BL field. This bit\n          is applicable only in the half-duplex mode. This bit is reserved and read-only (R) in\n          the full-duplex-only configurations.\",,,,,, " } /*  */,
	{ 0, "06-bl", /* range: */ 41, 1, /*decode: */ -1, -1, 1, /*desc: */ " \"Back-Off Limit\n          The back-off limit determines the random integer number (r) of slot time delays\n          (4,096 bit times for 1000 Mbps; 512 bit times for 10/100 Mbps) for which the MAC\n          waits before rescheduling a transmission attempt during retries after a collision.\n          00: k = min (n, 10)\n          01: k = min (n, 8)\n          10: k = min (n, 4)\n          11: k = min (n, 1)\n          where n = retransmission attempt\n          The random integer r takes the value in the range 0   r  2k\n          This bit is applicable only in the half-duplex mode. This bit is reserved and read-\n          only (R) in the full-duplex-only configurations.\",,,,,, " } /*  */,
	{ 0, "04-dc", /* range: */ 43, 1, /*decode: */ -1, -1, 1, /*desc: */ " Deferral Check When this bit is set, the deferral check function is enabled in the MAC. The MAC issues a Packet Abort status, along with the excessive deferral error bit set in the Tx packet status, when the Tx state machine is deferred for more than 24,288 bit times in 10 or 100 Mbps mode. If the MAC is configured for 1000 Mbps operation, the threshold for deferral is 155,680 bits times. Deferral begins when the transmitter is ready to transmit, but it is prevented because of an active carrier sense signal (CRS) on GMII or MII. The defer time is not cumulative. For example, if the transmitter defers for 10,000 bit times because the CRS signal is active and the CRS signal becomes inactive, the transmitter transmits and collision happens. Because of collision, the transmitter needs to back off and then defer again after back off completion. In such a scenario, the deferral timer is reset to 0, and it is restarted. When this bit is reset, the deferral check function is disabled and the MAC defers until the CRS signal goes inactive. This bit is applicable only in the half-duplex mode. This bit is reserved and readonly (R) in the full-duplex-only configurations. " } /*  */,
	{ 0, "03-prelen", /* range: */ 45, 1, /*decode: */ -1, -1, 1, /*desc: */ " Preamble Lengt for Transmit packets These bits control the number of preamble bytes that are added to the beginning of every Tx packet. The preamble reduction occurs only when the MAC is operating in the full-duplex mode. 2'b00: 7 bytes of preamble 2'b01: 5 bytes of preamble 2'b10: 3 bytes of preamble 2'b11: Reserved " } /*  */,
	{ 0, "01-te", /* range: */ 47, 1, /*decode: */ -1, -1, 1, /*desc: */ " Transmitter Enable When this bit is set, the Tx state machine of the MAC is enabled for transmission on the GMII or MII interface. When this bit is reset, the MAC Tx state machine is disabled after it completes the transmission of the current packet. The Tx state machine does not transmit any more packets. " } /*  */,
	{ 0, "00-re", /* range: */ 49, 1, /*decode: */ -1, -1, 1, /*desc: */ " Receiver Enable When this bit is set, the Rx state machine of the MAC is enabled for receiving packets from the GMII or MII interface. When this bit is reset, the MAC Rx state machine is disabled after it completes the reception of the current packet. The Rx state machine does not receive any more packets from the GMII or MII interface. " } /*  */};

/* regs: */
#define REG_DEF_CNT 1
static struct reg_def reg_def[REG_DEF_CNT] = {
	{ "0x4000a000-mac_config", /* field: */ 0, 25, /* call: */ 2, { 0x4000a000,0,0 },  /*desc: */ "" } /*  */};

/* groups: */
#define GROUP_DEF_CNT 1
static struct group_def group_def[GROUP_DEF_CNT] = {
	{ "", /* string: */ 0, 0 } /*  */};

/* strs: */
#define STR_DEF_CNT 0
static char *str_def[STR_DEF_CNT] = {
};

int mem(unsigned int reg) {
	int val = 0;
	return val;
}

int mem16(unsigned int reg) {
	int val = 0;
	return val;
}

int mem32(unsigned int reg) {
	int val = 0;
	return val;
}


static int verbosedecode = 0;

static int do_decode_field(struct reg_def *reg, struct field_def *f)
{
	int i; unsigned int v = 0;
	struct decode_def *d = &decode_def[f->decstart];
	struct range_def *r = range_def;
	for (i = f->start; i < f->start + f->cnt; i++) {
		int cnt = (r[i].end - r[i].start) + 1;
		v = v << cnt;
		v |= (reg->dec.regval >> r[i].start) & ((1 << cnt)-1);
	}
	f->dec.v = v;
	f->dec.n = 0;
	if (f->decstart != -1 && f->deccnt != -1 && d) {
		if (f->decisar)
			f->dec.n = d[v].n;
		else {
			for (i = 0; i < f->deccnt; i++) {
				if (d[i].idx == v) {
					f->dec.n = d[i].n;
					break;
				}
			}
		}
	}
	f->dec.valid = 1;
	return 0;
}

static int decode_field(struct seq_file *file, int ridx, int fidx, int val)
{
	struct reg_def *reg = &reg_def[ridx];
	struct field_def *f = &field_def[fidx];

	if ((ridx >= REG_DEF_CNT) ||
	    (fidx >= FIELD_DEF_CNT ||
	     f->regidx != ridx )) {
		seq_printf(file, "register/field index out of range");
		return 0;
	}
	reg->dec.regval = val;
	if (fidx != -1) {
		do_decode_field(reg, f);
		if (verbosedecode) {
			seq_printf(file, "0x%x: %s\nreg '%s': 0x%04x\n", f->dec.v, f->dec.n ? f->dec.n : "", reg->n, reg->dec.regval);
		} else {
			seq_printf(file,  "0x%x", f->dec.v);
		}
	} else {
		seq_printf(file, "0x%x", val);
	}
	return 0;
}


static int do_encode_field(struct reg_def *reg, struct field_def *f, unsigned int wval)
{
	int i; unsigned int v = reg->dec.regval;
	struct range_def *r = range_def;
	for (i = (f->start + f->cnt) - 1; i >= f->start; i--) {
		int cnt = (r[i].end - r[i].start) + 1;
		v &= ~(((1 << cnt)-1) << r[i].start);
		v |= (wval & ((1 << cnt)-1)) << r[i].start;
		wval = wval >> cnt;
	}
	return v;
}

static int encode_field(unsigned long d, int ridx, int fidx, int val, unsigned int *wval)
{
	struct reg_def *reg = &reg_def[ridx];
	struct field_def *f = &field_def[fidx];
	if ((ridx >= REG_DEF_CNT) ||
	    (fidx >= FIELD_DEF_CNT)) {
		printk(KERN_WARNING "register/field index out of range");
		return -1;
	}

	if (fidx == -1) {
		*wval = d;
		return 0;
	}

	reg->dec.regval = val;
	val = do_encode_field(reg, f, d);
	*wval = val;
	return 0;
}

/************************************************/

struct field_entry
{
	int fieldidx;
	void *data;
};

static int field_debugfs_show(struct seq_file *file, void *data)
{
	struct field_entry *field = (struct field_entry *) file->private;
	struct net_device *dev = (struct net_device *) field->data;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	long unsigned int reg_val;
	struct field_def *fdef = &field_def[field->fieldidx];
	struct reg_def *reg; unsigned int addr;

	if (field->fieldidx >= FIELD_DEF_CNT ||
	    fdef->regidx >= REG_DEF_CNT)
		return -ENOENT;
	reg = &reg_def[fdef->regidx];
	addr = reg->arg[0];
	if (addr < 0x40000000 ||
	    addr >= (0x40000000 + 0x10000))
		return -ENOENT;
	addr -= 0x40000000;

	reg_val = hw_if->tc9560_reg_rd(addr, 0, pdata);

	decode_field(file, fdef->regidx, field->fieldidx, reg_val);
	return 0;
}

static int field_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, field_debugfs_show, inode->i_private);
}

static ssize_t field_write_seq(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	struct field_entry *field = ((struct seq_file *)file->private_data)->private;
	struct net_device *dev = (struct net_device *) field->data;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	long unsigned int reg_val;
	struct field_def *fdef = &field_def[field->fieldidx];
	struct reg_def *reg; unsigned int addr; unsigned int wval = 0;
	int rc;
	long val;
	char *kbuf;

	if (field->fieldidx >= FIELD_DEF_CNT ||
	    fdef->regidx >= REG_DEF_CNT)
		return -ENOENT;
	reg = &reg_def[fdef->regidx];
	addr = reg->arg[0];
	if (addr < 0x40000000 ||
	    addr >= (0x40000000 + 0x10000))
		return -ENOENT;
	addr -= 0x40000000;

	reg_val = hw_if->tc9560_reg_rd(addr, 0, pdata);

	kbuf = memdup_user_nul(buf, len);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);
	rc = kstrtol(kbuf, 0, &val);
	kfree(kbuf);
	if (rc)
		return rc;

	encode_field(val, fdef->regidx, field->fieldidx, reg_val, &wval);

	/* bar-0 write */
	printk("[0x4000 %04x] <= 0x%08x\n" , addr, wval);

	hw_if->tc9560_reg_wr(addr, wval, 0, pdata);

	return len;
};

static const struct file_operations field_ops =
{
	.owner = THIS_MODULE,
	.open = field_seq_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = field_write_seq,
};

struct dentry *debugfs_create_field(struct dentry *parent,
				    int fieldidx,
				    void *data)
{
	struct field_entry *f;
	struct field_def *fdef = &field_def[fieldidx];

	if (IS_ERR(parent) || (fieldidx >= FIELD_DEF_CNT))
		return ERR_PTR(-ENOENT);

	f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	f->fieldidx = fieldidx;
	f->data = data;

	return debugfs_create_file(fdef->n, S_IRUGO, parent, f,
				   &field_ops);
}
EXPORT_SYMBOL_GPL(debugfs_create_field);

/************************************************/

struct reg_entry
{
	int regidx;
	void *data;
	struct dentry *reg_root;
};

static int reg_debugfs_show(struct seq_file *file, void *data)
{
	struct reg_entry *reg = (struct reg_entry *) file->private;
	struct net_device *dev = (struct net_device *) reg->data;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct reg_def *rdef = &reg_def[reg->regidx];
	int fieldidx; unsigned int addr;
	long unsigned int reg_val;

	addr = rdef->arg[0];
	if (addr < 0x40000000 ||
	    addr >= (0x40000000 + 0x10000))
		return -ENOENT;
	addr -= 0x40000000;
	reg_val = hw_if->tc9560_reg_rd(addr, 0, pdata);

	for (fieldidx = rdef->fstart; fieldidx < rdef->fstart+rdef->fcnt; fieldidx++) {
		struct field_def *fdef = &field_def[fieldidx];
		seq_printf(file, "%-16s:\n%s\n", fdef->n, fdef->desc);
	}
	for (fieldidx = rdef->fstart; fieldidx < rdef->fstart+rdef->fcnt; fieldidx++) {
		struct field_def *fdef = &field_def[fieldidx];
		seq_printf(file, "%-16s:", fdef->n);
		decode_field(file, reg->regidx, fieldidx, reg_val);
		seq_printf(file, "\n");
	}

	return 0;
}

static int reg_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, reg_debugfs_show, inode->i_private);
}

static const struct file_operations reg_ops =
{
	.owner = THIS_MODULE,
	.open = reg_seq_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek
};

struct dentry *debugfs_create_reg(struct dentry *parent,
				  int regidx,
				  void *data)
{
	char b[256];
	struct reg_entry *r;
	struct reg_def *reg = &reg_def[regidx];
	int fieldidx;


	if (IS_ERR(parent) || (regidx >= REG_DEF_CNT))
		return ERR_PTR(-ENOENT);

	r = kmalloc(sizeof(*r), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	r->regidx = regidx;
	r->data = data;

	sprintf(b, ".%s", reg->n);
	r->reg_root = debugfs_create_dir(b, parent);

	for (fieldidx = reg->fstart; fieldidx < reg->fstart+reg->fcnt; fieldidx++) {
		debugfs_create_field(r->reg_root, fieldidx, data);
	}

	return debugfs_create_file(reg->n, S_IRUGO, parent, r,
				   &reg_ops);

	return r->reg_root;
}

void dwc_create_debugfs(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	if (!( pdata->debugfs_root = debugfs_create_dir("DWC", NULL)))
		goto out;

	if (!( pdata->debugfs_root_regs = debugfs_create_dir("regs", pdata->debugfs_root) ))
		goto out;

	debugfs_create_reg(pdata->debugfs_root_regs, 0, dev);
out:
	return;
}
EXPORT_SYMBOL_GPL(dwc_create_debugfs);

void dwc_remove_debugfs(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	debugfs_remove_recursive(pdata->debugfs_root);
}
EXPORT_SYMBOL_GPL(dwc_remove_debugfs);
