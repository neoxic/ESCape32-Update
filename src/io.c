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
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <termios.h>
#include "common.h"

int openserial(const char *path) {
	int fd = open(path, O_RDWR);
	if (fd == -1) err(1, "%s", path);
	struct termios ts;
	if (tcgetattr(fd, &ts)) err(1, "tcgetattr('%s')", path);
	cfmakeraw(&ts);
	if (cfsetspeed(&ts, B38400)) err(1, "cfsetspeed('%s')", path);
	ts.c_cc[VMIN] = 0;
	ts.c_cc[VTIME] = 3; // 300ms read timeout
	if (tcsetattr(fd, TCSANOW, &ts)) err(1, "tcsetattr('%s')", path);
	if (tcflush(fd, TCIOFLUSH)) err(1, "tcflush('%s')", path);
	return fd;
}

int recv(int fd, uint8_t *buf, int len) {
	while (len) {
		int res = read(fd, buf, len);
		if (res == -1) err(1, "read(%d, %d)", fd, len);
		if (!res) return 0; // Timeout
		buf += res;
		len -= res;
	}
	return 1;
}

void send(int fd, const uint8_t *buf, int len) {
	while (len) {
		int res = write(fd, buf, len);
		if (res == -1) err(1, "write(%d, %d)", fd, len);
		buf += res;
		len -= res;
	}
	tcdrain(fd); // Wait for pending output to be transmitted
}

int recvval(int fd) {
	uint8_t buf[2];
	return recv(fd, buf, 2) && (buf[0] ^ buf[1]) == 0xff ? buf[0] : -1;
}

void sendval(int fd, int val) {
	uint8_t buf[2] = {val, ~val};
	send(fd, buf, 2);
}

int recvdata(int fd, uint8_t *buf) {
	int cnt = recvval(fd);
	if (cnt == -1) return -1;
	int len = (cnt + 1) << 2;
	uint32_t crc;
	return recv(fd, buf, len) && recv(fd, (uint8_t *)&crc, 4) && crc32(buf, len) == crc ? len : -1;
}

void senddata(int fd, const uint8_t *buf, int len) {
	uint32_t crc = crc32(buf, len);
	sendval(fd, (len >> 2) - 1);
	send(fd, buf, len);
	send(fd, (uint8_t *)&crc, 4);
}
