/**
 * @file uft_fat_table.c
 * @brief FAT table handling
 * @version 3.8.0
 */
/*
 * table.c
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

/*
 * table.c
 *
 * the file allocation table
 */

#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define lseek _lseek
#define read _read
#define write _write
#define close _close
#define open _open
/* Windows doesn't have endian.h - define le16toh/le32toh */
#define le16toh(x) (x)
#define le32toh(x) (x)
#define htole16(x) (x)
#define htole32(x) (x)
#else
#include <unistd.h>
#include <search.h>
#include <endian.h>
#endif

#include "table.h"

int fattabledebug = 0;
#define dprintf if (fattabledebug) printf

int fattableerror = 1;
#define eprintf if (fattableerror) printf

/*
 * read a whole FAT in cache; read all FATs if nfat == FAT_ALL
 */
int fatreadfat(fat *f, int nfat) {
	int32_t start, end, sector;

	if (nfat < FAT_ALL || nfat >= fatgetnumfats(f))
		return -1;

	start = fatgetreservedsectors(f);
	start += nfat == FAT_ALL ? 0 : nfat * fatgetfatsize(f);

	end = start;
	end += fatgetfatsize(f) * ((nfat == FAT_ALL) ? fatgetnumfats(f) : 1);

	dprintf(nfat == FAT_ALL ?
			"reading %sFATs%.0d in cache:" :
			"reading %sFAT%d in cache:",
		nfat == FAT_ALL ? "all " : "",
		nfat == FAT_ALL ? 0 : nfat);

	for (sector = start; sector < end; sector++)  {
		dprintf(" %d", sector);
		if (NULL == fatunitget(&f->sectors, f->offset,
				fatgetbytespersector(f), sector, f->fd)) {
			dprintf(" error reading sector %d\n", sector);
			return -1;
		}
	}
	dprintf("\n");

	return 0;
}

/*
 * the entry for a cluster in a fat: sector and position within
 */

unit *_fatclusterpos(fat *f, int nfat, int32_t n, int *pcluster) {
	int fatbegin, nsector;
	unit *ret;

	fatbegin = fatgetreservedsectors(f) + nfat * fatgetfatsize(f);

	*pcluster = n * (fatbits(f) / 4) / 2;
	nsector = *pcluster / fatgetbytespersector(f);
	*pcluster -= nsector * fatgetbytespersector(f);

	ret = fatunitget(&f->sectors, f->offset, fatgetbytespersector(f),
		fatbegin + nsector, f->fd);
	if (ret == NULL) {
		printf("while determining next of cluster %d: ", n);
		printf("cannot read sector %d ", fatbegin + nsector);
		printf("(sector %d of FAT%d)\n",  nsector, nfat);
		// exit(1);
	}
	return ret;
}

/*
 * on fat12, two fat entries take three bytes; for example, if the first two
 * clusters have sucessors 2->ABC and 3->123, the beginning of the table is:
 *
 *    F8 FF FF   BC 3A 12
 * next of 2:    **  *
 * next of 3:       *  **
 *
 * each entry spans two bytes; the formulae take these two bytes in reverse
 * order, since this way the entries are easier to separate: BC 3A 12 becomes
 * 12 3A BC, which is 3A BC for the first entry and 12 3A for the second
 *
 * the second byte for an entry may fall in the next fat sector; for this
 * reason, the two bytes have to be read individually, rather than as a single
 * 16-bit integer
 */

/*
 * next byte for a fat12 entry: it may be in the next sector
 * nfat is not needed because the sector to possibly load is just fs->n + 1
 */

unit *_fatclusterposnext(fat *f, unit *fs, int low, int *high) {
	if (low != fatgetbytespersector(f) - 1) {
		*high = low + 1;
		return fs;
	}

	*high = 0;
	return fatunitget(&f->sectors, f->offset, fatgetbytespersector(f),
		fs->n + 1, f->fd);
}

/*
 * access the fat entry corresponding to a cluster (see note in table.h)
 */

int32_t fatgetfat(fat *f, int nfat, int32_t n) {
	int pcluster, phigh;
	unit *fs, *fshigh;
	uint32_t content;

	if (fatbits(f) == -1)
		return FAT_ERR;

	if (nfat < 0 || nfat > fatgetnumfats(f)) {
		printf("not an existing fat: %d\n", nfat);
		exit(1);
	}

	fs = _fatclusterpos(f, nfat, n, &pcluster);
	if (fs == NULL)
		return FAT_ERR;

	switch (fatbits(f)) {
	case 12:
		/* see below for an explanation */
		fshigh = _fatclusterposnext(f, fs, pcluster, &phigh);
		if (fshigh == NULL)
			return FAT_ERR;
		content = _unit8uint(fshigh, phigh) << 8;
		content |= _unit8uint(fs, pcluster);
		content = content >> ((n & 1) << 2);
		content = content & 0x0FFF;
		return content;
	case 16:
		content = le16toh(_unit16int(fs, pcluster)) & 0xFFFF;
		return content;
	case 32:
		content = le32toh(_unit32int(fs, pcluster)) & 0x0FFFFFFF;
		return content;
	}

	return FAT_ERR;
}

int fatsetfat(fat *f, int nfat, int32_t n, int32_t next) {
	int pcluster, phigh;
	unit *fs, *fshigh;

	if (fatbits(f) == -1)
		return -1;

	if (nfat < 0 || nfat > fatgetnumfats(f)) {
		printf("not an existing fat: %d\n", nfat);
		exit(1);
	}

	fs = _fatclusterpos(f, nfat, n, &pcluster);
	if (fs == NULL)
		return -1;

	switch (fatbits(f)) {
	case 12:
		/* see below for an explanation */
		fshigh = _fatclusterposnext(f, fs, pcluster, &phigh);
		next = next << 4;
		next |= _unit8uint(fs, pcluster) & 0x0F;
		next |= (_unit8uint(fshigh, phigh) & 0xF0) << 12;
		next = next >> ((~n & 1) << 2);
		_unit8uint(fs, pcluster) = next & 0xFF;
		_unit8uint(fshigh, phigh) = (next >> 8) & 0xFF;
		break;
	case 16:
		_unit16int(fs, pcluster) = htole16(next);
		break;
	case 32:
		_unit32int(fs, pcluster) = htole32(next);
		break;
	}

	fs->dirty = 1;

	return 0;
}

/*
 * fix the first two entries in a fat (the table "header")
 */
int fatfixtableheader(fat *f, int nfat) {
	switch (fatbits(f)) {
	case 12:
		if (fatsetfat(f, nfat, 0, fatgetmedia(f) | 0x0F00))
			return -1;
		return fatsetfat(f, nfat, 1, 0x0FF8);
	case 16:
		if (fatsetfat(f, nfat, 0, fatgetmedia(f) | 0xFF00))
			return -1;
		return fatsetfat(f, nfat, 1, 0xFFF8);
	case 32:
		if (fatsetfat(f, nfat, 0, fatgetmedia(f) | 0x0FFFFF00))
			return -1;
		return fatsetfat(f, nfat, 1, 0x0FFFFFF8);
	}

	return -1;
}

/*
 * dirty bits in FAT (alternative location, main is the boot sector)
 */

int _fatdirtybitsshift(fat *f) {
	if (fatbits(f) == 12)
		return -1;
	else if (fatbits(f) == 16)
		return 14;
	else if (fatbits(f) == 32)
		return 26;
	else
		return -1;
}

int _swapdirtybits(int x) {
	return ((x & 0x02) >> 1) | ((x & 0x01) << 1);
}

int fatgetfatdirtybits(fat *f, int nfat) {
	int32_t entry;
	int shift;

	shift = _fatdirtybitsshift(f);
	if (shift == -1)
		return -1;

	entry = fatgetfat(f, nfat, 1);

	return _swapdirtybits(~ (entry >> shift));
}

int fatsetfatdirtybits(fat *f, int nfat, uint32_t dirty) {
	uint32_t entry, mask;
	int shift;

	shift = _fatdirtybitsshift(f);
	if (shift == -1)
		return -1;

	dirty = _swapdirtybits(dirty) << shift;
	mask =  (FAT_UNCLEAN | FAT_IOERROR) << shift;

	entry = ~ fatgetfat(f, nfat, 1);
	entry &= ~ mask;
	entry |= dirty;
	return fatsetfat(f, nfat, 1, ~ entry);
}

/*
 * tell whether a fat entry is UNUSED, EOF or BAD
 */

int fatisfatunused(fat *f, int32_t n) {
	(void) f;
	return n == 0;
}

int fatisfateof(fat *f, int32_t n) {
	uint32_t entry = (uint32_t) n;
	switch (fatbits(f)) {
	case 12:
		return (entry & 0x0FF8) == 0x0FF8;
	case 16:
		return (entry & 0xFFF8) == 0xFFF8;
	case 32:
		return (entry & 0x0FFFFFF8) == 0x0FFFFFF8;
	default:
		return 0;
	}
}

int fatisfatbad(fat *f, int32_t n) {
	uint32_t entry = (uint32_t) n;
	if (fatisfateof(f, n))
		return 0;
	switch (fatbits(f)) {
	case 12:
		return (entry & 0x0FF7) == 0x0FF7;
	case 16:
		return (entry & 0xFFF7) == 0xFFF7;
	case 32:
		return (entry & 0x0FFFFFF7) == 0x0FFFFFF7;
	default:
		return 0;
	}
}

/*
 * print a cluster entry
 */
void fatprintfat(fat *f, int32_t n) {
	if (fatisfatunused(f, n))
		printf("UNUSED");
	else if (fatisfatbad(f, n))
		printf("BAD");
	else if (fatisfateof(f, n))
		printf("EOF");
	else if (n > fatlastcluster(f))
		printf("OUT");
	else
		printf("%d", n);
}

/*
 * get the next of a cluster, according to a fat
 */

int32_t fatgetnextcluster(fat *f, int32_t n) {
	int32_t next;
	int ifat;

	if (n == FAT_ROOT)
		return FAT_EOF;

	if (n < FAT_ROOT) {
		printf("\nerror: next of non-cluster %d\n", n);
		return FAT_ERR;
	}

	if (n > fatlastcluster(f)) {
		printf("\nerror: cluster %d does not exists\n", n);
		printf("last cluster in the filesystem is ");
		printf("%d\n", fatlastcluster(f));
		return FAT_ERR;	
	}

	if (f->nfat != FAT_ALL)
		next = fatgetfat(f, f->nfat, n);
	else
		for (ifat = 0; ifat < fatgetnumfats(f); ifat++)
			if ((next = fatgetfat(f, ifat, n)) != FAT_ERR)
				break;
	if (next == FAT_ERR)
		return FAT_ERR;

	if (fatisfatbad(f, next))
		return FAT_BAD;
	else if (fatisfateof(f, next))
		return FAT_EOF;
	else if (next > fatlastcluster(f)) {
		eprintf("\nerror: next of cluster %d is %u, ", n, next);
		eprintf("does not exist\n");
		eprintf("last cluster in the filesystem is ");
		eprintf("%d\n", fatlastcluster(f));
		return FAT_ERR;
	}

	return next;
}

/*
 * set the next of a cluster in a fat (FAT_ALL = all fats)
 */

int fatsetnextcluster(fat *f, int32_t n, int32_t next) {
	int res;

	if (n > fatlastcluster(f)) {
		printf("\nerror: cluster %d does not exists\n", n);
		printf("last cluster in the filesystem is ");
		printf("%d\n", fatlastcluster(f));
		return FAT_ERR;	
	}

	if (n == FAT_ROOT)
		return -1;

	if (f->free != -1 && f->nfat == FAT_ALL) {
		if (fatgetnextcluster(f, n) != FAT_UNUSED &&
				next == FAT_UNUSED)
			f->free++;
		if (fatgetnextcluster(f, n) == FAT_UNUSED &&
				next != FAT_UNUSED)
			f->free--;
	}

	if (f->nfat == FAT_ALL) {
		res = 0;
		for (f->nfat = 0; f->nfat < fatgetnumfats(f); f->nfat++)
			if (fatsetnextcluster(f, n, next))
				res--;
		f->nfat = FAT_ALL;
		return res;
	}

	if (next == FAT_EOF)
		next = 0x0FFFFFF8;
	else if (next == FAT_BAD)
		next = 0x0FFFFFF7;

	switch (fatbits(f)) {
	case 12:
		return fatsetfat(f, f->nfat, n, next & 0x0FFF);
	case 16:
		return fatsetfat(f, f->nfat, n, next & 0xFFFF);
	case 32:
		return fatsetfat(f, f->nfat, n, next & 0x0FFFFFFF);
	}

	return -1;
}

/*
 * initialize a file allocation table
 */
int fatinittable(fat *f, int nfat) {
	int prevfat;
	int32_t cl, r, pilot;
	int32_t sector, start;
	unit *table;

	if (nfat < 0 || nfat >= fatgetnumfats(f))
		return -1;

	fatfixtableheader(f, nfat);

	prevfat = f->nfat;
	f->nfat = nfat;

	pilot = 2 * fatgetbytespersector(f);
	if (fatlastcluster(f) <= pilot)
		for (cl = FAT_FIRST; cl <= fatlastcluster(f); cl++)
			fatsetnextcluster(f, cl, FAT_UNUSED);
	else {
		for (cl = FAT_FIRST; cl < pilot; cl++)
			fatsetnextcluster(f, cl, FAT_UNUSED);

		start = fatgetreservedsectors(f) + fatgetfatsize(f) * nfat;
		table = fatunitget(&f->sectors, f->offset,
				fatgetbytespersector(f), start + 1, f->fd);
		fatunitgetdata(table);
		fatunitwriteback(table);
		for (sector = start + 1;
		     sector < start + fatgetfatsize(f) - 1;
		     sector++) {
			fatunitmove(&f->sectors, table, sector + 1);
			fatunitwriteback(table);
		}
		fatunitdelete(&f->sectors, table->n);
	}

	r = fatgetrootbegin(f);
	if (r >= FAT_FIRST)
		fatsetnextcluster(f, r, FAT_EOF);

	f->nfat = prevfat;

	f->last = FAT_FIRST;
	f->free = fatnumdataclusters(f) - 1;

	return 0;
}

/*
 * cluster intervals
 */

int fatclusterisbetween(int32_t cluster, int32_t begin, int32_t end) {
	if (begin < end && (cluster < begin || cluster > end))
		return 0;
	if (begin > end && cluster < begin && cluster > end)
		return 0;
	return 1;
}

void fatclusterisregular(fat *f, int32_t cluster) {
	if (fatclusterisbetween(cluster, FAT_FIRST, fatlastcluster(f)))
		return;
	printf("cluster %d not a regular cluster: not between %d and %d\n",
		cluster, FAT_FIRST, fatlastcluster(f));
	exit(1);
}

int32_t fatclusterintervalprev(fat *f, int32_t c, int32_t begin, int32_t end) {
	c--;
	if (c == begin - 1)
		c = end;
	if (c == FAT_FIRST - 1)
		c = fatlastcluster(f);
	return c;
}

int32_t fatclusterintervalnext(fat *f, int32_t c, int32_t begin, int32_t end) {
	c++;
	if (c == end + 1)
		c = begin;
	if (c == fatlastcluster(f) + 1)
		c = FAT_FIRST;
	return c;
}

/*
 * number of free clusters (wrap if end < begin)
 */

int32_t fatclusternumfreebetween(fat *f, int32_t begin, int32_t end) {
	int32_t c, num;

	dprintf("counting free clusters between %d and %d\n", begin, end);

	fatclusterisregular(f, begin);
	fatclusterisregular(f, end);

	dprintf("actual count is between %d and %d:", begin, end);

	num = 0;
	c = begin;
	do {
		dprintf(" %d", c);
		if (fatgetnextcluster(f, c) == FAT_UNUSED) {
			dprintf("*");
			num++;
		}

		c = fatclusterintervalnext(f, c, begin, end);
	} while (c != begin);

	dprintf("\n");
	return num;
}

int32_t fatclusternumfree(fat *f) {
	f->free = fatclusternumfreebetween(f, FAT_FIRST, fatlastcluster(f));
	return f->free;
}

/*
 * find free clusters (wrap if end < begin)
 */

int32_t fatclusterfindfreesequencebetween(fat *f,
		int32_t begin, int32_t end, int32_t start, int length) {
	int32_t cl, first;
	int count;

	if (length <= 0)
		return FAT_ERR;

	dprintf("searching for a free cluster between %d and %d, "
		"starting at %d\n", begin, end, start);

	if (start == -1)
		start = f->last != -1 ? f->last : FAT_FIRST;
	
	fatclusterisregular(f, begin);
	fatclusterisregular(f, end);
	fatclusterisregular(f, start);
	if (! fatclusterisbetween(start, begin, end))
		start = begin;

	dprintf("actual search: %d - %d, start %d:", begin, end, start);

	count = 0;
	cl = start;
	do {
		dprintf(" %d", cl);

		if (fatgetnextcluster(f, cl) != FAT_UNUSED)
			count = 0;
		else {
			dprintf(" <-- found: %d (%d)\n", cl, count + 1);
			if (count == 0)
				first = cl;
			count++;
			if (cl < first)
				count = 0;
			f->last = cl;
			if (count == length)
				return first;
		}

		cl = fatclusterintervalnext(f, cl, begin, end);
	} while (cl != start);

	dprintf(" not found\n");
	return FAT_ERR;
}

int32_t fatclusterfindfreebetween(fat *f,
		int32_t begin, int32_t end, int32_t start) {
	return fatclusterfindfreesequencebetween(f, begin, end, start, 1);
}

int32_t fatclusterfindfreesequence(fat *f, int length) {
	return fatclusterfindfreesequencebetween(f,
		FAT_FIRST, fatlastcluster(f), -1, length);
}

int32_t fatclusterfindfree(fat *f) {
	return fatclusterfindfreebetween(f,
		FAT_FIRST, fatlastcluster(f), -1);
}

/*
 * presence and count of bad clusters in an area
 */

int _fatclusterareabad(fat *f, int32_t begin, int32_t end, int st) {
	int32_t cl;
	int count = 0;

	cl = begin;
	do {
		if (fatgetnextcluster(f, cl) == FAT_BAD) {
			if (st)
				return -1;
			count++;
		}

		cl = fatclusterintervalnext(f, cl, begin, end);
	} while (cl != begin);

	return count;
}

int fatclusterareaisbad(fat *f, int32_t begin, int32_t end) {
	return _fatclusterareabad(f, begin, end, 1);
}

int fatclusternumbadbetween(fat *f, int32_t begin, int32_t end) {
	return _fatclusterareabad(f, begin, end, 0);
}

/*
 * find the "most free" area of a given size
 */
int32_t fatclustermostfree(fat *f, int size, int allowbad,
		int *maxfree) {
	int32_t start, max;
	int32_t prev, next;
	int numfree;
	int numbad;

	if (size > fatnumdataclusters(f))
		return FAT_ERR;

	start = FAT_FIRST;
	if (start + size > fatlastcluster(f))
		return FAT_ERR;

	*maxfree = 0;
	max = FAT_ERR;

	numfree = fatclusternumfreebetween(f, start, start + size - 1);
	numbad = fatclusternumbadbetween(f, start, start + size - 1);

	if (allowbad || numbad == 0) {
		*maxfree = numfree;
		max = start;
		if (*maxfree == size)
			return start;
	}

	dprintf("start: %d free: %d\n", start, numfree);

	for (start++; start + size - 1 <= fatlastcluster(f); start++) {
		// sliding window: one out, one in

		prev = fatgetnextcluster(f, start - 1);
		if (prev == FAT_UNUSED)
			numfree--;
		if (prev == FAT_BAD)
			numbad--;

		next = fatgetnextcluster(f, start + size - 1);
		if (next == FAT_UNUSED)
			numfree++;
		if (next == FAT_BAD)
			numbad++;

		dprintf("start: %d free: %d\n", start, numfree);

		if (numfree > *maxfree && (allowbad || numbad == 0)) {
			*maxfree = numfree;
			max = start;
			if (*maxfree == size)
				break;
		}
	}

	return max;
}

/*
 * find the first longest linear subchain of a chain
 */
int32_t fatclusterlongestlinear(fat *f, int32_t start,
		int *maxlen, int *maxindex) {
	int32_t maxstart, next;
	int len, index;

	len = 1;
	*maxlen = 1;

	maxstart = start;

	index = 1;
	*maxindex = index;

	for (next = fatgetnextcluster(f, start);
	     next != FAT_EOF && next != FAT_UNUSED;
	     next = fatgetnextcluster(f, start)) {
		if (next != start + 1)
			len = 1;
		else {
			len++;
			if (len > *maxlen) {
				*maxlen = len;
				maxstart = next - len + 1;
				*maxindex = index - len + 1;
			}
		}
		start = next;
		index++;
	}

	return maxstart;
}

/*
 * find an allocated cluster (wrap if end < begin)
 * can be used to check if an area is all free
 */
int fatclusterfindallocatedbetween(fat *f,
		int32_t begin, int32_t end, int32_t start) {
	int32_t c;

	dprintf("searching for a used clusters between %d and %d\n",
		begin, end);

	fatclusterisregular(f, begin);
	fatclusterisregular(f, end);
	fatclusterisregular(f, start);
	if (! fatclusterisbetween(start, begin, end))
		start = begin;

	dprintf("actual search is between %d and %d:", begin, end);

	c = start;
	do {
		dprintf(" %d", c);

		if (fatgetnextcluster(f, c) != FAT_UNUSED &&
				fatgetnextcluster(f, c) != FAT_BAD) {
			dprintf("\n");
			return c;
		}

		c = fatclusterintervalnext(f, c, begin, end);
	} while (c != start);

	dprintf("\n");
	return FAT_ERR;
}

int fatclusterfindallocated(fat *f, int32_t begin, int32_t end) {
	return fatclusterfindallocatedbetween(f, begin, end, begin);
}

/*
 * free a chain of clusters
 */
int fatclusterfreechain(fat *f, int32_t begin) {
	int32_t current, next;

	dprintf("free chain:");
	for (current = begin; current >= FAT_FIRST; current = next) {
		next = fatgetnextcluster(f, current);
		fatsetnextcluster(f, current, FAT_UNUSED);
		dprintf(" %d", current);
	}
	dprintf("\n");

	return 0;
}

/*
 * origin and size of a cluster
 */

int fatclusterposition(fat *f, int32_t cl, uint64_t *origin, int *size) {
	if (cl < FAT_ROOT) {
		printf("error: attempt to get position of cluster %d\n", cl);
		exit(1);
	}
	else if (cl == FAT_ROOT) {
		*size = fatgetrootentries(f) * 32;
		*origin = ((uint64_t) fatgetbytespersector(f)) * (
				fatgetreservedsectors(f) +
				((uint64_t) fatgetnumfats(f)) *
				fatgetfatsize(f)
			) -
			FAT_ROOT * *size;
	}
	else {
		*size = fatgetbytespersector(f) * fatgetsectorspercluster(f);
		*origin = ((uint64_t) fatgetbytespersector(f)) * (
				fatgetreservedsectors(f) +
				((uint64_t) fatgetnumfats(f)) *
				fatgetfatsize(f)
			) +
			fatgetrootentries(f) * 32 -
			FAT_FIRST * *size;
	}
	return 0;
}

/*
 * read and create a cluster; writeback is by fatunitwriteback(unit *)
 */

unit *fatclustercreate(fat *f, int32_t cl) {
	uint64_t origin;
	int size;
	unit *cluster;

	fatclusterposition(f, cl, &origin, &size);
	dprintf("origin: %" PRId64 ", size: %d\n", origin, size);

	cluster = fatunitget(&f->clusters, f->offset + origin, size, cl,
			f->fd);
	if (cluster != NULL)
		return cluster;

	cluster = fatunitcreate(size);
	if (cluster == NULL)
		return NULL;
	cluster->n = cl;
	cluster->origin = f->offset + origin;
	if (fatunitinsert(&f->clusters, cluster, 0)) {
		fatunitdestroy(cluster);
		return NULL;
	}
	return cluster;
}

unit *fatclusterread(fat *f, int32_t cl) {
	uint64_t origin;
	int size;

	fatclusterposition(f, cl, &origin, &size);
	dprintf("origin: %" PRId64 ", size: %d\n", origin, size);
	return fatunitget(&f->clusters, f->offset + origin, size, cl, f->fd);
}

/*
 * the cluster that contains a sector
 */
int32_t fatsectorposition(fat *f, uint32_t sector) {
	uint32_t s, ssize;

	s = sector;

	ssize = fatgetreservedsectors(f);
	if (s < ssize)
		return FAT_ERR - 1;
	s -= ssize;

	ssize = fatgetfatsize(f) * fatgetnumfats(f);
	if (s < ssize)
		return FAT_ERR - s / fatgetfatsize(f) - 2;
	s -= ssize;

	ssize = fatgetrootentries(f) * 32 / fatgetbytespersector(f);
	if (s < ssize)
		return FAT_ROOT;
	s -= ssize;

	return s / fatgetsectorspercluster(f) + 2;
}

