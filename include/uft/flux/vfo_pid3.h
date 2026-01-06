#pragma once

#include <vector>

#include "fdc_vfo_base.h"

/**
 * @brief VFO class using PID control
 * 
 */
class vfo_pid3 : public vfo_base {
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

	// to prevent controller becoming unstable on sudden change of gain.
	double m_gain_used=1.0;

    std::vector<double> m_pulse_pos_history;
    size_t m_hist_ptr;
    const size_t c_history_len = 4;     // must be power of 2 (2^n)
    double m_coeff_sum;                 // sum of coefficiencies for pulse pos LPF

public:
    vfo_pid3();
    void read_coeff(void);
    void disp_vfo_status(void) override;
    void reset(void) override;
    void soft_reset(void) override;
    double calc(double pulse_pos) override;
};
