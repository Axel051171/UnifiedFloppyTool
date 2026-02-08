#pragma once

#include "fdc_vfo_base.h"

/**
 * @brief VFO class using PID control
 * 
 */
class vfo_pid : public vfo_base {
public:
    double m_prev_pulse_pos;

    double m_prev_phase_err;
    double m_phase_err_I;

    double m_prev_phase_diff;
    double m_phase_diff_I;

    // coefficients
    double m_phase_err_PC;
    double m_phase_err_IC;
    double m_phase_err_DC;
    double m_phase_diff_PC;
    double m_phase_diff_IC;
    double m_phase_diff_DC;

public:
    vfo_pid();
    void read_coeff(void);
    void disp_vfo_status(void) override;
    void reset(void) override;
    void soft_reset(void) override;
    double calc(double pulse_pos) override;
};
