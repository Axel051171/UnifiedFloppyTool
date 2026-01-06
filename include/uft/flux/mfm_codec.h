#pragma once

#include <vector>
#include <random>

#include "bit_array.h"
#include "fdc_vfo_base.h"

//#define DEBUG

class mfm_codec {
private:
    bit_array   m_track;
    bool        m_track_ready;
    bool        m_sync_mode;
    uint64_t    m_bit_stream;                         // decoded MFM bit stream from the raw track bit data
    size_t      m_bit_width_w = ((4e6 / 1e6) * 2);    // Bit cell width for writing. default == 8  # 4MHz/500KHz
    const uint16_t m_missing_clock_a1 = 0x4489;       //  0100_0100_10*0_1001
    const uint16_t m_missing_clock_c2 = 0x5224;       //  0101_0010_*010_0100
    const uint16_t m_missing_clock_14 = 0x8912;       //  1000_1001_0*01_0010  A1 can decay to become 14
    const uint16_t m_missing_clock_28 = 0x5224;       //  1010_010*_0100_1000  C2 can decay to become 28
    const uint64_t m_pattern_ff = 0x5555555555555555; // for 4 bytes of sync data(unused)
    const uint64_t m_pattern_00 = 0xaaaaaaaaaaaaaaaa; // for 4 bytes of sync data

    bool        m_wraparound;                         // Flag to indicate that the head position went over the index hole (buffer wrap arounded)

    size_t m_prev_write_bit;        // to preserve the status of previous write data bit for MFM encoding

    // for data separator
    size_t      m_sampling_rate;
    size_t      m_data_bit_rate;

    size_t      m_current_bit_pos;          // current bit position to process
    double      m_distance_to_next_pulse;
    double      m_vfo_suspension_rate;
    vfo_base    *m_vfo;

    std::random_device m_rnd;

public:
    enum gain_state {
        low = 0,
        high = 1
    };
    gain_state  m_current_vfo_gain;

public:
    mfm_codec();
    ~mfm_codec();

    void set_vfo_type(size_t vfo_type);
    void reset_vfo(void);
    void soft_reset_vfo(void);

    void reset(void);
    void set_cell_size(double cell_size, double window_ratio=0.75f);    // typical window_ratio for actual drive is 0.5 but uses 0.75 as default in this SW
    void update_parameters(void);
    void set_data_bit_rate(size_t data_bit_rate);
    void set_sampling_rate(size_t sampling_rate);

    void set_track_data(bit_array track);
    bit_array get_track_data(void);
    void unset_track_data(void);
    inline bool is_track_ready(void) const { return m_track_ready; }
    inline void set_track_status_forcibly(bool status) { m_track_ready = status; };

    inline bool is_wraparound(void) const { return m_wraparound; }
    inline void clear_wraparound(void) { m_wraparound = false; }
    inline size_t get_track_length(void) { return m_track.get_bit_length(); }       /** unit = bit */

    int read_bit_ds(void);
    int read_bit_ds(double &error);
    bool mfm_read_byte(uint8_t& data, bool& missing_clock, bool ignore_missing_clock = true, bool ignore_sync_field = true);
    bool mfm_read_byte(uint8_t& data, bool& missing_clock, double &error, bool ignore_missing_clock = true, bool ignore_sync_field = true);
    void set_vfo_gain(gain_state state);
    void set_vfo_cell_size(double cell_size);
    uint16_t mfm_encoder(uint8_t data, bool mode = false);
    void mfm_write_byte(uint8_t data, bool mode = false, bool write_gate = true, bool elastic = false);
    void set_pos(size_t bit_pos);
    size_t get_pos(void);
    void set_real_pos(size_t bit_pos);
    size_t get_real_pos(void);

    inline void reset_sync_mode(void) { m_sync_mode = false; }
    inline void set_sync_mode(bool sync_mode) { m_sync_mode = sync_mode; }
    inline void set_vfo_gain_val(double low, double high) { m_vfo -> set_gain_val(low, high); }
    inline void disp_vfo_status(void) { m_vfo->disp_vfo_status(); }

    void enable_fluctuator(size_t numerator, size_t denominator);
    void enable_fluctuator(double vfo_suspension_rate);
    void disable_fluctuator(void);
};
