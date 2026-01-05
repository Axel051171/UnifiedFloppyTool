#pragma once

#include "fdc_vfo_base.h"

/**
 * @brief VFO class using PID control (2)
 * 
 */
class vfo_pid2 : public vfo_base {
public:
    double m_prev_phase_error;
    double m_phase_integral;
    double m_prev_freq_error;
    double m_freq_integral;
public:
    void disp_vfo_status(void) override;
    void reset(void) override;
    void soft_reset(void) override;
    double calc(double pulse_pos) override;
};
