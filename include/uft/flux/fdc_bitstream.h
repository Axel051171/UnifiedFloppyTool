#pragma once

#include <vector>
#include <random>
#include <cstdint>
#include <string.h>

#include "fdc_crc.h"
#include "mfm_codec.h"

//#define DEBUG

class fdc_bitstream {
private:
    /** 
     * FDC state machine status definition.
     */
    enum class fdc_state {
        IDLE = 0,
        CHECK_MARK,                 // Check special mark with missing clock pattern (A1/C2)
        READ_IDAM,                  // Read ID address mark
        READ_SECT,                  // Read Sector data
        OPERATION_COMPLETED         // Read data completed
    } m_state;

    mfm_codec m_codec;              // MFM codec object
    fdc_crc m_crcgen;               // CRC generator object

    size_t m_sampling_rate;         // Track data sampling rate [Hz] (4MHz == 4e6)
    size_t m_data_bit_rate;         // FDC bit data rate [bit/sec] (2D/MFM == 500Kbit/sec == 5e3

    std::default_random_engine m_rand_engine;
    std::normal_distribution<> m_rand_dist;
    inline int normal_distribution_random(void) { return static_cast<int>(m_rand_dist(m_rand_engine)); }  // around -3 ~ +3
public:
    /**
    * Structure for ID field data.
    */
    struct id_field {
        uint8_t     C;
        uint8_t     H;
        uint8_t     R;
        uint8_t     N;
        uint16_t    crc_val;
        bool        crc_sts;
        size_t      pos;
        size_t      end_pos;
    };

    /** 
     * Structure for sector data.
     */
    struct sector_data {
        std::vector<uint8_t> data;
        std::vector<size_t> pos;
        bool        dam_type;      /** false:DAM, true : DDAM */
        bool        crc_sts;       /** true:error */
        bool        record_not_found;
        size_t      id_pos;
        size_t      data_pos,data_end_pos;
    };

    enum gain_state {
        low = 0,
        high = 1
    };

    fdc_bitstream();
    void set_fdc_params(size_t sampling_rate, size_t data_bit_rate, double bit_window_ratio = 0.75f);   // bit_window_ratio = 0.5 is the standard defined in MFM spec.
    /** unit = bit */
    inline size_t get_track_length(void) { return m_codec.get_track_length(); }
    void set_track_data(bit_array track_data);
    bit_array get_track_data(void);
    void set_pos(size_t bit_pos);
    size_t get_pos(void);
    void set_real_pos(size_t bit_pos);
    size_t get_real_pos(void);
    inline bool is_wraparound(void) { return m_codec.is_wraparound(); }
    inline void clear_wraparound(void) { m_codec.clear_wraparound(); }
	bool read_byte(uint8_t& data, bool& missing_clock, double &error, bool ignore_missing_clock, bool ignore_sync_field);


    /** 
     * @brief Enable FDC read operation fluctuatior.
     * If you set the fluctuator with enable_fluctuator(1,4), the VFO in the data separator will operate at 
     * rate of 3/4 and stop operation at the rate of 1/4. The operation will be determined by 
     * a random generator, so the VFO operation will be stochastic, and this introduces some uncertainty in the read data.
     * @deprecated This function is deprecated. Use fdc_bitcontrol::enable_fluctuator(double) instead.
     */
    inline void enable_fluctuator(size_t numerator, size_t denominator) { m_codec.enable_fluctuator(numerator, denominator); }
    /** 
     * @brief Enable FDC read operation fluctuatior.
     * The VFO in the C/D separator will suspend its operation at the rate of vfo_suspension_rate (0.0-1.0).
     * The operation will be determined by a random generator, so the VFO operation will be stochastic, and this introduces some uncertainty in the read data.
     */
    inline void enable_fluctuator(double vfo_suspension_rate) { m_codec.enable_fluctuator(vfo_suspension_rate); }
    /** Disable FDC read operation fluctuator */
    inline void disable_fluctuator(void) { m_codec.disable_fluctuator(); }

    /** 
     * @brief Set VFO gain.
    * Recommended setting : High gain (high speed) mode(2.0f). Low gain (1.0f) for data reading. 
    * FDC needs to synchronized with the data stream quickly in the SYNC field before start reading the actual data (sector ID or sector body).
    * High speed (high gain) setting will be used during the SYNC field to lock-in the read timing quickly.
    * Low speed (low gain) setting will be used during reading data (other than SYNC field) to cancel slow spindle speed 
    * fluctuation but not follow the quick timing change such as irregular pulses in the bit stream and keep the same reading pace.
    */
    inline void set_vfo_gain_val(double low, double high) { m_codec.set_vfo_gain_val(low, high); }

    /**
     * @brief Set the VFO gain mode.
     *        Low gain for regular data reading and high gain for SYNC field.
     * 
     * @param state fdc_bitstream::gain_state::low or fdc_bitstream::gain_state::high
     */
    inline void set_vfo_gain_mode(gain_state state) { 
        switch(state) {
        case gain_state::low:
        default:
            m_codec.set_vfo_gain(mfm_codec::gain_state::low);
            break;
        case gain_state::high:
            m_codec.set_vfo_gain(mfm_codec::gain_state::high);
            break;
        }
    }
    inline void disp_vfo_status(void) { m_codec.disp_vfo_status(); }
    /**
     * @brief Set new VFO.
     * @param vfo_type 0=vfo_simple, 1=vfo_fixed, 2=vfo_pid, 3=vfo_pid2, 9=vfo_experimental
     */
    inline void set_vfo_type(size_t vfo_type) { m_codec.set_vfo_type(vfo_type); }
    /** 
     * @brief Reset VFO parameters
     */
    inline void reset_vfo(void) { m_codec.reset_vfo(); }
    /**
     * @brief Soft reset VFO parameters.
     *        Keep bit_cell_ref and reset window_size, window_ofst parameters.
     */
    inline void soft_reset_vfo(void) { m_codec.soft_reset_vfo(); }

    void write_data(uint8_t data, bool mode = false, bool write_gate = true);
    void read_data(uint8_t& data, bool& missing_clock, bool ignore_missing_clock = true, bool ignore_sync_field = true);

    std::vector<uint8_t> read_track(void);
    std::vector<std::vector<size_t>> read_track_ex(void);

    void write_track(const std::vector<uint8_t>& track_buf);
    size_t read_id(std::vector<uint8_t>& id_field, bool& crc_error);
    std::vector<id_field> read_all_idam(void);
    size_t read_sector_body_ex(size_t sect_length_code, std::vector<uint8_t>& sect_data, std::vector <size_t> &pos, bool& crc_error, bool& dam_type, bool &record_not_found, bool timeout=true);
    size_t read_sector_body(size_t sect_length_code, std::vector<uint8_t>& sect_data, bool& crc_error, bool& dam_type, bool &record_not_found, bool timeout=true);
    void write_sector_body(std::vector<uint8_t> write_data, bool dam_type, bool write_crc=true);

    sector_data read_sector(int trk, int sct);
    bool write_sector(int trk, int sct, bool dam_type, std::vector<uint8_t>& write_data, bool fluctuate=false);
};
