#pragma once

#include "fdc_vfo_base.h"

/**
 * @brief VFO class using simple proportional error correction (P)
 * 
 */
class vfo_simple : public vfo_base {
public:
    double m_freq_integral;
public:
    void disp_vfo_status(void) override;
    void reset(void) override;
    void soft_reset(void) override;
    double calc(double pulse_pos) override;
};
