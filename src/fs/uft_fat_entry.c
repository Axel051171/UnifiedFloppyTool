/*
 * entry.c
 * Copyright (C) 2016 <sgerwk@aol.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>
#include "entry.h"
#include "table.h"

int fatentrydebug = 0;
#define dprintf if (fatentrydebug) printf

#define ENTRYPOS(directory, index, pos)			\
	(fatunitgetdata((directory))[(index) * 32 + (pos)])

/*
 * check if an entry exists
 */
int fatentryexists(unit *directory, int index) {
	unsigned char c;
	c = ENTRYPOS(directory, index, 0);
	return c != 0x00 && c != 0xE5;
}

/*
 * check if this is the last entry of a directory
 */
int fatentryend(unit *directory, int index) {
	return directory == NULL || ENTRYPOS(directory, index, 0) == 0x00;
}

/*
 * check if this is a part of a long name entry
 */
int fatentryislongpart(unit *directory, int index) {
	return (fatentrygetattributes(directory, index) & FAT_ATTR_ALL) ==
		FAT_ATTR_LONGNAME;
}

/*
 * convert the name between the 11-byte array to a max-13-char string
 */

void fatshortnametostring(char dst[13], unsigned char src[11]) {
	char *scan, *dot;

	memset(dst, '\0', 13);

	memcpy(dst, src, 8);

	dot = dst + 8;
	for (scan = dst + 7; scan >= dst && *scan == ' '; scan--)
		dot--;

	if (src[8] == ' ')
		*dot = '\0';
	else {
		*dot++ = '.';
		memcpy(dot, src + 8, 3);
		dot[3] = '\0';
		for (scan = dot + 2; scan >= dot && *scan == ' '; scan--)
			*scan = '\0';
	}
}

int fatstringtoshortname(unsigned char dst[11], char src[13]) {
	char *pos, *dot;

	memset(dst, ' ', 11);

	dot = strchr(src, '.');

	pos = dot == NULL ? src + strlen(src) : dot;
	if (dot == src) {
		memcpy(dst, src, strlen(src) > 11 ? 11 : strlen(src));
		return 0;
	}
	if (pos - src > 8) {
		printf("invalid shortname: %s\n", src);
		return -1;
	}
	memcpy(dst, src, pos - src);
	if (dot == NULL)
		return 0;

	pos++;
	if (strlen(pos) > 3) {
		printf("invalid shortname: %s\n", src);
		return -1;
	}
	dprintf("%zu\n", strlen(pos));
	memcpy(dst + 8, pos, strlen(pos));

	return 0;
}

/*
 * short name as a string
 */

void fatentrygetshortname(unit *directory, int index, char shortname[13]) {
	fatshortnametostring(shortname, & ENTRYPOS(directory, index, 0));
}

int fatentrysetshortname(unit *directory, int index, char shortname[13]) {
	int res;
	res = fatstringtoshortname(& ENTRYPOS(directory, index, 0),
			shortname);
	if (! res)
		directory->dirty = 1;
	return res;
}

void fatentryprintshortname(unit *directory, int index) {
	char shortname[13];
	int i;

	fatshortnametostring(shortname, & ENTRYPOS(directory, index, 0));

	for (i = 0; i < 12; i++)
		if (shortname[i] == 0)
			putchar(' ');
		else if (shortname[i] < 32)
			putchar('?');
		else
			putchar(shortname[i]);
}

int fatentrycompareshortname(unit *directory, int index, char shortname[13]) {
	char name[13];
	fatentrygetshortname(directory, index, name);
	return strcmp(shortname, name);
}

int fatentryisdotfile(unit *directory, int index) {
	return	! fatentrycompareshortname(directory, index, DOTFILE) ||
		! fatentrycompareshortname(directory, index, DOTDOTFILE);
}

void fatentryfirst(unit *directory, int index, char first) {
	ENTRYPOS(directory, index, 0) = first;
	directory->dirty = 1;
}

void fatentrydelete(unit *directory, int index) {
	fatentryfirst(directory, index, 0xE5);
}

void fatentryzero(unit *directory, int index) {
	memset(& ENTRYPOS(directory, index, 0), 0, 32);
	directory->dirty = 1;
}

/*
 * get time/date
 */

void _tminit(struct tm *tm) {
	bzero(tm, sizeof(struct tm));
}

void _fatdatetotm(uint16_t d, struct tm *tm) {
	tm->tm_mday = d & 0x1F;
	tm->tm_mon = ((d >> 5) & 0x0F) - 1;
	tm->tm_year = 80 + (d >> 9);
	tm->tm_wday = -1;
}

void _fattimetotm(uint16_t d, struct tm *tm) {
	tm->tm_sec = 2 * (d & 0x1F);
	tm->tm_min = (d >> 5) & 0x3F;
	tm->tm_hour = d >> 11;
}

void _fattmfixday(struct tm *tm) {
	char *tz;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	tzset();

	mktime(tm);

	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	tzset();
}

int _fatentrygettime(unit *directory, int index, int time_pos, int date_pos,
		struct tm *tm) {
	_tminit(tm);
	if (time_pos >= 0)
		_fattimetotm(
			le16toh(_unit16uint(directory, 32 * index + time_pos)),
				tm);
	if (date_pos >= 0)
		_fatdatetotm(
			le16toh(_unit16uint(directory, 32 * index + date_pos)),
				tm);
	_fattmfixday(tm);
	return 0;
}

int fatentrygetwritetime(unit *directory, int index, struct tm *tm) {
	return _fatentrygettime(directory, index, 22, 24, tm);
}

int fatentrygetcreatetime(unit *directory, int index, struct tm *tm) {
	return _fatentrygettime(directory, index, 14, 16, tm);
}

int fatentrygetreadtime(unit *directory, int index, struct tm *tm) {
	return _fatentrygettime(directory, index, -1, 18, tm);
}

/*
 * set time/date
 */

void _fattmtodate(const struct tm *tm, uint16_t *d) {
	*d = 0;
	*d |= tm->tm_mday;
	*d |= (tm->tm_mon + 1) << 5;
	*d |= (tm->tm_year - 80) << 9;
}

void _fattmtotime(const struct tm *tm, uint16_t *d) {
	*d = 0;
	*d |= tm->tm_sec >> 1;
	*d |= tm->tm_min << 5;
	*d |= tm->tm_hour << 11;
}

int _fatentrysettime(unit *directory, int index, int time_pos, int date_pos,
		struct tm *tm) {
	uint16_t d, t;
	if (time_pos >= 0) {
		_fattmtotime(tm, &t);
		_unit16uint(directory, 32 * index + time_pos) = htole16(t);
	}
	if (date_pos >= 0) {
		_fattmtodate(tm, &d);
		_unit16uint(directory, 32 * index + date_pos) = htole16(d);
	}
	directory->dirty = 1;
	return 0;
}

int fatentrysetwritetime(unit *directory, int index, struct tm *tm) {
	return _fatentrysettime(directory, index, 22, 24, tm);
}

int fatentrysetcreatetime(unit *directory, int index, struct tm *tm) {
	return _fatentrysettime(directory, index, 14, 16, tm);
}

int fatentrysetreadtime(unit *directory, int index, struct tm *tm) {
	return _fatentrysettime(directory, index, -1, 18, tm);
}

/*
 * set now as file time
 */

void _fattmnow(struct tm *tm) {
	time_t t;
	t = time(NULL);
	localtime_r(&t, tm);
}

int fatentrysetwritetimenow(unit *directory, int index) {
	struct tm tm;
	_fattmnow(&tm);
	return _fatentrysettime(directory, index, 22, 24, &tm);
}

int fatentrysetcreatetimenow(unit *directory, int index) {
	struct tm tm;
	_fattmnow(&tm);
	return _fatentrysettime(directory, index, 14, 16, &tm);
}

int fatentrysetreadtimenow(unit *directory, int index) {
	struct tm tm;
	_fattmnow(&tm);
	return _fatentrysettime(directory, index, -1, 18, &tm);
}

/*
 * first cluster of a file
 */

int32_t fatentrygetfirstcluster(unit *directory, int index, int bits) {
	int32_t n;

	n = le16toh(_unit16uint(directory, index * 32 + 26));
	if (bits == 32)
		n |= le16toh(_unit16int(directory, index * 32 + 20)) << 16;

	return n;
}

int fatentrysetfirstcluster(unit *directory, int index, int bits, int32_t n) {
	int pos;

	if (index < 0 || index * 32 > directory->size)
		return -1;

	if (bits == 12)
		n = n & 0xFFF;

	if (n == FAT_ROOT)
		n = 0;

	_unit16uint(directory, index * 32 + 26) = htole16(n & 0xFFFF);
	if (bits == 32) {
		pos = index * 32 + 20;
		_unit16int(directory, pos) = htole16((n >> 16) & 0xFFFF);
	}

	directory->dirty = 1;

	return 0;
}

/*
 * file attributes
 */
unsigned char fatentrygetattributes(unit *directory, int index) {
	return _unit8int(directory, index * 32 + 11);
}

void fatentrysetattributes(unit *directory, int index, unsigned char attr) {
	_unit8int(directory, index * 32 + 11) = attr;
	directory->dirty = 1;
}

int fatentryisdirectory(unit *directory, int index) {
	return fatentrygetattributes(directory, index) & FAT_ATTR_DIR;
}

/*
 * file size
 */
uint32_t fatentrygetsize(unit *directory, int index) {
	return le32toh(_unit32uint(directory, index * 32 + 0x1C));
}

void fatentrysetsize(unit *directory, int index, uint32_t size) {
	_unit32uint(directory, index * 32 + 0x1C) = htole32(size);
	directory->dirty = 1;
}

/*
 * print the entry position: cluster number and index
 */
void fatentryprintpos(unit *directory, int index, int minsize) {
	char buf[1024];
	snprintf(buf, 1024, "%d,%d", directory->n, index);
	printf("%-*s", minsize, buf);
}

/*
 * simplified printing of a ucs2 string
 */
void _fatprintucs2(uint16_t *c, int len) {
	int i;

	for (i = 0; i < len; i ++)
		if (c[i] != 0xFFFF && c[i] != 0x0000)
			printf("%c", c[i]);
		else
			printf(" ");
}

/*
 * print a summary of the file entry
 */
void fatentryprint(unit *directory, int index) {
	if (fatentryislongpart(directory, index)) {
		printf(_unit8int(directory, index * 32) & 0x40 ? "s": " ");
		printf(" %02X ", _unit8int(directory, index * 32) & 0x3F);
		_fatprintucs2((uint16_t *) &ENTRYPOS(directory, index, 1), 5);
		_fatprintucs2((uint16_t *) &ENTRYPOS(directory, index, 14), 6);
		_fatprintucs2((uint16_t *) &ENTRYPOS(directory, index, 28), 2);
		printf("      %02X", _unit8uint(directory, index * 32 + 13));
		return;
	}
	fatentryprintshortname(directory, index);
	printf(" 0x%02X", fatentrygetattributes(directory, index));
	printf(" %'8u", fatentrygetsize(directory, index));
}

