// a8rawconv - A8 raw disk conversion utility
// Copyright (C) 2014-2020 Avery Lee
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"
#include <time.h>

uint64_t get_time64() {
	static_assert(sizeof(time_t) > 4, "time_t is 32-bit when ideally it should be 64-bit.");

	return (uint64_t)time(nullptr);
}

std::string get_localtime_scp_us() {
	// get current time as UTC
	time_t now = time(nullptr);

	// break it apart into calendar fields
	tm now_tm = *localtime(&now);

	// format in the form used by SuperCard Pro in US locale
	char buf[128] {};
	snprintf(buf, sizeof buf, "%u/%u/%u %u:%02u:%02u %s"
		, now_tm.tm_mon + 1
		, now_tm.tm_mday
		, now_tm.tm_year + 1900
		, (now_tm.tm_hour + 11) % 12 + 1
		, now_tm.tm_min
		, now_tm.tm_sec
		, now_tm.tm_hour >= 12 ? "PM" : "AM"
	);
	buf[127] = 0;

	return std::string(buf);
}
