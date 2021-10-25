#ifndef SRC_CONTROL_PWM_H_
#define SRC_CONTROL_PWM_H_

#include <stdint.h>

class Pwm
{
public:
    Pwm(uint8_t pin, uint32_t frequency, uint8_t resolution_bits);
    void setup();
    void set_duty(uint32_t duty);  // duti is between 0 and 2^resolution_bits

private:
    const uint8_t  channel_;
    const uint8_t  pin_;
    const uint32_t frequency_;
    const uint8_t  resolution_bits_;
};

#endif  // SRC_CONTROL_PWM_H_
