#include <iostream>
#include <fstream>
#include <string>

#include "vfo_pid.h"

vfo_pid::vfo_pid() : vfo_base()
{ 
    reset();
}

/**
 * @brief Read PID coefficient from a file 'vfo_settings.txt'.
 * 
 */
void vfo_pid::read_coeff(void) {
    std::ifstream ifs("vfo_settings.txt");
    if(ifs.is_open()) {
        std::string tmp;
        ifs >> tmp; m_phase_err_PC = std::stod(tmp);
        ifs >> tmp; m_phase_err_IC = std::stod(tmp);
        ifs >> tmp; m_phase_err_DC = std::stod(tmp);
        ifs >> tmp; m_phase_diff_PC = std::stod(tmp);
        ifs >> tmp; m_phase_diff_IC = std::stod(tmp);
        ifs >> tmp; m_phase_diff_DC = std::stod(tmp);
        ifs.close();
    }
}

void vfo_pid::disp_vfo_status(void) {
    vfo_base::disp_vfo_status();
    std::cout << "-- vfo_pid --" << std::endl;
    std::cout << "Prev pulse pos : " << m_prev_pulse_pos << std::endl;
    std::cout << "Prev phase err : " << m_prev_phase_err << std::endl;
    std::cout << "Phase diff prev     : " << m_prev_phase_diff << std::endl;
    std::cout << "Phase err  prev     : " << m_prev_phase_err << std::endl;
    std::cout << "Phase diff integral : " << m_phase_diff_I << std::endl;
    std::cout << "Phase err  integral : " << m_phase_err_I << std::endl;
    std::cout << "-- PID coefficients --" << std::endl;
    std::cout << "Phase err  P,I,D coeff  : " << m_phase_err_PC   << ", " << m_phase_err_IC  << ", " << m_phase_err_DC  << std::endl;
    std::cout << "Phase diff P,I,D coeff  : " << m_phase_diff_PC  << ", " << m_phase_diff_IC << ", " << m_phase_diff_DC << std::endl;
}

void vfo_pid::reset(void) {
    vfo_base::reset();
    soft_reset();
}

void vfo_pid::soft_reset(void) {
    vfo_base::soft_reset();
    m_prev_pulse_pos = 0.f;
    
    m_prev_phase_err = 0.f;
    m_phase_err_I = 0.f;

    m_prev_phase_diff = 0.f;
    m_phase_diff_I = 0.f;
#if 0
    m_phase_err_PC =1.f/8.f;
    m_phase_err_IC =1.f/32.f;
    m_phase_err_DC =1.f/64.f;
    m_phase_diff_PC =1.f/128.f;
    m_phase_diff_IC =1.f/128.f;
    m_phase_diff_DC =1.f/64.f;
#endif
    m_phase_err_PC =1.f/8.f;
    m_phase_err_IC =1.f/32.f;
    m_phase_err_DC =1.f/128.f;
    m_phase_diff_PC =1.f/512.f;
    m_phase_diff_IC =1.f/64.f;
    m_phase_diff_DC =1.f/256.f;
    read_coeff();       // Read PID coefficit from a file.
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
double vfo_pid::calc(double pulse_pos) {
    // Detect whether the pulse position is leading or lagging. (phase_diff (-)==slow freq, (+)==fast freq)
    double phase_diff_P = m_prev_pulse_pos - pulse_pos;
    // phase correction. assuming phase difference is less than 180deg.
    if(phase_diff_P >   m_cell_size / 2.f) {
        phase_diff_P -= m_cell_size;
    } else 
    if (phase_diff_P < -m_cell_size / 2.f) {
        phase_diff_P += m_cell_size;
    }
    double phase_diff_D = phase_diff_P - m_prev_phase_diff;
    m_phase_diff_I     += phase_diff_P;
    m_prev_phase_diff   = phase_diff_P;

    // Phase error from the center of the bit cell
    double phase_err_P  = m_cell_center - pulse_pos;
    double phase_err_D  = phase_err_P - m_prev_phase_err;
    m_phase_err_I      += phase_err_P;
    m_prev_phase_err    = phase_err_P;

    // Cell size adjustment == frequency correction + phase adjust
    //double new_cell_size = m_cell_size_ref - (m_phase_diff_integral * (1.0f/24.f) ) * m_current_gain - 
    //                        (phase_err * (1.0f/2.f) - phase_err_diff * (1.0f/8.f) + m_phase_err_integral * (1.0f/64.f));
    //double new_cell_size = m_cell_size_ref 
    //                       - (/*phase_diff * (1.f/4.f) +*/ m_phase_diff_integral * (1.f/64.f) ) * m_current_gain              // Freq control (PI)
    //                       - (phase_err * (1.f/4.f) - phase_err_diff * (1.f/8.f) + m_phase_err_integral * (1.0f/64.f));   // Phase control (PID)
    //double new_cell_size = m_cell_size_ref 
    //            - (phase_diff_P * (1.f/8.f) - phase_diff_D * (1.f/4.f) + m_phase_diff_I * (1.f/64.f)) * m_current_gain   // Freq control (PI)
    //            - (phase_err_P  * (1.f/8.f) - phase_err_D  * (1.f/4.f) + m_phase_err_I  * (1.f/64.f));                  // Phase control (PID)
    double new_cell_size = m_cell_size_ref 
                - (phase_diff_P * m_phase_diff_PC - phase_diff_D * m_phase_diff_DC + m_phase_diff_I * m_phase_diff_IC) * m_current_gain   // Freq control (PI)
                - (phase_err_P  * m_phase_err_PC  - phase_err_D  * m_phase_err_DC  + m_phase_err_I  * m_phase_err_IC );                  // Phase control (PID)

    // FDD spindle variation (long term) = 2-2.5%
    // FDD spindle wow/flutter variation (short term)= +-2-2.5%
    // fast-slow combination = (2.5+2.5) *2 = 10%
    // VFO oscillation center frequency variation by temp and power supply voltage variation = 5%
    // Varition in total = 15% 
    //constexpr double tolerance = 0.5f;
    constexpr double tolerance = 0.8f;
    new_cell_size = limit(new_cell_size, m_cell_size_ref * (1.f/(1.f + tolerance)) , m_cell_size_ref * (1.0f + tolerance));

    set_cell_size(new_cell_size);
    m_prev_pulse_pos = pulse_pos;

    return pulse_pos;
}
