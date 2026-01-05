/**
 * @file fdc_vfo_base.cpp
 * @author Yasunori Shimura (yasu0710@gmail.com)
 * @brief VFO for data separator
 * @version 0.1
 * @date 2022-08-20
 * 
 * @copyright Copyright (c) 2022
 *
 */

#include <iostream>

#include "fdc_vfo_base.h"

void vfo_base::disp_vfo_status(void) {
    std::cout << "-- vfo_base --" << std::endl;
    std::cout << "Cell_size : " << m_cell_size << std::endl;
    std::cout << "Cell_size_ref : " << m_cell_size_ref << std::endl;
    std::cout << "Window ratio  : " << m_window_ratio << std::endl;
    std::cout << "Window size   : " << m_window_size << std::endl;
    std::cout << "Windoe offset : " << m_window_ofst << std::endl;
    std::cout << "Gain (Low)    : " << m_gain_l << std::endl;
    std::cout << "Gain (High)   : " << m_gain_h << std::endl;
    std::cout << "Current gain  : " << m_current_gain << std::endl;
}

void vfo_base::reset(void) {
    set_params(4e6, 500e3);
    set_gain_val(VFO_GAIN_L_DEFAULT, VFO_GAIN_H_DEFAULT);
    set_gain_mode(vfo_base::gain_state::low);
}

void vfo_base::soft_reset(void) {
    if(m_sampling_rate != 0 && m_fdc_bit_rate != 0 && m_data_window_ratio != 0) {
        set_params(m_sampling_rate, m_fdc_bit_rate, m_data_window_ratio);
    }
}

void vfo_base::set_params(size_t sampling_rate, size_t fdc_bit_rate, double data_window_ratio) {
    // keep parameters for soft reset
    m_sampling_rate = sampling_rate;
    m_fdc_bit_rate = fdc_bit_rate;
    m_data_window_ratio = data_window_ratio;

    m_cell_size_ref = static_cast<double>(sampling_rate) / static_cast<double>(fdc_bit_rate);
    m_cell_size = m_cell_size_ref;

    data_window_ratio = limit(data_window_ratio, 0.3f, 0.9f);
    m_window_ratio = data_window_ratio;
    update_cell_params();
}

void vfo_base::set_cell_size(double cell_size) {
    //constexpr double tolerance = 0.2f;
    //cell_size = limit(cell_size, m_cell_size_ref * (1.f - tolerance), m_cell_size_ref * (1.f + tolerance));
    m_cell_size = cell_size;
    update_cell_params();
}

/**
 * @brief Update bit cell related parameters.
 *   012345678
 *   | WWWW  |
 *     <-->    Window size (4)
 *   <->       Window ofst (2)
 *   <------>  Cell size   (8)
 */
void vfo_base::update_cell_params(void) {
    m_window_size = m_cell_size * m_window_ratio;               // typical window_ratio for actual drive is 0.5 but uses 0.75 as default in this SW
    m_window_size = m_window_size < 1.f ? 1.f : m_window_size;  // Avoid too narrow m_data_window_size situation
    m_window_ofst = (m_cell_size - m_window_size) / 2.f;
    m_cell_center = m_cell_size / 2.f; 
}

void vfo_base::set_gain_val(double gain_l, double gain_h) {
     m_gain_l = gain_l;
     m_gain_h = gain_h; 
}

/**
 * @brief Set VFO gain mode (H or L).
 * 
 * @param state vfo_base::gain_state::low or vfo_base::gain_state::high. The low gain setting will be used for normal data reading and switched to the high gain setting during SYNC field reading. 
 *              mfm_codec class will control the gain setting through this function.
 */
void vfo_base::set_gain_mode(gain_state state) {
    switch(state) {
    case gain_state::low:
    default:
        m_current_gain = m_gain_l;
        break;
    case gain_state::high:
        m_current_gain = m_gain_h;
        break;
    }
}

