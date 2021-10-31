#include "Filter.h"

#include <cmath>

#include <algorithm>

Filter::Filter(uint8_t samples_size)
  : samples_size_{samples_size}
  , current_sampe_index_{0}
  , running_average_value_{0}
  , running_average_int_value_{0}
{
    if (0 != samples_size_) {
        samples = new SamplesType[samples_size_];
        for (int i = 0; i < samples_size_ - 1; ++i) {
            samples[i] = 0;
        }
    }
}

Filter::~Filter()
{
    delete[] samples;
}

uint16_t
Filter::filter_running_median_n(uint16_t new_value)
{
    samples[current_sampe_index_] = new_value;
    if ((current_sampe_index_ < samples_size_ - 1) &&
        (samples[current_sampe_index_] > samples[current_sampe_index_ + 1])) {
        for (int i = current_sampe_index_; i < samples_size_ - 1; ++i) {
            if (samples[i] > samples[i + 1]) {
                // Swap. Swapping via XOR is slowlier (tested on Arduino)
                SamplesType tmp = samples[i];
                samples[i]      = samples[i + 1];
                samples[i + 1]  = tmp;
            }
            else {
                break;
            }
        }
    }
    else {
        if ((current_sampe_index_ > 0) && (samples[current_sampe_index_ - 1] > samples[current_sampe_index_])) {
            for (int i = current_sampe_index_; i > 0; --i) {
                if (samples[i] < samples[i - 1]) {
                    // Swap. Swapping via XOR is slowlier (tested on Arduino)
                    SamplesType tmp = samples[i];
                    samples[i]      = samples[i - 1];
                    samples[i - 1]  = tmp;
                }
                else {
                    break;
                }
            }
        }
    }
    if (++current_sampe_index_ >= samples_size_) {
        current_sampe_index_ = 0;
    }
    return samples[samples_size_ / 2];
}

// Fast version of running median filter for 3 samples. Can give good results in combination with running average filter
uint16_t
Filter::filter_running_median_3(uint16_t new_value)
{
    samples[current_sampe_index_] = new_value;
    if (++current_sampe_index_ >= 3) {
        current_sampe_index_ = 0;
    }

    SamplesType& a = samples[0];
    SamplesType& b = samples[1];
    SamplesType& c = samples[2];
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
Filter::filter_running_average(float new_value, float k)
{
    // Exponential running average
    running_average_value_ += (new_value - running_average_value_) * k;
    return running_average_value_;
}

int16_t
Filter::filter_running_average_int(int16_t new_value)
{
    running_average_int_value_ += (new_value - running_average_int_value_) / 2;
    return running_average_int_value_;
}

// https://alexgyver.ru/lessons/filters/
// This filter better adapts to fast changes of value
float
Filter::filter_running_average_adaptive(float new_value, float k_slow, float k_fast, float threshold)
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

int16_t
Filter::filter_running_average_adaptive_int(int16_t new_value, int8_t k_slow, int8_t k_fast, int8_t threshold)
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

bool
Filter::filter_median_n_get_result(uint16_t* result)
{
    if (current_sampe_index_ != samples_size_) {
        // Filter is not full
        return false;
    }

    auto median_position = samples + samples_size_ / 2;
    std::nth_element(samples, median_position, samples + samples_size_);
    *result = *median_position;
    return true;
}

void
Filter::filter_median_n_clear()
{
    // Do not perform zeroing, just ignore all content of filter
    current_sampe_index_ = 0;
}

bool
Filter::filter_median_n_add_sample(uint16_t new_value)
{
    if (current_sampe_index_ == samples_size_) {
        return true;
    }
    samples[current_sampe_index_++] = new_value;
    return (current_sampe_index_ == samples_size_);
}
