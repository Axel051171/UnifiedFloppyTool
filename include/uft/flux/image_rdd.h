#pragma once

#include <string.h>

#include "bit_array.h"
#include "image_base.h"
#include "fdc_vfo_def.h"
#include "fdc_bitstream.h"

class disk_image_rdd : public disk_image {
private:
    size_t m_vfo_type;
    double m_gain_l, m_gain_h;
public:
    disk_image_rdd(void) : disk_image(), m_vfo_type(VFO_TYPE_DEFAULT), m_gain_l(VFO_GAIN_L_DEFAULT), m_gain_h(VFO_GAIN_H_DEFAULT) {};

    void read(const std::string file_name) override;
    void read(std::istream &ifp);
    void write(const std::string file_name) const override;
    bool write(std::ostream &ofp) const ;
    void set_vfo_type(const size_t vfo_type) override { m_vfo_type = vfo_type; };
    void set_gain(double gain_l, double gain_h) override { m_gain_l = gain_l; m_gain_h = gain_h; };

	bool IsFM7CorocoroTypeA(class fdc_bitstream &fdc,uint64_t pos,unsigned char C,unsigned char H,unsigned char R,bool crc_sts,const std::vector <uint8_t> &data) const;
	bool IsFM7CorocoroTypeB(class fdc_bitstream &fdc,uint64_t pos,unsigned char C,unsigned char H,unsigned char R,bool crc_sts,const std::vector <uint8_t> &data) const;

	bool CheckCorocoroTypeASignature(bool crc_sts,const std::vector <uint8_t> &data) const;
	bool CheckCorocoroTypeBSignature(bool crc_sts,const std::vector <uint8_t> &data) const;

	bool CheckLeafInTheForestSignature(uint8_t C,uint8_t H,const std::vector <fdc_bitstream::id_field> &id_list) const;

	std::vector <bool> MarkUnstablePulses(fdc_bitstream &fdc,const std::vector<fdc_bitstream::id_field> &id_list) const;
};
