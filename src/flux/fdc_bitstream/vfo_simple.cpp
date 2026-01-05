#include <iostream>

#include "vfo_simple.h"

void vfo_simple::reset(void) {
    vfo_base::reset();
    m_freq_integral = 0.f;
}

void vfo_simple::soft_reset(void) {
    vfo_base::soft_reset();
    m_freq_integral = 0.f;
}

void vfo_simple::disp_vfo_status(void) {
    vfo_base::disp_vfo_status();
    std::cout << "-- vfo_simple --" << std::endl;
    std::cout << "Freq integral  : " << m_freq_integral << std::endl;
}

/**
 * @brief Simple VFO
 * 
 * @param pulse_pos 
 * @return double 
 */
double vfo_simple::calc(double pulse_pos) {

    // Cell size adjustment == frequency correction
    double freq_error = m_cell_center - pulse_pos;
    m_freq_integral += freq_error;
    double new_cell_size = m_cell_size_ref - m_freq_integral * 0.1f * m_current_gain;        // 'P' only control
#if 1
    // Data pulse position adjustment == phase correction
    double phase_error = m_cell_center - pulse_pos;
    double phase_correction = phase_error * 0.2f * m_current_gain;  // P control (no ID element)
    pulse_pos += phase_correction;
#endif
    set_cell_size(new_cell_size);

    return pulse_pos;
}