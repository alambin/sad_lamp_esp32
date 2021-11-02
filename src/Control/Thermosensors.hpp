#ifndef SRC_CONTROL_THERMOSENSORS_H_
#define SRC_CONTROL_THERMOSENSORS_H_

#include <stdint.h>

#include <DallasTemperature.h>
#include <OneWire.h>

// Asynchronously reads data from 2 thermal sensors. getTemperatures() returns results of last reading. Temperature is
// read once per conversion timeout (depends on sensor precision - see below).
//
// Following table is taken from datasheet: https://pdf1.alldatasheet.com/datasheet-pdf/view/58557/DALLAS/DS18B20.html
// Precision | Conversion timeout | Temperature precision
// 9  bit    | 93.75 ms (tconv/8) | 0.5
// 10 bit    | 187.5 ms (tconv/4) | 0.25
// 11 bit    | 375 ms (tconv/2)   | 0.125
// 12 bit    | 750 ms tconv       | 0.0625
class ThermoSensors
{
public:
    ThermoSensors(uint8_t pin);
    void setup();
    void loop();

    void get_temperatures(float (&temperatures)[2]) const;

    static constexpr float kInvalidTemperature{DEVICE_DISCONNECTED_C};

private:
    float convert_by_calibration(float T, DeviceAddress const& sensor_address) const;

    uint8_t                   pin_;
    OneWire                   oneWire_;
    DallasTemperature         sensors_;
    static constexpr uint8_t  num_of_sensors_{2};
    DeviceAddress             addresses_[num_of_sensors_];
    static constexpr uint8_t  resolution_{12};  // 9 bit - 0.5 degrees precision; 12 bit - 0.06 degrees
    static constexpr uint16_t conversion_timeout_{750 >> (12 - resolution_)};
    float                     last_temperatures_[num_of_sensors_];
};

#endif  // SRC_CONTROL_THERMOSENSORS_H_
