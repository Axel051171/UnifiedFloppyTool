#pragma once
/**
 * @brief VFO for data separator in mfm_codec
 * 
 */

#include "fdc_vfo_def.h"

/**
 * @brief Base class of VFO (virtual base class).
 * 
 */
class vfo_base {
public:
    double  m_cell_size;        // Current data cell size (width) in samping data count (4 means data cell size is 4 sampling data width)
    double  m_cell_size_ref;    // Reference data cell size (standard cell size)
    double  m_window_ratio;     // data window width ratio to the data cell width (0.75 means window = cell*0.75 )
    double  m_window_size;      // Current data window size (width) in sampling data count
    double  m_window_ofst;      // Current data window start position from the top of the data cell
    double  m_cell_center;

    double  m_gain_l;
    double  m_gain_h;
    double  m_current_gain;
    // for soft reset
    double  m_sampling_rate;
    double  m_fdc_bit_rate;
    double  m_data_window_ratio;
public:
    enum gain_state {
        low = 0,
        high = 1
    };
public:
    vfo_base() : m_cell_size(), m_cell_size_ref(), m_window_ratio(), m_window_size(), m_window_ofst(), m_cell_center(),
                 m_gain_l(VFO_GAIN_L_DEFAULT), m_gain_h(VFO_GAIN_H_DEFAULT), 
                 m_current_gain(vfo_base::gain_state::low),
                 m_sampling_rate(), m_fdc_bit_rate(), m_data_window_ratio() {};
    virtual void disp_vfo_status(void);
    virtual void reset(void);
    virtual void soft_reset(void);
    virtual void set_params(size_t sampling_rate, size_t fdc_bit_rate, double data_window_ratio = 1.f);
    virtual void set_cell_size(double cell_size);
    virtual void update_cell_params(void);
    void set_gain_val(double gain_l, double gain_h);
    void set_gain_mode(gain_state state);
    virtual double calc(double pulse_pos) = 0;
    inline double limit(double val, double lower_limit, double upper_limit) {
        if(val < lower_limit) val = lower_limit;
        if(val > upper_limit) val = upper_limit;
        return val;
    }
};

#include "vfo_fixed.h"
#include "vfo_simple.h"
#include "vfo_pid.h"
#include "vfo_pid2.h"
#include "vfo_simple2.h"
#include "vfo_pid3.h"
#include "vfo_experimental.h"
