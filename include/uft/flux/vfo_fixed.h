#pragma once

#include "fdc_vfo_base.h"

/**
 * @brief Fixed VFO class (no VFO)
 * 
 */
class vfo_fixed : public vfo_base {
public:
    void disp_vfo_status(void) override {
        vfo_base::disp_vfo_status();
        std::cout << "-- vfo_fixed --" << std::endl;
    }
    double calc(double pulse_pos) override;
};
