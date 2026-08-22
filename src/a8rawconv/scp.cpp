#include "stdafx.h"
#include "serial.h"

const char *scp_lookup_error(uint8_t c) {
	switch(c) {
		case 0x01:	return "bad command";
		case 0x02:	return "command error";
		case 0x03:	return "packet checksum error";
		case 0x04:	return "USB timeout";
		case 0x05:	return "track 0 not found";
		case 0x06:	return "no drive selected";
		case 0x07:	return "motor not enabled";
		case 0x08:	return "drive not ready";
		case 0x09:	return "no index pulse detected";
		case 0x0A:	return "zero revolutions chosen";
		case 0x0B:	return "read too long";
		case 0x0C:	return "invalid length";
		case 0x0E:	return "location boundary is odd";
		case 0x0F:	return "disk write protected";
		case 0x10:	return "RAM test failed";
		case 0x11:	return "no disk in drive";
		case 0x12:	return "bad baud rate selected";
		case 0x13:	return "bad command for selected port";
		default:
			return "unknown";
	}
}

void scp_init(const char *path) {
	serial_open(path);

	// attempt to resync the SCP
	printf("Initializing SCP device.\n");

	static const uint8_t infocmd[] = { 0x00, 0x00, 0x00, 0xD0, 0x00, 0x1A };

	serial_write(infocmd, sizeof infocmd);

	uint8_t resp[2];
	int state = 0;
	for(;;) {
		serial_read(resp, 1);

		if (state == 0) {
			if (resp[0] == 0xD0)
				state = 1;
		} else if (state == 1) {
			if (resp[0] == 0x4F)
				break;
			else if (resp[0] != 0xD0)
				state = 0;
		}
	}

	serial_read(resp, 2);
	printf("SCP version info: hardware version %u.%u, firmware version %u.%u\n", resp[0] >> 4, resp[0] & 15, resp[1] >> 4, resp[1] & 15);
}

void scp_shutdown() {
	serial_close();
}

void scp_report_fatal_error(const uint8_t buf[2]) {
	// try to turn off the motor and deselect the drive before we exit
	static const uint8_t kMotorAndSelectOff[] = {
		0x87, 0x00, 0x87+0x4A,
		0x86, 0x00, 0x86+0x4A,
		0x83, 0x00, 0x83+0x4A,
		0x82, 0x00, 0x82+0x4A
	};

	serial_write(kMotorAndSelectOff, sizeof kMotorAndSelectOff);
	scp_shutdown();

	fatalf("SCP command failed with status: cmd=%02X, status=%02X (%s)\n", buf[0], buf[1], scp_lookup_error(buf[1]));
}

bool scp_get_status(uint8_t buf[2]) {
	serial_read(buf, 2);

	return buf[1] == 0x4F;
}

bool scp_read_status() {
	uint8_t buf[2];

	if (!scp_get_status(buf))
		scp_report_fatal_error(buf);

	return true;
}

uint8_t scp_compute_checksum(const uint8_t *src, size_t len) {
	uint8_t chk = 0x4A;

	while(len--)
		chk += *src++;

	return chk;
}

bool scp_send_command(const void *buf, uint32_t len) {
	char buf2[32];

	if (len > 31)
		fatal("SCP command too long");

	// we have to write the entire command all in one go, or the SCP device
	// issues a USB timeout error

	memcpy(buf2, buf, len);
	buf2[len] = scp_compute_checksum((const uint8_t *)buf, len);
	serial_write(buf2, len+1);

	return scp_read_status();
}

bool scp_select_drive(bool driveB, bool select) {
	uint8_t selcmd[2];
	selcmd[0] = (select ? 0x80 : 0x82) + (driveB ? 1 : 0);
	selcmd[1] = 0;
	return scp_send_command(selcmd, 2);
}

bool scp_select_side(bool side2) {
	uint8_t selcmd[3];
	selcmd[0] = 0x8D;
	selcmd[1] = 0x01;
	selcmd[2] = side2 ? 1 : 0;

	return scp_send_command(selcmd, 3);
}

bool scp_select_density(bool hidensity) {
	uint8_t selcmd[3];
	selcmd[0] = 0x8C;
	selcmd[1] = 0x01;
	selcmd[2] = hidensity ? 1 : 0;

	return scp_send_command(selcmd, 3);
}

bool scp_motor(bool drive, bool enabled) {
	uint8_t cmd[2];
	cmd[0] = (enabled ? 0x84 : 0x86) + (drive ? 0x01 : 0x00);
	cmd[1] = 0;

	return scp_send_command(cmd, 2);
}

bool scp_seek0() {
	uint8_t cmd[2];
	cmd[0] = 0x88;
	cmd[1] = 0x00;

	return scp_send_command(cmd, 2);
}

bool scp_seek(int track) {
	uint8_t cmd[3];
	cmd[0] = 0x89;
	cmd[1] = 0x01;
	cmd[2] = track;

	return scp_send_command(cmd, 3);
}

bool scp_select_side(int side) {
	uint8_t cmd[3];
	cmd[0] = 0x8D;
	cmd[1] = 0x01;
	cmd[2] = (uint8_t)side;

	return scp_send_command(cmd, 3);
}

bool scp_mem_read(void *data, uint32_t offset, uint32_t len) {
	while(len > 0) {
		uint32_t tc = len > 0xF000 ? 0xF000 : len;

		uint8_t cmd[11];
		cmd[0] = 0xA9;
		cmd[1] = 0x08;
		cmd[2] = (uint8_t)(offset >> 24);
		cmd[3] = (uint8_t)(offset >> 16);
		cmd[4] = (uint8_t)(offset >>  8);
		cmd[5] = (uint8_t)(offset >>  0);
		cmd[6] = (uint8_t)(tc >> 24);
		cmd[7] = (uint8_t)(tc >> 16);
		cmd[8] = (uint8_t)(tc >>  8);
		cmd[9] = (uint8_t)(tc >>  0);
		cmd[10] = scp_compute_checksum(cmd, 10);
		serial_write(cmd, 11);
		serial_read(data, tc);

		bool r = scp_read_status();
		if (!r)
			return false;

		data = (char *)data + tc;
		len -= tc;
		offset += tc;
	}

	return true;
}

bool scp_mem_write(const void *data, uint32_t offset, uint32_t len) {
	while(len > 0) {
		uint32_t tc = len > 0xF000 ? 0xF000 : len;

		uint8_t cmd[11];
		cmd[0] = 0xAA;
		cmd[1] = 0x08;
		cmd[2] = (uint8_t)(offset >> 24);
		cmd[3] = (uint8_t)(offset >> 16);
		cmd[4] = (uint8_t)(offset >>  8);
		cmd[5] = (uint8_t)(offset >>  0);
		cmd[6] = (uint8_t)(tc >> 24);
		cmd[7] = (uint8_t)(tc >> 16);
		cmd[8] = (uint8_t)(tc >>  8);
		cmd[9] = (uint8_t)(tc >>  0);
		cmd[10] = 0x4A + cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4] + cmd[5] + cmd[6] + cmd[7] + cmd[8] + cmd[9];
		serial_write(cmd, 11);
		serial_write(data, tc);

		bool r = scp_read_status();
		if (!r)
			return false;

		data = (const char *)data + tc;
		len -= tc;
		offset += tc;
	}

	return true;
}

bool scp_erase(bool rpm360) {
	uint8_t tmpbuf[204];
	memset(tmpbuf, 0, sizeof tmpbuf);
	scp_mem_write(tmpbuf, 0, 204);

	uint8_t cmd[8];
	cmd[0] = 0xA2;
	cmd[1] = 0x05;
	cmd[2] = 0;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 102;
	cmd[6] = rpm360 ? 0x0D : 0x05;
	cmd[7] = 0x4A + cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4] + cmd[5] + cmd[6];
	serial_write(cmd, 8);
	return scp_read_status();
}

bool scp_track_read(bool rpm360, uint8_t revs, bool prefer_8bit, bool splice) {
	uint8_t cmd[5];
	cmd[0] = 0xA0;
	cmd[1] = 0x02;
	cmd[2] = revs;

	uint8_t result[2];

	for(int i=0; i<2; ++i) {
		cmd[3] = (rpm360 ? 0x08 : 0x00) + (prefer_8bit ? 0x02 : 0x00) + (splice ? 0x00 : 0x01);
		cmd[4] = scp_compute_checksum(cmd, 4);
		serial_write(cmd, 5);

		if (scp_get_status(result)) {
			return prefer_8bit;
		}

		// check for read too long error
		if (result[1] != 0x0B)
			scp_report_fatal_error(result);

		// retry in other format
		prefer_8bit = !prefer_8bit;
	}

	// neither 8-bit nor 16-bit worked
	scp_report_fatal_error(result);
	return false;
}

bool scp_track_getreadinfo(uint32_t data[10]) {
	uint8_t buf[40];
	buf[0] = 0xA1;
	buf[1] = 0x00;
	buf[2] = 0xEB;
	serial_write(buf, 3);
	bool status = scp_read_status();
	serial_read(buf, 40);

	for(int i=0; i<10; ++i) {
		data[i] = ((uint32_t)buf[i*4+0] << 24)
			+ ((uint32_t)buf[i*4+1] << 16)
			+ ((uint32_t)buf[i*4+2] <<  8)
			+ ((uint32_t)buf[i*4+3] <<  0);
	}

	return status;
}

bool scp_track_write(bool rpm360, uint32_t bitCellCount, bool splice, bool erase) {
	uint8_t cmd[7];
	cmd[0] = 0xA2;
	cmd[1] = 0x05;
	cmd[2] = (uint8_t)(bitCellCount >> 24);
	cmd[3] = (uint8_t)(bitCellCount >> 16);
	cmd[4] = (uint8_t)(bitCellCount >>  8);
	cmd[5] = (uint8_t)(bitCellCount >>  0);
	cmd[6] = (rpm360 ? 0x08 : 0x00) + (splice ? 0x00 : 0x01) + (erase ? 0x04 : 0x00);
	return scp_send_command(cmd, 7);
}
