#include <iostream>

#include "vfo_simple2.h"

void vfo_simple2::disp_vfo_status(void) {
    vfo_base::disp_vfo_status();
    std::cout << "-- vfo_simple2 --" << std::endl;
}

void vfo_simple2::reset(void) {
    vfo_base::reset();
}

void vfo_simple2::soft_reset(void) {
    vfo_base::soft_reset();
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
double vfo_simple2::calc(double pulse_pos) {
    double phase_diff = m_cell_center - pulse_pos;
    double new_cell_size = m_cell_size - phase_diff * 0.025f * m_current_gain;
    set_cell_size(new_cell_size);

    pulse_pos = m_cell_center;      // fixed
    return pulse_pos;
}
