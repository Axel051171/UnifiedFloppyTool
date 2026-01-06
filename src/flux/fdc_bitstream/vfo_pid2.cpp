#include <iostream>

#include "vfo_pid2.h"

void vfo_pid2::disp_vfo_status(void) {
    vfo_base::disp_vfo_status();
    std::cout << "-- vfo_pid2 --" << std::endl;
    std::cout << "Prev phase err : " << m_prev_phase_error << std::endl;
    std::cout << "Phase integral : " << m_phase_integral << std::endl;
    std::cout << "Prev freq err  : " << m_prev_freq_error << std::endl;
    std::cout << "Freq integral  : " << m_freq_integral << std::endl;
}

void vfo_pid2::reset(void) {
    vfo_base::reset();
    m_prev_phase_error = 0.f;
    m_prev_freq_error = 0.f;
    m_phase_integral = 0.f;
    m_freq_integral = 0.f;
}

void vfo_pid2::soft_reset(void) {
    vfo_base::soft_reset();
    m_prev_phase_error = 0.f;
    m_prev_freq_error = 0.f;
    m_phase_integral = 0.f;
    m_freq_integral = 0.f;
}


/**
 * @brief Calculate new pulse position based on the current bit cell parameters. Adjust pulse position, and calculate and optimize bit cell parameters.
 * 
 * @param pulse_pos Pulse position (the distance to the next pulse position in bit unit).
 * @return double Adjusted pulse position.
 *            0123456789abcdef0
 *  Bit cell  |               |
 *  Window    |   WWWWWWWW    |
 *  Center    |       ^       |
 *  Pulse     |     |         |
 *  Error     |     |-|       |
 * 
 *  Phase correction : shift next pulse position to adjust the phase.
 *  Freq. correction : change bit cell size by accumlated (integrated) error.
 */
double vfo_pid2::calc(double pulse_pos) {

    // Cell size adjustment == frequency correction
    double freq_error = m_cell_center - pulse_pos;
    double freq_error_diff = freq_error - m_prev_freq_error;
    m_freq_integral += freq_error;                                                              // 'I' element
    double new_cell_size = m_cell_size_ref - (m_freq_integral * 0.01f) * m_current_gain;        // 'I' only control
    m_prev_freq_error = freq_error;

    // Data pulse position adjustment == phase correction
    double phase_error = m_cell_center - pulse_pos;
    constexpr double phase_error_limitter = 0.25f;
    phase_error = limit(phase_error, -m_cell_size_ref * phase_error_limitter, 
                                      m_cell_size_ref * phase_error_limitter);
    m_phase_integral += phase_error;
    double phase_err_diff =  phase_error - m_prev_phase_error;
    double phase_correction = (phase_error * 0.02f /*- phase_err_diff * 0.02f*/ + m_phase_integral * 0.002f) * m_current_gain;  // PI control (no D element)
    constexpr double phase_correction_limitter = 0.125f;
    phase_correction = limit(phase_correction, -m_cell_size_ref * phase_correction_limitter, 
                                                m_cell_size_ref * phase_correction_limitter);
    pulse_pos += phase_correction;
    m_prev_phase_error = phase_error;

    set_cell_size(new_cell_size);

    return pulse_pos;
}
