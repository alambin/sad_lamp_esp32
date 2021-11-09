#include "LedDriver.h"

#include <algorithm>

#include "Persistency.h"
#include "src/Utils/Logger.h"

namespace
{
constexpr uint16_t kDefaultSunraiseDurationMinutes{15};

// TODO: there are many different options on dimming functions:
//       https://blog.moonsindustries.com/2018/12/02/what-are-dimming-curves-and-how-to-choose/)
//       Most popular (should try):
//       1. logarithmyc
//       2. DALI logarithmic
//       3. Gamma

constexpr uint16_t brightness_levels[] = {
    1,   2,   4,   6,   9,   12,  16,  20,  25,  30,  36,  42,  49,  56,  64,  72,  81,  90,  100, 110, 121, 132,
    144, 156, 169, 182, 196, 210, 225, 240, 256, 272, 289, 306, 324, 342, 361, 380, 400, 420, 441, 462, 484, 506,
    529, 552, 576, 600, 625, 650, 676, 702, 729, 756, 784, 812, 841, 870, 900, 930, 961, 992, 992, 992};
constexpr uint8_t  kMaxNumOfBrightnessLevels{sizeof(brightness_levels) / sizeof(brightness_levels[0])};
constexpr uint16_t kMaxValueOfBrightnessLevel{992};

// For debugging.
// constexpr uint16_t brightness_levels[kMaxNumOfBrightnessLevels] = {
//     1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
//     21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
//     41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,
//     61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
//     81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100,
//     101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120};


// const uint8_t brightness_levels_gamma8_remapped[] = {
//     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
//     0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,
//     2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,   5,   6,
//     6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,  10,  10,  11,  11,  11,  12,  12,  13,
//     13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,
//     24,  25,  25,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  39,  39,
//     40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,  51,  52,  54,  55,  56,  57,  58,  59,  60,  61,
//     62,  63,  64,  66,  67,  68,  69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,
//     90,  92,  93,  95,  96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114, 115, 117, 119, 120, 122, 124,
//     126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167,
//     169, 171, 173, 175, 177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213, 215, 218,
//     220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255};

// constexpr uint8_t gammaCorrect[] = {
//     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02,
//     0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06,
//     0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x0C, 0x0C,
//     0x0C, 0x0D, 0x0D, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x10, 0x10, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13, 0x14, 0x14, 0x15,
//     0x16, 0x16, 0x17, 0x17, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1C, 0x1C, 0x1D, 0x1E, 0x1E, 0x1F, 0x20, 0x21, 0x21,
//     0x22, 0x23, 0x24, 0x24, 0x25, 0x26, 0x27, 0x28, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2E, 0x2F, 0x30, 0x31,
//     0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x43, 0x44, 0x45,
//     0x46, 0x47, 0x48, 0x49, 0x4B, 0x4C, 0x4D, 0x4E, 0x50, 0x51, 0x52, 0x53, 0x55, 0x56, 0x57, 0x59, 0x5A, 0x5B, 0x5D,
//     0x5E, 0x5F, 0x61, 0x62, 0x63, 0x65, 0x66, 0x68, 0x69, 0x6B, 0x6C, 0x6E, 0x6F, 0x71, 0x72, 0x74, 0x75, 0x77, 0x79,
//     0x7A, 0x7C, 0x7D, 0x7F, 0x81, 0x82, 0x84, 0x86, 0x87, 0x89, 0x8B, 0x8D, 0x8E, 0x90, 0x92, 0x94, 0x96, 0x97, 0x99,
//     0x9B, 0x9D, 0x9F, 0xA1, 0xA3, 0xA5, 0xA6, 0xA8, 0xAA, 0xAC, 0xAE, 0xB0, 0xB2, 0xB4, 0xB6, 0xB8, 0xBA, 0xBD, 0xBF,
//     0xC1, 0xC3, 0xC5, 0xC7, 0xC9, 0xCC, 0xCE, 0xD0, 0xD2, 0xD4, 0xD7, 0xD9, 0xDB, 0xDD, 0xE0, 0xE2, 0xE4, 0xE7, 0xE9,
//     0xEB, 0xEE, 0xF0, 0xF3, 0xF5, 0xF8, 0xFA, 0xFD, 0xFF};

}  // namespace

LedDriver::LedDriver(uint8_t pin, uint32_t pwm_frequency, uint32_t updating_period_ms)
  : pwm_{pin, pwm_frequency, kPwmResolutionBits}
  , initial_updating_period_ms_{updating_period_ms}
  , adjusted_updating_period_ms_{updating_period_ms}
  , is_sunrise_in_progress_{false}
  , sunrise_start_time_{0}
  , sunrise_duration_sec_{0}
  , current_brightness_{0}
  , thermal_factor_{1.0}
{
}

void
LedDriver::setup()
{
    pwm_.setup();

    if (!Persistency::instance().is_variable_stored(Persistency::kSunraiseDurationMinutes)) {
        DEBUG_PRINTLN(String{"Setting initial value of kSunraiseDurationMinutes to "} +
                      String{kDefaultSunraiseDurationMinutes});
        Persistency::instance().set_word(Persistency::kSunraiseDurationMinutes, kDefaultSunraiseDurationMinutes);
    }

    uint16_t duration_min{Persistency::instance().get_word(Persistency::kSunraiseDurationMinutes)};
    set_sunrise_duration(duration_min);
    set_brightness_manually(0.0);

    DEBUG_PRINTLN(String{"Read from Persistency: sunrise duration "} + String{duration_min} + " minutes");
}

void
LedDriver::run_sunrise()
{
    if (!is_sunrise_in_progress_) {
        return;
    }

    // Do not execute too often
    static unsigned long last_updating_time{0};
    auto                 now = millis();
    if ((now - last_updating_time) < adjusted_updating_period_ms_) {
        return;
    }
    last_updating_time = now;

    unsigned long delta_time_ms{(now - sunrise_start_time_)};
    if (delta_time_ms >= (sunrise_duration_sec_ * 1000)) {
        // Sunrise is finished
        set_brightness_manually(1.0);  // Keep lamp turned on
        return;
    }

    pwm_.set_duty(brightness_to_pwm_duty(thermal_factor_ * map_sunrise_time_to_level(delta_time_ms)));
}

void
LedDriver::start_sunrise()
{
    is_sunrise_in_progress_ = true;
    sunrise_start_time_     = millis();
}

void
LedDriver::stop_sunrise()
{
    is_sunrise_in_progress_ = false;
}

void
LedDriver::set_brightness_manually(float level)
{
    stop_sunrise();  // Manual control of brightness cancells sunrise
    uint16_t duty{brightness_to_pwm_duty(thermal_factor_ * map_manual_control_to_level(level))};

    // TODO: Check code on new HW on breadboard
    // BUG in HW: by some reason VOM1271 with open circuit gives only 1.2 V instead of 8.4V. It is not enough to
    // completely close MOP. Because of that on duty 100% output voltage is 2+ volts.
    // Also pay attention that max voltage, that ESP32 can give to input of VOM1271 is just 3.02V (GPIO max current is
    // 12 mA, but we are trying to get 10 mA). To solve it we will need one more MOP key at input of VOM1271 and one
    // move wire with +3.3 V. Question is how to mount that key on existing board/wire.

    String message{String{"LAMBIN level = "} + String{level} +
                   "; map_manual_control_to_level(level) = " + String{map_manual_control_to_level(level)} +
                   "; thermal_factor_ * map_manual_control_to_level(level) = " +
                   String{thermal_factor_ * map_manual_control_to_level(level)} +
                   "; current_brightness_ = " + String{current_brightness_, 2}};
    DEBUG_PRINTLN(message);


    pwm_.set_duty(duty);

    DEBUG_PRINTLN(String{"LAMBIN LedDriver::set_brightness_manually(): k = "} + String(thermal_factor_, 2) +
                  String("; scaled brightness = ") + String((thermal_factor_ * current_brightness_ * 100.0), 2));
    DEBUG_PRINTLN(String{"LAMBIN LedDriver::set_brightness_manually(): duty = "} + String{duty});
}

void
LedDriver::set_thermal_factor(float thermal_factor)
{
    thermal_factor  = constrain(thermal_factor, 0.0, 1.0);
    thermal_factor_ = thermal_factor;
    // Update brightness based on received thermal_factor
    pwm_.set_duty(brightness_to_pwm_duty(thermal_factor_ * current_brightness_));

    DEBUG_PRINTLN(String("LAMBIN LedDriver::set_thermal_factor(): k = ") + String(thermal_factor_, 2) +
                  String("; scaled brightness = ") + String((thermal_factor_ * current_brightness_ * 100.0), 2));
}

void
LedDriver::set_sunrise_duration(uint16_t duration_m)
{
    sunrise_duration_sec_ = (uint32_t)duration_m * 60;

    // Adjust updating period.
    // New sunrise duration can be so small that duration of each level will be less than initial updating period.
    // In this case updating will be very slow and we will miss some levels. To avoid it, we should set shorter updating
    // persiod. In this case each update we will switch to new level.
    unsigned long new_duration_of_each_level_ms{(sunrise_duration_sec_ * 1000) / kMaxNumOfBrightnessLevels};
    adjusted_updating_period_ms_ = std::min(initial_updating_period_ms_, new_duration_of_each_level_ms);
}

float
LedDriver::map_sunrise_time_to_level(uint32_t delta_time_ms)
{
    // Avoid overflow
    uint8_t index;
    if (delta_time_ms <= 0xFFFFFF) {
        // This will not cause overflow
        index = (delta_time_ms * kMaxNumOfBrightnessLevels) / (sunrise_duration_sec_ * 1000);
    }
    else {
        // Max delta is 24 hours = 24*60*60*1000. So, this code will not cause overflow
        index = ((delta_time_ms / 1000) * kMaxNumOfBrightnessLevels) / sunrise_duration_sec_;
    }

    if (index >= kMaxNumOfBrightnessLevels) {
        DEBUG_PRINTLN("ERROR: calculated index in brightness level table is out of range");
        current_brightness_ = 1.0;
        return current_brightness_;
    }

    current_brightness_ = static_cast<float>(brightness_levels[index]) / static_cast<float>(kMaxValueOfBrightnessLevel);

    // Return inverted PWM duty cycle, because current 100% duty cycle makes 0 ohm on DIM input of
    // LED driver, which corresponds to 0% brightness
    return current_brightness_;
}

float
LedDriver::map_manual_control_to_level(float manual_level)
{
    // manual_level is read from potentiometer. Usually dependency of potentiometer's resistance from rotation angle is
    // not lineral, so this function's goal is to provide mapping between real readings from potentiometer and LED
    // brightness level
    // In theory eyes perception curve lays above y=x, and resistance-angle curve lays below y=x. So, they should
    // compensate each other.
    // In practice - I don't see any difference between mapping functions.
    // Probably we don't need mapping here. User is setting brightness manually, so he will choose brightness as he
    // wants by changing angle of potentiometer.
    current_brightness_ = manual_level;

    // Return inverted PWM duty cycle, because current 100% duty cycle makes 0 ohm on DIM input of LED driver, which
    // corresponds to 0% brightness
    // return InvertLevel(manual_level >> 2);
    return current_brightness_;
}

uint16_t
LedDriver::brightness_to_pwm_duty(float level)
{
    constexpr uint16_t max_duty{(1 << kPwmResolutionBits) - 1};
    return static_cast<uint16_t>(max_duty * level);
}
