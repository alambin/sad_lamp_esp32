#ifndef SRC_CONTROL_FILTER_H_
#define SRC_CONTROL_FILTER_H_

#include <stdint.h>

class Filter
{
public:
    using SamplesType = float;

    Filter(uint8_t samples_size);
    ~Filter();

    // Running median filters. Each new value is processed and result is returned based on N last values
    uint16_t filter_running_median_n(uint16_t new_value);
    uint16_t filter_running_median_3(uint16_t new_value);

    // Running average filters. Each new value is processed and result is returned based on N last values
    float filter_running_average(float new_value, float k = 0.5f);
    float filter_running_average_adaptive(float new_value,
                                          float k_slow    = 0.03f,
                                          float k_fast    = 0.9f,
                                          float threshold = 10.0f);
    // Int versions of running average filters. On Arduino they were faster, on ESP32 - not (sometimes even slowlier!)
    int16_t filter_running_average_int(int16_t new_value);
    int16_t filter_running_average_adaptive_int(int16_t new_value,
                                                int8_t  k_slow    = 3,
                                                int8_t  k_fast    = 9,
                                                int8_t  threshold = 10);

    // Median filter. Values are first added to filter and then, when it is full, we can read result. All calculations
    // are postponed till the filter is full
    // Return false in case of error
    bool filter_median_n_get_result(uint16_t* result);
    // Clear/ignore values, previously stored in filter
    void filter_median_n_clear();
    // Return true in case filter is full. If filter is fill, all new values are ignorred.
    bool filter_median_n_add_sample(uint16_t new_value);

private:
    const uint8_t samples_size_;
    SamplesType*  samples{nullptr};
    uint8_t       current_sampe_index_;
    float         running_average_value_;
    int16_t       running_average_int_value_;
};

#endif  // SRC_CONTROL_FILTER_H_
