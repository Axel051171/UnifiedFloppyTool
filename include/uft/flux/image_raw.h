#pragma once

#include <cassert>

#include <string>
#include <sstream>
#include <vector>

#include "image_base.h"

class disk_image_raw : public disk_image {
private:
    size_t  m_overlap;
public:
    disk_image_raw() : disk_image(), m_overlap() {};

    void read(const std::string raw_file_name) override;
    void write(const std::string raw_file_name) const override;

    disk_image_raw& operator=(disk_image &image)
    {
        m_base_prop = image.get_property();
        m_track_data = image.get_track_data_all();
        return *this;
    }

};
