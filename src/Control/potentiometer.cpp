#include "Potentiometer.h"

#include <esp32-hal-gpio.h>
#include "Persistency.h"
#include "src/Utils/Logger.h"

namespace
{
constexpr uint8_t  kAutoCalibrationFilterNumOfSamples{50};
constexpr uint16_t kDefaultMinPotVal{625};   // 0.5V
constexpr uint16_t kDefaultMaxPotVal{3000};  // 2.4V
}  // namespace

Potentiometer::Potentiometer(uint8_t pin, uint32_t sampling_ms)
  : pin_{pin}
  , sampling_ms_{sampling_ms}
  , current_value_{0}
  , filter_{3}  // Median 3 filter
  , auto_calibration_filter_{kAutoCalibrationFilterNumOfSamples}
  , is_auto_calubration_in_progress_{false}
  , calibrated_min_val_{kDefaultMinPotVal}
  , calibrated_max_val_{kDefaultMaxPotVal}
{
}

void
Potentiometer::setup()
{
    pinMode(pin_, ANALOG);

    if (!Persistency::instance().is_variable_stored(Persistency::kPotentiometerMinVal)) {
        DEBUG_PRINTLN(String{"Setting initial value of kPotentiometerMinVal to"} + String{kDefaultMinPotVal});
        Persistency::instance().set_word(Persistency::kPotentiometerMinVal, kDefaultMinPotVal);
    }
    if (!Persistency::instance().is_variable_stored(Persistency::kPotentiometerMaxVal)) {
        DEBUG_PRINTLN(String{"Setting initial value of kPotentiometerMaxVal to"} + String{kDefaultMaxPotVal});
        Persistency::instance().set_word(Persistency::kPotentiometerMaxVal, kDefaultMaxPotVal);
    }

    calibrated_min_val_ = Persistency::instance().get_word(Persistency::kPotentiometerMinVal);
    calibrated_max_val_ = Persistency::instance().get_word(Persistency::kPotentiometerMaxVal);

    DEBUG_PRINTLN(String{"Read from Persistency: calibrated range: "} + String{calibrated_min_val_} + "-" +
                  String{calibrated_max_val_});
}

void
Potentiometer::loop()
{
    static unsigned long last_sampling_time{0};
    auto                 now = millis();
    if ((now - last_sampling_time) < sampling_ms_) {
        return;
    }
    last_sampling_time = now;

    current_value_ = filter(analogRead(pin_));
    auto_calibrate();
}

float
Potentiometer::read() const
{
    return map_to_calibrated_range();
}

uint16_t
Potentiometer::filter(uint16_t new_value)
{
    // print_debug(new_value);

    return (uint16_t)filter_.filter_running_average_adaptive(filter_.filter_running_median_3(new_value));
}

void
Potentiometer::auto_calibrate()
{
    // Calibrate based on filtered value (current_value_) to avoid reaction on noise at min/max raw values
    bool is_extreme_value{(current_value_ < calibrated_min_val_) || (current_value_ > calibrated_max_val_)};
    if (is_auto_calubration_in_progress_) {
        if (!is_extreme_value) {
            // Autocalibration should be interrupted because filter is not full and current value is not extreme (min
            // or max)
            is_auto_calubration_in_progress_ = false;
            auto_calibration_filter_.filter_median_n_clear();
            return;
        }

        // Autocalibration is in progress
        if (!auto_calibration_filter_.filter_median_n_add_sample(current_value_)) {
            // Filter is not full yet
            return;
        }

        // Filter is full. Get its value and stop autocalibration
        is_auto_calubration_in_progress_ = false;

        uint16_t median_value{0};
        auto_calibration_filter_.filter_median_n_get_result(&median_value);
        auto_calibration_filter_.filter_median_n_clear();
        bool should_update_min_val{current_value_ < calibrated_min_val_};
        (should_update_min_val ? calibrated_min_val_ : calibrated_max_val_) = median_value;
        Persistency::instance().set_word(
            should_update_min_val ? Persistency::kPotentiometerMinVal : Persistency::kPotentiometerMaxVal,
            median_value);
    }
    else {
        // Autocalibration was not started previously
        if (is_extreme_value) {
            // ... but should be started now
            is_auto_calubration_in_progress_ = true;
            auto_calibration_filter_.filter_median_n_clear();
            auto_calibration_filter_.filter_median_n_add_sample(current_value_);
        }
    }
}

float
Potentiometer::map_to_calibrated_range() const
{
    float mapped_value = (float)(current_value_ - calibrated_min_val_) / (calibrated_max_val_ - calibrated_min_val_);
    // During autocalibration it is possible that result is out of range [0.0; 1.0]
    mapped_value = (mapped_value < 0.0f) ? 0.0f : mapped_value;
    mapped_value = (mapped_value > 1.0f) ? 1.0f : mapped_value;
    return mapped_value;
}

// void
// Potentiometer::print_debug(uint16_t new_value)
// {
//     volatile auto start_time                     = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      running_median_10gyver_val             = filter_running_median_n(new_value);
//     volatile auto running_median_10gyver_time            = portGET_RUN_TIME_COUNTER_VALUE() - start_time;
//     start_time                                   = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      running_median_3gyver_val              = filter_running_median_3(new_value);
//     volatile auto running_median_3gyver_time             = portGET_RUN_TIME_COUNTER_VALUE() - start_time;
//     start_time                                   = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t     running_ median_3gyver_avg_val          =
//     (uint16_t)(filter_running_average(running_median_3gyver_val)); volatile auto running_median_3gyver_avg_time =
//     portGET_RUN_TIME_COUNTER_VALUE() - start_time; start_time                                   =
//     portGET_RUN_TIME_COUNTER_VALUE(); uint16_t      running_median_3gyver_avg_int_val      =
//     filter_running_average_int(running_median_3gyver_val); volatile auto running_median_3gyver_avg_int_time     =
//     portGET_RUN_TIME_COUNTER_VALUE() - start_time; start_time                                   =
//     portGET_RUN_TIME_COUNTER_VALUE(); uint16_t      running_median_3gyver_avg_adp_val      =
//     (uint16_t)(filter_running_average_adaptive(running_median_3gyver_val)); volatile auto
//     running_median_3gyver_avg_adp_time     = portGET_RUN_TIME_COUNTER_VALUE() - start_time; start_time =
//     portGET_RUN_TIME_COUNTER_VALUE(); uint16_t      running_median_3gyver_avg_adp_int_val  =
//     filter_running_average_adaptive_int(running_median_3gyver_val); volatile auto
//     running_median_3gyver_avg_adp_int_time = portGET_RUN_TIME_COUNTER_VALUE() - start_time;

//     DEBUG_PRINT("NotFiltered:");
//     DEBUG_PRINT(new_value);
//     DEBUG_PRINT(", running_median_10gyver_val:");
//     DEBUG_PRINTrunning_(median_10gyver_val);
//     DEBUG_PRINT(", running_median_3gyver_val:");
//     DEBUG_PRINT(running_median_3gyver_val);
//     // DEBUG_PRINT(", running_median_3gyver_avg_val:");
//     // DEBUG_PRINT(running_median_3gyver_avg_val);
//     // DEBUG_PRINT(", running_median_3gyver_avg_int_val:");
//     // DEBUG_PRINT(running_median_3gyver_avg_int_val);
//     // DEBUG_PRINT(", running_median_3gyver_avg_adp_val:");
//     // DEBUG_PRINT(running_median_3gyver_avg_adp_val);
//     // DEBUG_PRINT(", running_median_3gyver_avg_adp_int_val:");
//     // DEBUG_PRINT(running_median_3gyver_avg_adp_int_val);
//     DEBUG_PRINTLN();

//     // Print performance
//     // DEBUG_PRINT("running_median_10gyver_time:");
//     // DEBUG_PRINT(running_median_10gyver_time);
//     // DEBUG_PRINT(", running_median_3gyver_time:");
//     // DEBUG_PRINT(running_median_3gyver_time);
//     // DEBUG_PRINTLN();

//     // Print avg performance
//     // static uint32_t runc_count = 0;
//     // ++runc_count;
//     // static uint32_t running_median_10gyver_average_time     = 0;
//     // static uint32_t running_median_3gyver_average_time = 0;
//     // static uint32_t running_median_3gyver_avg_average_time     = 0;
//     // static uint32_t running_median_3gyver_avg_int_average_time = 0;
//     // static uint32_t running_median_3gyver_avg_adp_average_time     = 0;
//     // static uint32_t running_median_3gyver_avg_adp_int_average_time = 0;

//     // static bool reset_made = false;
//     // if ((runc_count >= 100) || reset_made) {
//     //     if (!reset_made) {
//     //         runc_count = 1;
//     //         reset_made = true;
//     //     }

//     //     running_median_10gyver_average_time += running_median_10gyver_time;
//     //     running_median_3gyver_average_time += running_median_3gyver_time;
//     //     running_median_3gyver_avg_average_time +=running_ median_3gyver_avg_time;
//     //     running_median_3gyver_avg_int_average_time += running_median_3gyver_avg_int_time;
//     //     running_median_3gyver_avg_adp_average_time += running_median_3gyver_avg_adp_time;
//     //     running_median_3gyver_avg_adp_int_average_time += running_median_3gyver_avg_adp_int_time;
//     // }

//     // DEBUG_PRINT("running_median_10gyver_average_time(ns):");
//     // DEBUG_PRINT((running_median_10gyver_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", running_median_3gyver_average_time(ns):");
//     // DEBUG_PRINT((running_median_3gyver_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", running_median_3gyver_avg_average_time(ns):");
//     // DEBUG_PRINT((running_median_3gyver_avg_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", running_median_3gyver_avg_int_average_time(ns):");
//     // DEBUG_PRINT((running_median_3gyver_avg_int_average_time * 4) / runc_count);
//     // DEBUG_PRINT("running_median_3gyver_avg_adp_average_time(ns):");
//     // DEBUG_PRINT((running_median_3gyver_avg_adp_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", running_median_3gyver_avg_adp_int_average_time(ns):");
//     // DEBUG_PRINT((running_median_3gyver_avg_adp_int_average_time * 4) / runc_count);
//     // DEBUG_PRINTLN();
// }

// Test results:
// 300 ms sampling.
// filter_running_median_n:
//      quite big latency, but avoids "noise". Weird results - never decreasing. Reason is that separated filter
//      instance should be used for each filter measure/test
//      Performance on ESP32: 375-380 ns
// filter_running_running_median_3:
//      significantly less latency. but follows noice of input signal.
//      Doesn't filter out most of jumps of input signal, only significant ones
//      Performance on ESP32: 203 ns
// filter_running_median_3 + filter_running_average:
//      Much less noise, than in filter_running_median_3. Filters out big jumps of filter_running_median_3.
//      Latency is bigger than for filter_running_median_3, but less than for filter_running_median_n
//      NOTE: you can change k parameter to control "smoothness" of filter
//      Performance of filter_running_average alone on ESP32: 92 ns
//      Performance of both filters together: 255 ns
// filter_running_median_3 + filter_running_average_adaptive:
//      Adaptive filter almost repeats non-adaptive version if signal doesn't change fast. In case of fast changes
//      adaptive has WAY less latency!
//      Performance of filter_running_average_adaptive alone on ESP32: 138 ns
//      Performance of both filters together: 303 ns
// filter_running_median_3 + filter_running_average_int vs filter_running_median_3 + filter_running_average:
//      Int version has just slightly bigger latency (ignorable). If there are no fast changes, then it almost repeats
//      float version. Sometimes result duffers just by "1".
//      Performance of filter_running_average_int alone on ESP32: 104 ns
//      Performance of both filters together: 248 ns
//      NOTE! Int version is SLOWLIER than float!
// filter_running_median_3 + filter_running_average_adaptive_int vs filter_running_median_3 +
// filter_running_average_adaptive:
//      Int version has exactly the same latency. Precision is nearly the same (difference is ignorable)
//      Performance of filter_running_average_adaptive_int alone on ESP32: 134
//      Performance of both filters together: 294
//      NOTE! Int version is nearly the same as float. Difference is just few nanoseconds (faster).
//
// NOTE! Performance measurement may be very inaccurate because it is based on CPU ticks. Every tick is about 4 ns.
//       But it is OK for comparison of filters performances with each other
//
// Performance conclusion. All filters perform quite on the same speed (refer to standalone measurements). Difference
// betwiin the slowliest and the fastests filters is just 50 ns, wich is almost nothing. Because of that it is
// reasonable to choose filter giving the best quality (filter_running_median_3 + filter_running_average_adaptive).


// Following variables and function currently are not used, but can be useful in future

// Command to see the REF_VOLTAGE, burned in eFuses: espefuse.py --port /dev/ttyUSB0 adc_info
// More precise method: redirect vRef to one of GPIOs by command adc2_vref_to_gpio(25) and measure it by voltmeter
// constexpr uint32_t kAdcRefVoltage{1070};

// struct AdcInfo
// {
//     adc_unit_t    adc_unit;
//     adc_channel_t adc_channel_num;
// };
// constexpr AdcInfo
// pin_to_adc_info(uint8_t pin)
// {
//     switch (pin) {
//     case ADC1_CHANNEL_0_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_0};
//     case ADC1_CHANNEL_1_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_1};
//     case ADC1_CHANNEL_2_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_2};
//     case ADC1_CHANNEL_3_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_3};
//     case ADC1_CHANNEL_4_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_4};
//     case ADC1_CHANNEL_5_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_5};
//     case ADC1_CHANNEL_6_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_6};
//     case ADC1_CHANNEL_7_GPIO_NUM:
//         return {ADC_UNIT_1, ADC_CHANNEL_7};

//     case ADC2_CHANNEL_0_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_0};
//     case ADC2_CHANNEL_1_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_1};
//     case ADC2_CHANNEL_2_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_2};
//     case ADC2_CHANNEL_3_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_3};
//     case ADC2_CHANNEL_4_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_4};
//     case ADC2_CHANNEL_5_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_5};
//     case ADC2_CHANNEL_6_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_6};
//     case ADC2_CHANNEL_7_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_7};
//     case ADC2_CHANNEL_8_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_8};
//     case ADC2_CHANNEL_9_GPIO_NUM:
//         return {ADC_UNIT_2, ADC_CHANNEL_9};

//     default:
//         return {ADC_UNIT_MAX, ADC_CHANNEL_MAX};
//     }
//     return {ADC_UNIT_MAX, ADC_CHANNEL_MAX};
// }
