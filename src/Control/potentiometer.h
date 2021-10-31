#ifndef SRC_CONTROL_POTENTIOMETER_H_
#define SRC_CONTROL_POTENTIOMETER_H_

#include <stdint.h>

#include "Filter.h"

class Potentiometer
{
public:
    Potentiometer(uint8_t pin, uint32_t sampling_ms);
    void setup();
    void loop();

    // 0.0-1.0
    float read() const;

private:
    uint16_t filter(uint16_t value);
    void     auto_calibrate();
    float    map_to_calibrated_range() const;

    const uint8_t       pin_;
    const unsigned long sampling_ms_;
    uint16_t            current_value_;
    Filter              filter_;

    // Variables for auto-calibration
    Filter   auto_calibration_filter_;
    bool     is_auto_calubration_in_progress_;
    uint16_t calibrated_min_val_;
    uint16_t calibrated_max_val_;

    // This is for debugging and confuguring filters
    // void print_debug(uint16_t new_value);
};

#endif  // SRC_CONTROL_POTENTIOMETER_H_
