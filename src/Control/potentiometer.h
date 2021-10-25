#ifndef SRC_CONTROL_POTENTIOMETER_H_
#define SRC_CONTROL_POTENTIOMETER_H_

#include <stdint.h>

class Potentiometer
{
public:
    Potentiometer(uint8_t pin, uint32_t sampling_ms);
    void     setup();
    void     loop();

    // 0-4095
    uint16_t read() const;

private:
    uint16_t filter(uint16_t new_value);

    // Different filters. You can experiment with settigns of each filter and with their combinations
    uint16_t filter_median_n(uint16_t new_value);
    uint16_t filter_median_3(uint16_t new_value);
    float    filter_running_average(float new_value, float k = 0.5f);
    float    filter_running_average_adaptive(float new_value,
                                             float k_slow    = 0.03f,
                                             float k_fast    = 0.9f,
                                             float threshold = 10.0f);
    // Int versions of RunningAverage. They are faster, but requires almost the same memory. More details in .cpp
    int16_t filter_running_average_int(int16_t new_value);
    int16_t filter_running_average_adaptive_int(int16_t new_value,
                                                int8_t  k_slow    = 3,
                                                int8_t  k_fast    = 9,
                                                int8_t  threshold = 10);

    const uint8_t       pin_;
    const unsigned long sampling_ms_;

    static constexpr uint8_t samples_size_ = 10;
    uint16_t                 samples[samples_size_];
    uint8_t                  current_sampe_index_;
    float                    running_average_value_;
    int16_t                  running_average_int_value_;
    uint16_t                 current_value_;

    // This is for debugging and confuguring filters
    // void print_debug(uint16_t new_value);
};

#endif  // SRC_CONTROL_POTENTIOMETER_H_
