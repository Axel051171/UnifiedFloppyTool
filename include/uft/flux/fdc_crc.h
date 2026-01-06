#pragma once

#include <vector>
#include <cstdint>

class fdc_crc {
private:
	const uint32_t m_polynomial = 0b0001000100000010000100000000;   // 0001 0001 0000 0010 0001 [0000 0000]
	uint32_t       m_crc_val;
public:
	fdc_crc();

	void reset(size_t type = 0);
	uint16_t get(void);
	
	void data(uint8_t dt);
	void data(std::vector<uint8_t>& buf);
};
