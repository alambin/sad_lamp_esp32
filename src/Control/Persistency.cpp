#include "Persistency.h"

#include "src/Utils/Logger.h"

namespace
{
constexpr char kAlarmDowKey[]                = "AlarmDow";
constexpr char kAlarmMinutesKey[]            = "AlarmMinute";
constexpr char kAlarmHoursKey[]              = "AlarmHours";
constexpr char kIsAlarmOnKey[]               = "IsAlarmOn";
constexpr char kFanPwmStepsNumberKey[]       = "FanPwmStepsNumber";
constexpr char kSunraiseDurationMinutesKey[] = "SunraiseDurationMinutes";
constexpr char kFanPwmFrequencyKey[]         = "FanPwmFrequency";
}  // namespace

Persistency&
Persistency::instance()
{
    static Persistency instance;
    return instance;
}

Persistency::Persistency()
{
    preferences.begin("SadLamp", false);
}

Persistency::~Persistency()
{
    preferences.end();
}

uint8_t
Persistency::get_byte(Variable variable)
{
    switch (variable) {
    case kAlarmDow:
        return preferences.getUChar(kAlarmDowKey);
    case kAlarmMinutes:
        return preferences.getUChar(kAlarmMinutesKey);
    case kAlarmHours:
        return preferences.getUChar(kAlarmHoursKey);
    case kIsAlarmOn:
        return preferences.getUChar(kIsAlarmOnKey);
    case kFanPwmStepsNumber:
        return preferences.getUChar(kFanPwmStepsNumberKey);
    default:
        DEBUG_PRINTLN(String{"ERROR: can not read byte for variable '"} + String{(int)variable} + "'");
        return 0;
    }
}

void
Persistency::set_byte(Variable variable, uint8_t value)
{
    switch (variable) {
    case kAlarmDow:
        preferences.putUChar(kAlarmDowKey, value);
        break;
    case kAlarmMinutes:
        preferences.putUChar(kAlarmMinutesKey, value);
        break;
    case kAlarmHours:
        preferences.putUChar(kAlarmHoursKey, value);
        break;
    case kIsAlarmOn:
        preferences.putUChar(kIsAlarmOnKey, value);
        break;
    case kFanPwmStepsNumber:
        preferences.putUChar(kFanPwmStepsNumberKey, value);
        break;
    default:
        DEBUG_PRINTLN(String{"ERROR: can not write byte for variable '"} + String{(int)variable} + "'");
        break;
    }
}

uint16_t
Persistency::get_word(Variable variable)
{
    switch (variable) {
    case kSunraiseDurationMinutes:
        return preferences.getUShort(kSunraiseDurationMinutesKey);
    case kFanPwmFrequency:
        return preferences.getUShort(kFanPwmFrequencyKey);
    default:
        DEBUG_PRINTLN(String{"ERROR: can not read word for variable '"} + String{(int)variable} + "'");
        return 0;
    }
}

void
Persistency::set_word(Variable variable, uint16_t value)
{
    switch (variable) {
    case kSunraiseDurationMinutes:
        preferences.putUShort(kSunraiseDurationMinutesKey, value);
        break;
    case kFanPwmFrequency:
        preferences.putUShort(kFanPwmFrequencyKey, value);
        break;
    default:
        DEBUG_PRINTLN(String{"ERROR: can not write word for variable '"} + String{(int)variable} + "'");
        break;
    }
}
