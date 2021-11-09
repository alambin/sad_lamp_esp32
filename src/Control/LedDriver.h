#ifndef SRC_CONTROL_LED_DRIVER_H_
#define SRC_CONTROL_LED_DRIVER_H_

#include <WString.h>
#include <stdint.h>

#include "Pwm.h"

// Controls current driver for powerful LED
class LedDriver
{
public:
    LedDriver(uint8_t pin, uint32_t pwm_frequency, uint32_t updating_period_ms = 1000);
    void setup();
    void run_sunrise();

    void set_sunrise_duration(uint16_t duration_m);

    void set_brightness_manually(float level);  // level is in range [0..1023]
    void set_thermal_factor(float thermal_factor);

    void start_sunrise();
    void stop_sunrise();

private:
    float    map_sunrise_time_to_level(uint32_t delta_time_ms);
    float    map_manual_control_to_level(float manual_level);
    uint16_t brightness_to_pwm_duty(float level);

    Pwm                      pwm_;
    static constexpr uint8_t kPwmResolutionBits = 10;
    const unsigned long      initial_updating_period_ms_;
    unsigned long            adjusted_updating_period_ms_;
    bool                     is_sunrise_in_progress_;
    unsigned long            sunrise_start_time_;
    uint32_t                 sunrise_duration_sec_;
    float                    current_brightness_;
    float                    thermal_factor_;
};

#endif  // SRC_CONTROL_LED_DRIVER_H_
