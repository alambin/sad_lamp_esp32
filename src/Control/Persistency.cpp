#include "Persistency.h"

#include "src/Utils/Logger.h"

namespace
{
// NOTE !!! KEYS MUST BE MAX 15 CHARS !!!
constexpr char kAlarmDowKey[]                = "AlarmDow";
constexpr char kAlarmMinutesKey[]            = "AlarmMinute";
constexpr char kAlarmHoursKey[]              = "AlarmHours";
constexpr char kIsAlarmOnKey[]               = "IsAlarmOn";
constexpr char kSunraiseDurationMinutesKey[] = "SunrDurMins";
constexpr char kFanPwmFrequencyKey[]         = "FanPwmFreq";
constexpr char kFanPwmStepsNumberKey[]       = "FanPwmStepsNum";
constexpr char kPotentiometerMinValKey[]     = "PotMinVal";
constexpr char kPotentiometerMaxValKey[]     = "PotMaxVal";

const char*
var_to_key(Persistency::Variable variable)
{
    switch (variable) {
    case Persistency::Variable::kAlarmDow:
        return kAlarmDowKey;
    case Persistency::Variable::kAlarmMinutes:
        return kAlarmMinutesKey;
    case Persistency::Variable::kAlarmHours:
        return kAlarmHoursKey;
    case Persistency::Variable::kIsAlarmOn:
        return kIsAlarmOnKey;
    case Persistency::Variable::kFanPwmStepsNumber:
        return kFanPwmStepsNumberKey;
    case Persistency::Variable::kSunraiseDurationMinutes:
        return kSunraiseDurationMinutesKey;
    case Persistency::Variable::kFanPwmFrequency:
        return kFanPwmFrequencyKey;
    case Persistency::Variable::kPotentiometerMinVal:
        return kPotentiometerMinValKey;
    case Persistency::Variable::kPotentiometerMaxVal:
        return kPotentiometerMaxValKey;
    default:
        return nullptr;
    }
    return nullptr;
}

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
    case kPotentiometerMinVal:
        return preferences.getUShort(kPotentiometerMinValKey);
    case kPotentiometerMaxVal:
        return preferences.getUShort(kPotentiometerMaxValKey);
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
    case kPotentiometerMinVal:
        preferences.putUShort(kPotentiometerMinValKey, value);
        break;
    case kPotentiometerMaxVal:
        preferences.putUShort(kPotentiometerMaxValKey, value);
        break;
    default:
        DEBUG_PRINTLN(String{"ERROR: can not write word for variable '"} + String{(int)variable} + "'");
        break;
    }
}

bool
Persistency::is_variable_stored(Variable variable)
{
    return preferences.isKey(var_to_key(variable));
}

void
Persistency::clear()
{
    preferences.clear();
}
