#pragma once

#include "fdc_vfo_base.h"

/**
 * @brief VFO class using PID control (2)
 * 
 */
class vfo_simple2 : public vfo_base {
public:

public:
    void disp_vfo_status(void) override;
    void reset(void) override;
    void soft_reset(void) override;
    double calc(double pulse_pos) override;
};
