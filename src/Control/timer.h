#ifndef SRC_CONTROL_TIMER_H_
#define SRC_CONTROL_TIMER_H_

#include <TimeLib.h>
#include <WString.h>

class Timer
{
public:
    class AlarmHandler
    {
    public:
        virtual void on_alarm() = 0;
    };

    enum class DaysOfWeek : uint8_t
    {
        kMonday    = 0b00000001,
        kTuesday   = 0b00000010,
        kWednesday = 0b00000100,
        kThursday  = 0b00001000,
        kFriday    = 0b00010000,
        kSaturday  = 0b00100000,
        kSunday    = 0b01000000,
        kEveryDay  = 0b01111111
    };

    explicit Timer(unsigned long reading_period_ms = 500);
    void setup();
    void check_alarm();

    void   set_alarm_str(const String& str);
    String get_alarm_str() const;
    bool   enable_alarm_str(const String& str);
    void   register_alarm_handler(AlarmHandler* alarm_handler);
    void   toggle_alarm();

    void   set_time_str(const String& str) const;
    String get_time_str() const;
    time_t get_time() const;

private:
    struct AlarmData
    {
        AlarmData();
        AlarmData(uint8_t h, uint8_t m, DaysOfWeek dw);
        AlarmData& operator=(const tmElements_t& time_elements);
        bool       operator==(const tmElements_t& time_elements) const;

        uint8_t    hour;
        uint8_t    minute;
        DaysOfWeek dow;
    };

    tmElements_t str_to_datetime(const String& str) const;
    AlarmData    str_to_alarm(const String& str) const;
    String       datetime_to_str(const tmElements_t& datetime) const;

    const unsigned long reading_period_ms_;
    AlarmData           alarm_;
    AlarmData           last_triggered_alarm_;
    bool                is_alarm_enabled_;
    AlarmHandler*       alarm_handler_;
};

#endif  // SRC_CONTROL_TIMER_H_
