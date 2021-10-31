#include "Timer.h"

#include <DS1307RTC.h>

#include "Persistency.h"
#include "src/Utils/Logger.h"

namespace
{
Timer::DaysOfWeek
timelib_wday_to_dow(uint8_t c)
{
    switch (c) {
    case 1:
        return Timer::DaysOfWeek::kSunday;
    case 2:
        return Timer::DaysOfWeek::kMonday;
    case 3:
        return Timer::DaysOfWeek::kTuesday;
    case 4:
        return Timer::DaysOfWeek::kWednesday;
    case 5:
        return Timer::DaysOfWeek::kThursday;
    case 6:
        return Timer::DaysOfWeek::kFriday;
    case 7:
        return Timer::DaysOfWeek::kSaturday;
    default:
        return Timer::DaysOfWeek::kEveryDay;
    }
}
}  // namespace

Timer::Timer(unsigned long reading_period_ms)
  : reading_period_ms_{reading_period_ms}
  , last_triggered_alarm_{0xFF, 0xFF, DaysOfWeek::kEveryDay}
  , is_alarm_enabled_{false}
  , alarm_handler_{nullptr}
{
}

void
Timer::setup()
{
    // Rework this code as it is done in Potentiometer: consider 1st run, when these variables are not set.
    if (!Persistency::instance().is_variable_stored(Persistency::kAlarmHours)) {
        Persistency::instance().set_byte(Persistency::kAlarmHours, 0);
    }
    if (!Persistency::instance().is_variable_stored(Persistency::kAlarmMinutes)) {
        Persistency::instance().set_byte(Persistency::kAlarmMinutes, 0);
    }
    if (!Persistency::instance().is_variable_stored(Persistency::kAlarmDow)) {
        Persistency::instance().set_byte(Persistency::kAlarmDow, 0);
    }
    if (!Persistency::instance().is_variable_stored(Persistency::kIsAlarmOn)) {
        Persistency::instance().set_byte(Persistency::kIsAlarmOn, 0);
    }

    alarm_.hour       = Persistency::instance().get_byte(Persistency::kAlarmHours);
    alarm_.minute     = Persistency::instance().get_byte(Persistency::kAlarmMinutes);
    alarm_.dow        = static_cast<Timer::DaysOfWeek>(Persistency::instance().get_byte(Persistency::kAlarmDow));
    is_alarm_enabled_ = (Persistency::instance().get_byte(Persistency::kIsAlarmOn) == 1);

    String message{String{"Read from Persistency: alarm time "} + String{alarm_.hour} + ":" + String{alarm_.minute} +
                   " DoW= 0x" + String{(int)alarm_.dow, HEX} + ". Alarm is " +
                   ((is_alarm_enabled_ ? "enabled" : "disabled"))};
    DEBUG_PRINTLN(message);
}

// We are not using hardware alarm. Reason: there are only 2 alarms. They can be configured to trigger either on
// specified day of week, either on specified day of month. But in case of SAD Lamp we want alarm to trigger every day.
// The only option to do it is to check current hour and minute in ESP's main loop.
void
Timer::check_alarm()
{
    if ((!is_alarm_enabled_) || (alarm_handler_ == nullptr)) {
        return;
    }

    // Do not read from RTC on every iteration of loop(). Reading 2 times per second is quite safe.
    static unsigned long last_reading_time{0};
    auto                 now = millis();
    if ((now - last_reading_time) < reading_period_ms_) {
        return;
    }
    last_reading_time = now;

    tmElements_t datetime;
    if (!RTC.read(datetime)) {
        return;
    }

    bool does_dow_match{(uint8_t)alarm_.dow & (uint8_t)timelib_wday_to_dow(datetime.Wday)};
    if (does_dow_match && (datetime.Hour == alarm_.hour) && (datetime.Minute == alarm_.minute) &&
        (datetime.Second == 0)) {
        // Trigger alarm only once
        if (!(last_triggered_alarm_ == datetime)) {
            last_triggered_alarm_ = datetime;
            alarm_handler_->on_alarm();
        }
    }
}

void
Timer::register_alarm_handler(AlarmHandler* alarm_handler)
{
    alarm_handler_ = alarm_handler;
}

String
Timer::get_time_str() const
{
    tmElements_t datetime;
    String       result;
    if (RTC.read(datetime)) {
        result = datetime_to_str(datetime);
    }
    return result;
}

time_t
Timer::get_time() const
{
    return RTC.get();
}

void
Timer::set_time_str(const String& str) const
{
    DEBUG_PRINTLN(String{"Received command 'Set time' "} + str);
    auto datetime{str_to_datetime(str)};
    RTC.write(datetime);
}

void
Timer::set_alarm_str(const String& str)
{
    DEBUG_PRINTLN(String{"Received command 'Set alarm' "} + str);

    alarm_ = str_to_alarm(str);

    // Store new alarm value to Persistency
    Persistency::instance().set_byte(Persistency::kAlarmHours, alarm_.hour);
    Persistency::instance().set_byte(Persistency::kAlarmMinutes, alarm_.minute);
    Persistency::instance().set_byte(Persistency::kAlarmDow, (uint8_t)(alarm_.dow));

    String message{String{"Stored to Persistency alarm at "} + String{alarm_.hour} + ":" + String{alarm_.minute} +
                   " DoW= 0x" + String{(int)alarm_.dow, HEX}};
    DEBUG_PRINTLN(message);
}

String
Timer::get_alarm_str() const
{
    // E HH:MM WW
    char str[11];

    str[0] = (is_alarm_enabled_) ? 'E' : 'D';
    str[1] = ' ';

    // A terminating null character is automatically appended by snprintf
    snprintf_P(str + 2, 9, "%02d:%02d %02x", alarm_.hour, alarm_.minute, (uint8_t)alarm_.dow);
    return String(str);
}

bool
Timer::enable_alarm_str(const String& str)
{
    DEBUG_PRINTLN(String{"Received command 'Enable alarm' "} + str);

    if (str[0] == 'E') {
        if (!is_alarm_enabled_) {
            toggle_alarm();
        }
        return true;
    }
    else if (str[0] == 'D') {
        if (is_alarm_enabled_) {
            toggle_alarm();
        }
        return true;
    }
    return false;
}

void
Timer::toggle_alarm()
{
    if (is_alarm_enabled_) {
        is_alarm_enabled_ = false;
        Persistency::instance().set_byte(Persistency::kIsAlarmOn, 0);
    }
    else {
        is_alarm_enabled_ = true;
        Persistency::instance().set_byte(Persistency::kIsAlarmOn, 1);
    }

    DEBUG_PRINTLN(String{"Alarm is "} + (is_alarm_enabled_ ? "enabled" : "disabled"));
}

Timer::AlarmData::AlarmData()
  : AlarmData(0, 0, Timer::DaysOfWeek::kEveryDay)
{
}

Timer::AlarmData::AlarmData(uint8_t h, uint8_t m, DaysOfWeek dw)
  : hour{h}
  , minute{m}
  , dow{dw}
{
}

Timer::AlarmData&
Timer::AlarmData::operator=(const tmElements_t& time_elements)
{
    hour   = time_elements.Hour;
    minute = time_elements.Minute;
    // Do NOT take DoW into account
    return *this;
}

bool
Timer::AlarmData::operator==(const tmElements_t& time_elements) const
{
    // Do NOT take DoW into account
    return ((hour == time_elements.Hour) && (minute == time_elements.Minute));
}

tmElements_t
Timer::str_to_datetime(const String& str) const
{
    tmElements_t tm;
    // HH:MM:SS DD/MM/YYYY
    tm.Hour   = str.substring(0, 2).toInt();
    tm.Minute = str.substring(3, 5).toInt();
    tm.Second = str.substring(6, 8).toInt();
    tm.Day    = str.substring(9, 11).toInt();
    tm.Month  = str.substring(12, 14).toInt();
    tm.Year   = CalendarYrToTm(str.substring(15, 19).toInt());

    // We have to set week day manually, because DS1307RTC library doesn't do it.
    // We are doing this calculation by building time_t (seconds since 19700) from input data and by converting it
    // back to tmElements_t by breakTime() function. This function calculates week day.
    tmElements_t datetime;
    breakTime(makeTime(tm), datetime);

    return datetime;
}

Timer::AlarmData
Timer::str_to_alarm(const String& str) const
{
    // HH:MM
    uint8_t h = str.substring(0, 2).toInt();
    uint8_t m = str.substring(3, 5).toInt();

    DaysOfWeek dow = DaysOfWeek::kEveryDay;
    if (str.length() >= 7) {
        auto res = strtoul(str.substring(6, 8).c_str(), 0, 16);
        if (!((res == 0) || (res == ULONG_MAX))) {
            dow = static_cast<Timer::DaysOfWeek>(res & 0x7F);
        }
    }

    return Timer::AlarmData{h, m, dow};
}

String
Timer::datetime_to_str(const tmElements_t& datetime) const
{
    // H:MM:SS DD/MM/YYYY
    char str[20];
    // A terminating null character is automatically appended by snprintf
    snprintf_P(str,
               20,
               "%02d:%02d:%02d %02d/%02d/%04d",
               datetime.Hour,
               datetime.Minute,
               datetime.Second,
               datetime.Day,
               datetime.Month,
               tmYearToCalendar(datetime.Year));
    return String(str);
}
