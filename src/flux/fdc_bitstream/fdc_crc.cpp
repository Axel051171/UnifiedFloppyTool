/**
 * @file fdc_crc.cpp
 * @author Yasunori Shimura (yasu0710@gmail.com)
 * @brief CRC data generator for FDC
 * @version 0.1
 * @date 2022-08-11
 * 
 * @details
 * CRC polynomial = CCITT - 16  G(X) = 1 + X ^ 5 + X ^ 12 + X ^ 16 == 1 001 000 0010 001 == 0x11021.
 * FDC calculates CRC from the top of the address marks(0xA1) to CRC values.
 * When FDC generates CRC, the CRC field(2 bytes) are set to 0x00, 0x00 and the initial CRC value is 0x84Cf.
 * However, the top of address marks can't be read precisely when reading the data from the disc. I decided to skip the top x3 0xA1 values and start CRC caluculation from (F8/FB/FC/FE) with CRC initial value of 0xe59a.
 *
 * If the data are read correctly, the return value will be 0x0000.
 * data = [0xfe, 0x01, 0x01, 0x03, 0x01, 0xdd, 0xea].
 * crc_value = crc.data(data[:])  # crc_value must be 0.
 * 
 * @copyright Copyright (c) 2022
 */
#include <vector>

#include "fdc_crc.h"

/**
 * @brief Construct a new fdc crc::fdc crc object
 * 
 */
fdc_crc::fdc_crc() { 
	reset();
};

/**
 * @brief Reset CRC generator.
 * 
 * @param type Determine CRC preset value. Default=0. Use normal preset value unless you have specific requirement. (others:Normal(0xe59a00), 1:Special(0x84cf00))
 */
void fdc_crc::reset(size_t type) {
	switch (type) {
	case 1:  
		/** In case AM = A1 A1 A1 + F8 / FB / FC / FE */
		m_crc_val = 0x84cf00; break;
	default: 
		/** In case the 1st x3 A1s are omitted(F8 / FB / FC / FE) */
		m_crc_val = 0xe59a00; break;
	}
}

/**
 * @brief Get CRC calculated value.
 * 
 * @return uint16_t CRC value.
 */
uint16_t fdc_crc::get(void) { return (m_crc_val >> 8) & 0x0ffffu; }

/**
 * @brief Supply a data for CRC calculation.
 * 
 * @param[in] dt Input data to the CRC generator
 */
void fdc_crc::data(uint8_t dt) {
	m_crc_val |= dt;
	for (size_t i = 0; i < 8; i++) {
		m_crc_val <<= 1;
		if (m_crc_val & 0x1000000) {
			m_crc_val ^= m_polynomial;
		}
	}
}

/**
 * @brief Supply a data array for CRC calculation. All data in the buffer will be supplyed to the CRC generator.
 * 
 * @param buf Input data to the CRC generator.
 */
void fdc_crc::data(std::vector<uint8_t>& buf) {
	for (auto it = buf.begin(); it != buf.end(); ++it) {
		data(static_cast<uint8_t>(*it));
	}
}
