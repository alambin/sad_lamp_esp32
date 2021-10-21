#include "potentiometer.h"

#include <esp32-hal-gpio.h>
#include "src/Utils/Logger.h"

Potentiometer::Potentiometer(uint8_t pin, uint32_t sampling_ms)
  : pin_{pin}
  , sampling_ms_{sampling_ms}
  , current_sampe_index_{0}
  , running_average_value_{0}
  , running_average_int_value_{0}
  , current_value_{0}
{
    for (auto& sample : samples) {
        sample = 0;
    }
}

void
Potentiometer::setup()
{
    pinMode(pin_, ANALOG);
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
}

uint16_t
Potentiometer::read() const
{
    return current_value_;
}

uint16_t
Potentiometer::filter(uint16_t new_value)
{
    // print_debug(new_value);

    return (uint16_t)filter_running_average_adaptive(filter_median_3(new_value));
}

uint16_t
Potentiometer::filter_median_n(uint16_t new_value)
{
    samples[current_sampe_index_] = new_value;
    if ((current_sampe_index_ < samples_size_ - 1) &&
        (samples[current_sampe_index_] > samples[current_sampe_index_ + 1])) {
        for (int i = current_sampe_index_; i < samples_size_ - 1; ++i) {
            if (samples[i] > samples[i + 1]) {
                // Swap. Swapping via XOR is slowlier (tested on Arduino)
                uint16_t tmp   = samples[i];
                samples[i]     = samples[i + 1];
                samples[i + 1] = tmp;
            }
        }
    }
    else {
        if ((current_sampe_index_ > 0) && (samples[current_sampe_index_ - 1] > samples[current_sampe_index_])) {
            for (int i = current_sampe_index_; i > 0; --i) {
                if (samples[i] < samples[i - 1]) {
                    // Swap. Swapping via XOR is slowlier (tested on Arduino)
                    uint16_t tmp   = samples[i];
                    samples[i]     = samples[i - 1];
                    samples[i - 1] = tmp;
                }
            }
        }
    }
    if (++current_sampe_index_ >= samples_size_) {
        current_sampe_index_ = 0;
    }
    return samples[samples_size_ / 2];
}

// Fast version of median filter for 3 samples. Can give good results in combination with running average filter
uint16_t
Potentiometer::filter_median_3(uint16_t new_value)
{
    samples[current_sampe_index_] = new_value;
    if (++current_sampe_index_ >= 3) {
        current_sampe_index_ = 0;
    }

    uint16_t& a = samples[0];
    uint16_t& b = samples[1];
    uint16_t& c = samples[2];
    if ((a <= b) && (a <= c)) {
        return (b <= c) ? b : c;
    }
    else {
        if ((b <= a) && (b <= c)) {
            return (a <= c) ? a : c;
        }
        else {
            return (a <= b) ? a : b;
        }
    }
}

float
Potentiometer::filter_running_average(float new_value, float k)
{
    // Exponential running average
    running_average_value_ += (new_value - running_average_value_) * k;
    return running_average_value_;
}

// Int version is slightly slowlier, but looks the same precise.
// Performance: 8-10 times faster
// Memory: 5674/462 -> 5630/464. Surprisingly it requires 2 more bytes or RAM. But 44 less bytes of flash
int16_t
Potentiometer::filter_running_average_int(int16_t new_value)
{
    running_average_int_value_ += (new_value - running_average_int_value_) / 2;
    return running_average_int_value_;
}

// https://alexgyver.ru/lessons/filters/
// This filter better adapts to fast changes of value
float
Potentiometer::filter_running_average_adaptive(float new_value, float k_slow, float k_fast, float threshold)
{
    float k;
    // Speed of filter depends on absolute value of difference
    if (abs(new_value - running_average_value_) > threshold)
        k = k_fast;
    else
        k = k_slow;

    running_average_value_ += (new_value - running_average_value_) * k;
    return running_average_value_;
}

// Int version the same fast as float version. It looks like litle bit less precise. Ex. it is always 2-3 higher or
// lower than original signal or float filter value. Idk why. Sometimes it doesn't react on signal change.
// Ex. signal = 8, int_filter = 6, then signal = 4, int_filter = 6
// This is because of division on 10. Small changes will be invisible for int version, but anyway it is still quite
// precise
// Performance: 2.5-2.8 times faster
// Memory: 5746/470 -> 5760/472. Surprisingly it requires 2 more bytes or RAM. AND 16 more bytes of flash!
int16_t
Potentiometer::filter_running_average_adaptive_int(int16_t new_value, int8_t k_slow, int8_t k_fast, int8_t threshold)
{
    int8_t k;
    // Speed of filter depends on absolute value of difference
    if (abs(new_value - running_average_int_value_) > threshold)
        k = k_fast;
    else
        k = k_slow;

    running_average_int_value_ += ((new_value - running_average_int_value_) * k) / 10;
    return running_average_int_value_;
}

// void
// Potentiometer::print_debug(uint16_t new_value)
// {
//     volatile auto start_time                     = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      median_10gyver_val             = filter_median_n(new_value);
//     volatile auto median_10gyver_time            = portGET_RUN_TIME_COUNTER_VALUE() - start_time;
//     start_time                                   = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      median_3gyver_val              = filter_median_3(new_value);
//     volatile auto median_3gyver_time             = portGET_RUN_TIME_COUNTER_VALUE() - start_time;
//     start_time                                   = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      median_3gyver_avg_val          = (uint16_t)(filter_running_average(median_3gyver_val));
//     volatile auto median_3gyver_avg_time         = portGET_RUN_TIME_COUNTER_VALUE() - start_time;
//     start_time                                   = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      median_3gyver_avg_int_val      = filter_running_average_int(median_3gyver_val);
//     volatile auto median_3gyver_avg_int_time     = portGET_RUN_TIME_COUNTER_VALUE() - start_time;
//     start_time                                   = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      median_3gyver_avg_adp_val      = (uint16_t)(filter_running_average_adaptive(median_3gyver_val));
//     volatile auto median_3gyver_avg_adp_time     = portGET_RUN_TIME_COUNTER_VALUE() - start_time;
//     start_time                                   = portGET_RUN_TIME_COUNTER_VALUE();
//     uint16_t      median_3gyver_avg_adp_int_val  = filter_running_average_adaptive_int(median_3gyver_val);
//     volatile auto median_3gyver_avg_adp_int_time = portGET_RUN_TIME_COUNTER_VALUE() - start_time;

//     DEBUG_PRINT("NotFiltered:");
//     DEBUG_PRINT(new_value);
//     DEBUG_PRINT(", median_10gyver_val:");
//     DEBUG_PRINT(median_10gyver_val);
//     DEBUG_PRINT(", median_3gyver_val:");
//     DEBUG_PRINT(median_3gyver_val);
//     // DEBUG_PRINT(", median_3gyver_avg_val:");
//     // DEBUG_PRINT(median_3gyver_avg_val);
//     // DEBUG_PRINT(", median_3gyver_avg_int_val:");
//     // DEBUG_PRINT(median_3gyver_avg_int_val);
//     // DEBUG_PRINT(", median_3gyver_avg_adp_val:");
//     // DEBUG_PRINT(median_3gyver_avg_adp_val);
//     // DEBUG_PRINT(", median_3gyver_avg_adp_int_val:");
//     // DEBUG_PRINT(median_3gyver_avg_adp_int_val);
//     DEBUG_PRINTLN();

//     // Print performance
//     // DEBUG_PRINT("median_10gyver_time:");
//     // DEBUG_PRINT(median_10gyver_time);
//     // DEBUG_PRINT(", median_3gyver_time:");
//     // DEBUG_PRINT(median_3gyver_time);
//     // DEBUG_PRINTLN();

//     // Print avg performance
//     // static uint32_t runc_count = 0;
//     // ++runc_count;
//     // static uint32_t median_10gyver_average_time     = 0;
//     // static uint32_t median_3gyver_average_time = 0;
//     // static uint32_t median_3gyver_avg_average_time     = 0;
//     // static uint32_t median_3gyver_avg_int_average_time = 0;
//     // static uint32_t median_3gyver_avg_adp_average_time     = 0;
//     // static uint32_t median_3gyver_avg_adp_int_average_time = 0;

//     // static bool reset_made = false;
//     // if ((runc_count >= 100) || reset_made) {
//     //     if (!reset_made) {
//     //         runc_count = 1;
//     //         reset_made = true;
//     //     }

//     //     median_10gyver_average_time += median_10gyver_time;
//     //     median_3gyver_average_time += median_3gyver_time;
//     //     median_3gyver_avg_average_time += median_3gyver_avg_time;
//     //     median_3gyver_avg_int_average_time += median_3gyver_avg_int_time;
//     //     median_3gyver_avg_adp_average_time += median_3gyver_avg_adp_time;
//     //     median_3gyver_avg_adp_int_average_time += median_3gyver_avg_adp_int_time;
//     // }

//     // DEBUG_PRINT("median_10gyver_average_time(ns):");
//     // DEBUG_PRINT((median_10gyver_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", median_3gyver_average_time(ns):");
//     // DEBUG_PRINT((median_3gyver_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", median_3gyver_avg_average_time(ns):");
//     // DEBUG_PRINT((median_3gyver_avg_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", median_3gyver_avg_int_average_time(ns):");
//     // DEBUG_PRINT((median_3gyver_avg_int_average_time * 4) / runc_count);
//     // DEBUG_PRINT("median_3gyver_avg_adp_average_time(ns):");
//     // DEBUG_PRINT((median_3gyver_avg_adp_average_time * 4) / runc_count);
//     // DEBUG_PRINT(", median_3gyver_avg_adp_int_average_time(ns):");
//     // DEBUG_PRINT((median_3gyver_avg_adp_int_average_time * 4) / runc_count);
//     // DEBUG_PRINTLN();
// }

// Test results:
// 300 ms sampling.
// filter_median_n:
//      quite big latency, but avoids "noise". Weird results - never decreasing. No idea why.
//      Performance on ESP32: 375-380 ns
// filter_median_3:
//      significantly less latency. but follows noice of input signal.
//      Doesn't filter out most of jumps of input signal, only significant ones
//      Performance on ESP32: 203 ns
// filter_median_3 + filter_running_average:
//      Much less noise, than in filter_median_3. Filters out big jumps of filter_median_3.
//      Latency is bigger than for filter_median_3, but less than for filter_median_n
//      NOTE: you can change k parameter to control "smoothness" of filter
//      Performance of filter_running_average alone on ESP32: 92 ns
//      Performance of both filters together: 255 ns
// filter_median_3 + filter_running_average_adaptive:
//      Adaptive filter almost repeats non-adaptive version if signal doesn't change fast. In case of fast changes
//      adaptive has WAY less latency!
//      Performance of filter_running_average_adaptive alone on ESP32: 138 ns
//      Performance of both filters together: 303 ns
// filter_median_3 + filter_running_average_int vs filter_median_3 + filter_running_average:
//      Int version has just slightly bigger latency (ignorable). If there are no fast changes, then it almost repeats
//      float version. Sometimes result duffers just by "1".
//      Performance of filter_running_average_int alone on ESP32: 104 ns
//      Performance of both filters together: 248 ns
//      NOTE! Int version is SLOWLIER than float!
// filter_median_3 + filter_running_average_adaptive_int vs filter_median_3 + filter_running_average_adaptive:
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
// reasonable to choose filter giving the best quality (filter_median_3 + filter_running_average_adaptive).
