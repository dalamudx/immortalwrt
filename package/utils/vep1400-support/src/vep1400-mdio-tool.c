/*
 * vep1400-mdio-tool: MDIO / SMI access tool for Dell EMC VEP1400 (Marvell 88E6190).
 *
 * Clean-room open-source implementation matching the exact CLI and ioctl contracts
 * of the original Dell vep1400-mdio-tool binary.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/mii.h>

static struct ifreq ifr;
static int skfd = -1;

struct counter_entry {
	uint32_t id;
	const char *name;
};

static const struct counter_entry counter_list[] = {
	{ 0x000, "InGoodOctetsLo" },
	{ 0x001, "InGoodOctetsHi" },
	{ 0x002, "InBadOctets" },
	{ 0x01c, "InRxErr" },
	{ 0x01d, "InFCSErr" },
	{ 0x400, "InDiscards" },
	{ 0x401, "InFiltered" },
	{ 0x00e, "OutOctetsLo" },
	{ 0x00f, "OutOctetsHi" },
	{ 0x003, "OutFCSErr" },
	{ 0x41d, "OutDiscards" },
};

static int dbg_printf(int level, const char *fmt, ...)
{
	va_list args;
	char buf[1024];

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (level == -1)
		return fputs(buf, stderr);
	if (level == 1)
		return fputs(buf, stdout);
	return level;
}

static int mdio_read(int fd, int phy_id, int reg_num)
{
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;

	mii->phy_id = (uint16_t)phy_id;
	mii->reg_num = (uint16_t)reg_num;

	if (ioctl(fd, SIOCGMIIREG, &ifr) >= 0)
		return mii->val_out;

	dbg_printf(-1, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
	return -1;
}

static int mdio_write(int fd, int phy_id, int reg_num, uint16_t val)
{
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;

	mii->phy_id = (uint16_t)phy_id;
	mii->reg_num = (uint16_t)reg_num;
	mii->val_in = val;

	if (ioctl(fd, SIOCSMIIREG, &ifr) >= 0)
		return 0;

	dbg_printf(-1, "SIOCSMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
	return -1;
}

static int internal_smi_read_c22(int fd, uint16_t dev, uint16_t reg)
{
	uint16_t cmd = (dev << 5) | 0x9800 | (reg & 0x1f);
	int status = mdio_write(fd, 28, 24, cmd);
	if (status < 0) {
		dbg_printf(-1, "internal SMI read reg SMIC write error\n");
		return status;
	}

	for (int i = 0; i <= 99; i++) {
		usleep(10);
		status = mdio_read(fd, 28, 24);
		if ((status & 0x8000) == 0)
			break;
	}
	if ((status & 0x8000) != 0) {
		dbg_printf(-1, "internal SMI read reg status error\n");
		return -1;
	}

	int val = mdio_read(fd, 28, 25);
	if (val < 0) {
		dbg_printf(-1, "internel smi read reg SMID error\n");
		return -1;
	}
	return val;
}

static int internal_smi_write_c22(int fd, uint16_t dev, uint16_t reg, uint16_t val)
{
	int status = mdio_write(fd, 28, 25, val);
	if (status < 0) {
		dbg_printf(-1, "internal SMI write reg SMID write error\n");
		return status;
	}

	uint16_t cmd = (dev << 5) | 0x9400 | (reg & 0x1f);
	status = mdio_write(fd, 28, 24, cmd);
	if (status < 0) {
		dbg_printf(-1, "internal SMI write reg SMIC write error\n");
		return status;
	}

	for (int i = 0; i <= 99; i++) {
		usleep(10);
		status = mdio_read(fd, 28, 24);
		if ((status & 0x8000) == 0)
			break;
	}
	if ((status & 0x8000) != 0) {
		dbg_printf(-1, "internal SMI write reg status error\n");
		return -1;
	}
	return 0;
}

static int internal_smi_read_c45(int fd, uint16_t dev, uint16_t reg, uint16_t dtype)
{
	int status = mdio_write(fd, 28, 25, reg);
	if (status < 0) {
		dbg_printf(-1, "internal SMI read c45 set reg SMID write error\n");
		return status;
	}

	uint16_t cmd1 = (dev << 5) | 0x8000 | (dtype & 0x1f);
	status = mdio_write(fd, 28, 24, cmd1);
	if (status < 0) {
		dbg_printf(-1, "internal SMI read c45 set reg SMIC write error\n");
		return status;
	}

	for (int i = 0; i <= 99; i++) {
		usleep(10);
		status = mdio_read(fd, 28, 24);
		if ((status & 0x8000) == 0)
			break;
	}
	if ((status & 0x8000) != 0) {
		dbg_printf(-1, "internal SMI read c45 set reg status error\n");
		return -1;
	}

	uint16_t cmd2 = (dev << 5) | 0x8c00 | (dtype & 0x1f);
	status = mdio_write(fd, 28, 24, cmd2);
	if (status < 0) {
		dbg_printf(-1, "internal SMI read c45 get data SMIC write error\n");
		return status;
	}

	for (int j = 0; j <= 99; j++) {
		usleep(10);
		status = mdio_read(fd, 28, 24);
		if ((status & 0x8000) == 0)
			break;
	}
	if ((status & 0x8000) != 0) {
		dbg_printf(-1, "internal SMI read c45 get data status error\n");
		return -1;
	}

	int val = mdio_read(fd, 28, 25);
	if (val < 0) {
		dbg_printf(-1, "internel smi read c45 get data SMID error\n");
		return -1;
	}
	return val;
}

static int internal_smi_write_c45(int fd, uint16_t dev, uint16_t reg, uint16_t dtype, uint16_t val)
{
	int status = mdio_write(fd, 28, 25, reg);
	if (status < 0) {
		dbg_printf(-1, "internal SMI write c45 set reg SMID write error\n");
		return status;
	}

	uint16_t cmd1 = (dev << 5) | 0x8000 | (dtype & 0x1f);
	status = mdio_write(fd, 28, 24, cmd1);
	if (status < 0) {
		dbg_printf(-1, "internal SMI write c45 set reg SMIC write error\n");
		return status;
	}

	for (int i = 0; i <= 99; i++) {
		usleep(10);
		status = mdio_read(fd, 28, 24);
		if ((status & 0x8000) == 0)
			break;
	}
	if ((status & 0x8000) != 0) {
		dbg_printf(-1, "internal SMI write c45 set reg status error\n");
		return -1;
	}

	status = mdio_write(fd, 28, 25, val);
	if (status < 0) {
		dbg_printf(-1, "internal SMI write c45 set data SMID write error\n");
		return status;
	}

	uint16_t cmd2 = (dev << 5) | 0x8400 | (dtype & 0x1f);
	status = mdio_write(fd, 28, 24, cmd2);
	if (status < 0) {
		dbg_printf(-1, "internal SMI write c45 set data SMIC write error\n");
		return status;
	}

	for (int j = 0; j <= 99; j++) {
		usleep(10);
		status = mdio_read(fd, 28, 24);
		if ((status & 0x8000) == 0)
			break;
	}
	if ((status & 0x8000) != 0) {
		dbg_printf(-1, "internal SMI write c45 set data status error\n");
		return -1;
	}
	return 0;
}

static int show_counter(int fd, int port)
{
	if (port > 10) {
		dbg_printf(-1, "invalid port number\n");
		return -1;
	}

	puts("--------------------------------------------------");
	printf(" port: %d counters\n", port);
	puts("--------------------------------------------------");

	uint32_t op_base = ((uint32_t)(port + 1) << 5) | 0xc000;
	for (size_t i = 0; i < sizeof(counter_list) / sizeof(counter_list[0]); i++) {
		uint32_t op = counter_list[i].id | (op_base & 0xfffffbe0);
		int status = mdio_write(fd, 27, 29, (uint16_t)op);
		if (status < 0) {
			dbg_printf(-1, "show counter set reg STS_OP write error\n");
			return status;
		}

		for (int j = 0; j <= 99; j++) {
			usleep(10);
			if ((mdio_read(fd, 27, 29) & 0x8000) == 0)
				break;
		}

		int hi = mdio_read(fd, 27, 30);
		if (hi < 0) {
			dbg_printf(-1, "show counter read reg STS_CNTR_3_2 error\n");
			return -1;
		}
		int lo = mdio_read(fd, 27, 31);
		if (lo < 0) {
			dbg_printf(-1, "show counter read reg STS_CNTR_3_2 error\n");
			return -1;
		}

		uint32_t count = ((uint32_t)hi << 16) | (uint16_t)lo;
		printf("\t%-16s:%10u\n", counter_list[i].name, count);
	}
	return 0;
}

static void usage(void)
{
	puts(
		"vep1400 mdio tool - version 0.7\n"
		"-- basic mdio read/write:\n"
		"  vep1400-mdio-tool <dev> r|w mac|int|ser <daddr> <reg> [<dtype>|<page>] [<val>] [quiet]\n"
		"    - dev      eth device\n"
		"    - r/w      read/write\n"
		"    - mac      MAC register\n"
		"    - int      internal PHY register\n"
		"    - ser      SERDES register\n"
		"    - daddr    value for device address (hex)\n"
		"    - reg      value for register offset (hex)\n"
		"    - dtype    value for device type (hex) (SERDES reg access)\n"
		"    - page     value for register page (hex) (internal PHY access)\n"
		"    - value    value to write (hex) (write mode only)\n"
		"    - quiet    Suppress non error messages\n"
		"\n"
		"-- show port info:\n"
		"  vep1400-mdio-tool <dev> s counter <port>\n"
		"    - dev      eth device\n"
		"    - s        show\n"
		"    - counter  show statistic counters\n"
		"    - port     value for port to show (dec) or \"all\" for all ports\n"
	);
}

int main(int argc, char **argv)
{
	if (argc <= 3) {
		usage();
		return -1;
	}

	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (skfd < 0) {
		perror("socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name) - 1);

	if (mdio_read(skfd, 0, 0) < 0) {
		dbg_printf(-1, "Unable to use interface '%s': %s\n", argv[1], strerror(errno));
		close(skfd);
		return -1;
	}

	int verbose = 1;
	int effective_argc = argc;
	if (strncmp(argv[effective_argc - 1], "quiet", 5) == 0) {
		verbose = 0;
		effective_argc--;
	}

	switch (argv[2][0]) {
	case 'r':
		if (argv[3][0] == 'm') {
			if (effective_argc != 6)
				goto invalid_args;
			int daddr = strtol(argv[4], NULL, 16) & 0x1f;
			int reg = strtol(argv[5], NULL, 16) & 0x1f;
			int val = mdio_read(skfd, daddr, reg);
			if (val < 0) {
				dbg_printf(-1, "mac reg read fail\n");
				close(skfd);
				return -1;
			}
			dbg_printf(verbose, "mac reg read daddr:0x%02x reg:0x%02x\n", daddr, reg);
			dbg_printf(verbose, "0x%.4x\n", val);
			break;
		}
		if (argv[3][0] == 'i') {
			if (effective_argc != 7)
				goto invalid_args;
			int daddr = strtol(argv[4], NULL, 16) & 0x1f;
			int reg = strtol(argv[5], NULL, 16) & 0x1f;
			int page = strtol(argv[6], NULL, 16);
			if (internal_smi_write_c22(skfd, daddr, 22, (uint16_t)page) != 0) {
				dbg_printf(-1, "internal phy page set fail\n");
				close(skfd);
				return -1;
			}
			int val = internal_smi_read_c22(skfd, daddr, reg);
			if (val < 0) {
				dbg_printf(-1, "internal phy reg read fail\n");
				close(skfd);
				return -1;
			}
			dbg_printf(verbose, "internal phy reg read daddr:0x%02x reg:0x%02x page:0x%x\n", daddr, reg, page);
			dbg_printf(verbose, "0x%.4x\n", val);
			break;
		}
		if (argv[3][0] == 's') {
			if (effective_argc != 7)
				goto invalid_args;
			int daddr = strtol(argv[4], NULL, 16) & 0x1f;
			int reg = strtol(argv[5], NULL, 16) & 0xffff;
			int dtype = strtol(argv[6], NULL, 16) & 0x1f;
			int val = internal_smi_read_c45(skfd, daddr, reg, dtype);
			if (val < 0) {
				dbg_printf(-1, "serdes reg read fail\n");
				close(skfd);
				return -1;
			}
			dbg_printf(verbose, "serdes reg read daddr:0x%02x reg:0x%04x dtype:0x%02x\n", daddr, reg, dtype);
			dbg_printf(verbose, "0x%.4x\n", val);
			break;
		}
		goto invalid_args;

	case 'w':
		if (argv[3][0] == 'm') {
			if (effective_argc != 7)
				goto invalid_args;
			int daddr = strtol(argv[4], NULL, 16) & 0x1f;
			int reg = strtol(argv[5], NULL, 16) & 0x1f;
			int val = strtol(argv[6], NULL, 16) & 0xffff;
			if (mdio_write(skfd, daddr, reg, (uint16_t)val) < 0) {
				dbg_printf(-1, "mac reg write fail\n");
				close(skfd);
				return -1;
			}
			dbg_printf(verbose, "mac reg write daddr:0x%02x reg:0x%02x val:0x%04x\n", daddr, reg, val);
			break;
		}
		if (argv[3][0] == 'i') {
			if (effective_argc != 8)
				goto invalid_args;
			int daddr = strtol(argv[4], NULL, 16) & 0x1f;
			int reg = strtol(argv[5], NULL, 16) & 0x1f;
			int page = strtol(argv[6], NULL, 16);
			if (internal_smi_write_c22(skfd, daddr, 22, (uint16_t)page) != 0) {
				dbg_printf(-1, "internal phy page set fail\n");
				close(skfd);
				return -1;
			}
			int val = strtol(argv[7], NULL, 16) & 0xffff;
			if (internal_smi_write_c22(skfd, daddr, reg, (uint16_t)val) < 0) {
				dbg_printf(-1, "internal phy reg write fail\n");
				close(skfd);
				return -1;
			}
			dbg_printf(verbose, "internal phy reg write daddr:0x%02x reg:0x%02x page:0x%x val:0x%04x\n",
				   daddr, reg, page, val);
			break;
		}
		if (argv[3][0] == 's') {
			if (effective_argc != 8)
				goto invalid_args;
			int daddr = strtol(argv[4], NULL, 16) & 0x1f;
			int reg = strtol(argv[5], NULL, 16) & 0xffff;
			int dtype = strtol(argv[6], NULL, 16) & 0x1f;
			int val = strtol(argv[7], NULL, 16) & 0xffff;
			if (internal_smi_write_c45(skfd, daddr, reg, dtype, (uint16_t)val) < 0) {
				dbg_printf(-1, "serdes reg write fail\n");
				close(skfd);
				return -1;
			}
			dbg_printf(verbose, "serdes reg write daddr:0x%02x reg:0x%04x dtype:0x%02x val:0x%04x\n",
				   daddr, reg, dtype, val);
			break;
		}
		goto invalid_args;

	case 's':
		if (argv[3][0] != 'c' || effective_argc != 5)
			goto invalid_args;
		if (strncmp(argv[4], "all", 3) == 0) {
			for (int i = 0; i <= 10; i++) {
				if (show_counter(skfd, i) < 0) {
					dbg_printf(-1, "show port counters fail\n");
					close(skfd);
					return -1;
				}
			}
		} else {
			int port = (uint8_t)strtol(argv[4], NULL, 10);
			if (show_counter(skfd, port) < 0) {
				dbg_printf(-1, "show port counters fail\n");
				close(skfd);
				return -1;
			}
		}
		putchar('\n');
		break;

invalid_args:
		dbg_printf(-1, "invalid arguments\n");
		usage();
		close(skfd);
		return -1;

	default:
		dbg_printf(-1, "invalid arguments\n");
		usage();
		break;
	}

	close(skfd);
	return 0;
}
