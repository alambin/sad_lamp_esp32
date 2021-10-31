#ifndef SRC_CONTROL_PERSISTENCY_H_
#define SRC_CONTROL_PERSISTENCY_H_

#include <Preferences.h>

class Persistency
{
public:
    enum Variable : uint8_t
    {
        kAlarmDow,                 // 1 byte
        kAlarmMinutes,             // 1 byte
        kAlarmHours,               // 1 byte
        kIsAlarmOn,                // 1 byte
        kSunraiseDurationMinutes,  // 2 bytes
        kFanPwmFrequency,          // 2 bytes
        kFanPwmStepsNumber,        // 1 byte
        kPotentiometerMinVal,      // 2 bytes
        kPotentiometerMaxVal       // 2 bytes
    };

    Persistency(Persistency const&) = delete;
    Persistency(Persistency&&)      = delete;
    Persistency& operator=(Persistency const&) = delete;
    Persistency& operator=(Persistency&&) = delete;

    static Persistency& instance();

    uint8_t  get_byte(Variable variable);
    void     set_byte(Variable variable, uint8_t value);
    uint16_t get_word(Variable variable);
    void     set_word(Variable variable, uint16_t value);

    bool is_variable_stored(Variable variable);
    void clear();

private:
    Persistency();
    ~Persistency();

    Preferences preferences;
};

#endif  // SRC_CONTROL_PERSISTENCY_H_
