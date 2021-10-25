#include "Pwm.h"

#include <esp32-hal-ledc.h>

namespace
{
static uint8_t kMaxUsedPwmChannel{0};
}  // namespace

Pwm::Pwm(uint8_t pin, uint32_t frequency, uint8_t resolution_bits)
  : channel_{kMaxUsedPwmChannel++}
  , pin_{pin}
  , frequency_{frequency}
  , resolution_bits_{resolution_bits}
{
}

void
Pwm::setup()
{
    ledcSetup(channel_, frequency_, resolution_bits_);
    ledcAttachPin(pin_, channel_);
}

void
Pwm::set_duty(uint32_t duty)
{
    ledcWrite(channel_, duty);
}
