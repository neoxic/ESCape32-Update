/*
** Copyright (C) 2022 Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This firmware is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this firmware. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <err.h>
#include "common.h"

#define VERSION "1.0"

#define CMD_PROBE  0
#define CMD_INFO   1
#define CMD_READ   2
#define CMD_WRITE  3
#define CMD_UPDATE 4

#define RES_OK    0
#define RES_ERROR 1

static const char *device = "/dev/ttyUSB0";
static const char *filename;
static int opts;

#define opt_f 1
#define opt_B 2
#define opt_V 4

static int parseargs(int argc, char *argv[]) {
	int opt;
	while ((opt = getopt(argc, argv, "d:BfVvh?")) != -1) {
		switch (opt) {
			case 'd':
				device = optarg;
				break;
			case 'f':
				opts |= opt_f;
				break;
			case 'B':
				opts |= opt_B;
				break;
			case 'V':
			case 'v':
				opts |= opt_V;
				break;
			case 'h':
			case '?':
			default:
				return 0;
		}
	}
	argc -= optind;
	argv += optind;
	if (!argc) return 1;
	if (argc > 1) return 0;
	filename = argv[0];
	return 1;
}

static void checkres(int res, int val, const char *msg) {
	if (res == val || opts & opt_f) return;
	errx(1, "%s (result %d, expected %d)", msg, res, val);
}

static void recvack(int fd, const char *msg) {
	checkres(recvval(fd), RES_OK, msg);
}

static int maxlen(int pos, int size) {
	int len = size - pos;
	if (len > 1024) len = 1024;
	return len;
}

int main(int argc, char *argv[]) {
	if (!parseargs(argc, argv)) {
		fprintf(stderr,
			"Usage: %s [options] [<image>]\n"
			"  <image>      Binary image filename for update\n"
			"               (ESC info is printed if omitted).\n"
			"Options:\n"
			"  -d <device>  Serial device name.\n"
			"  -f           Ignore errors (forced update).\n"
			"  -B           Update bootloader.\n"
			"  -V           Print version.\n",
			argv[0]);
		return 1;
	}
	if (opts & opt_V) {
		printf("ESCape32-Update " VERSION "\n");
		return 0;
	}
	printf("Connecting to ESC via '%s'...\n", device);
	int fd = openserial(device);
	sendval(fd, CMD_PROBE);
	recvack(fd, "Connection failed");
	if (filename) { // Start update
		int boot = opts & opt_B;
		int size = boot ? 4096 : 26624;
		uint8_t data[size];
		FILE *f = fopen(filename, "r");
		if (!f || (!(size = fread(data, 1, sizeof data, f)) && ferror(f))) err(1, "%s", filename);
		if (!size || size == (int)sizeof data || size & 3) errx(1, "%s: Invalid image", filename);
		fclose(f);
		if (boot) {
			printf("Updating bootloader...\n");
			sendval(fd, CMD_UPDATE);
			for (int pos = 0; pos < size; pos += 1024) {
				printf("%4d%%\r", pos * 100 / size);
				fflush(stdout);
				senddata(fd, data + pos, maxlen(pos, size));
				recvack(fd, "Error writing data");
			}
			recvack(fd, "Update failed"); // Wait for ACK after reboot
		} else {
			printf("Updating firmware...\n");
			for (int pos = 0; pos < size; pos += 1024) {
				printf("%4d%%\r", pos * 100 / size);
				fflush(stdout);
				sendval(fd, CMD_WRITE);
				sendval(fd, pos / 1024); // Block number
				senddata(fd, data + pos, maxlen(pos, size));
				recvack(fd, "Error writing data");
			}
		}
		printf("Done!\n");
	} else { // Print info
		printf("Fetching ESCape32 info...\n");
		uint8_t blinfo[32];
		sendval(fd, CMD_INFO);
		checkres(recvdata(fd, blinfo), 32, "Error reading data");
		printf("Bootloader revision %d\n", blinfo[0]);
		uint8_t fwinfo[20];
		sendval(fd, CMD_READ);
		sendval(fd, 0); // First block
		sendval(fd, 4); // (4+1)*4=20 bytes
		checkres(recvdata(fd, fwinfo), 20, "Error reading data");
		if (*(uint16_t *)fwinfo == 0x32ea) printf("Firmware revision %d [%s]\n", fwinfo[2], fwinfo + 4);
		else printf("Firmware not installed!\n");
	}
	close(fd);
	return 0;
}
