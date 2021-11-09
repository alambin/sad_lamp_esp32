#include "Thermosensors.hpp"

#include "src/Utils/Logger.h"

namespace
{
// Calibration data:
//
// 1st sensor (28B61675D0013CA2 - orange wires):
// 36.7 on medical thermometer -> 35.31 on sensor
// Boiling water (100) -> 98.75 (max measured), but water in boiling pan can have different temperatures in different
// areas!
constexpr DeviceAddress kSensorAddress1  = {0x28, 0xB6, 0x16, 0x75, 0xD0, 0x01, 0x3C, 0xA2};
constexpr float         kRawHigh1        = 98.75;
constexpr float         kReferenceHigh1  = 100.0;
constexpr float         kRawLow1         = 35.31;
constexpr float         kReferenceLow1   = 36.7;
constexpr float         kRawRange1       = kRawHigh1 - kRawLow1;
constexpr float         kReferenceRange1 = kReferenceHigh1 - kReferenceLow1;

// 2nd sensor (287B2275D0013CEC - blue wires):
// 36.8 on medical thermometer -> 36.47 on sensor
// Boiling water (100) -> 100.0 (max measured), but water in boiling pan can have different temperatures in different
// areas!
constexpr DeviceAddress kSensorAddress2  = {0x28, 0x7B, 0x22, 0x75, 0xD0, 0x01, 0x3C, 0xEC};
constexpr float         kRawHigh2        = 100.0;
constexpr float         kReferenceHigh2  = 100.0;
constexpr float         kRawLow2         = 36.47;
constexpr float         kReferenceLow2   = 36.8;
constexpr float         kRawRange2       = kRawHigh2 - kRawLow2;
constexpr float         kReferenceRange2 = kReferenceHigh2 - kReferenceLow2;

bool
are_sensor_addresses_equal(DeviceAddress const& l, DeviceAddress const& r)
{
    for (uint8_t i = 0; i < 8; ++i) {
        if (l[i] != pgm_read_byte(&r[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace

ThermoSensors::ThermoSensors(uint8_t pin)
  : pin_{pin}
  , oneWire_{}
  , sensors_{}
  , last_temperatures_{kInvalidTemperature, kInvalidTemperature}
{
}

void
ThermoSensors::setup()
{
    oneWire_.begin(pin_);
    sensors_.setOneWire(&oneWire_);
    sensors_.begin();

    DEBUG_PRINTLN(String{"Found "} + String{sensors_.getDeviceCount()} + " thermal sensors.");
    if (sensors_.getDeviceCount() < num_of_sensors_) {
        DEBUG_PRINTLN(String{"ERROR: expected number of sensors is "} + String{num_of_sensors_});
    }

    for (int i = 0; i < num_of_sensors_; ++i) {
        if (sensors_.getAddress(addresses_[i], i)) {
            sensors_.setResolution(addresses_[i], resolution_);
        }
        else {
            DEBUG_PRINTLN(String{"Unable to get address for Device "} + String{i});
        }
    }

    sensors_.setResolution(resolution_);
    sensors_.setWaitForConversion(false);
    sensors_.requestTemperatures();
}

void
ThermoSensors::loop()
{
    static uint32_t last_reading_time{0};
    auto            now = millis();
    if ((now - last_reading_time) < conversion_timeout_) {
        return;
    }
    last_reading_time = now;

    last_temperatures_[0] = convert_by_calibration(sensors_.getTempC(addresses_[0]), addresses_[0]);
    last_temperatures_[1] = convert_by_calibration(sensors_.getTempC(addresses_[1]), addresses_[1]);
    sensors_.requestTemperatures();
}

void
ThermoSensors::get_temperatures(float (&temperatures)[2]) const
{
    temperatures[0] = last_temperatures_[0];
    temperatures[1] = last_temperatures_[1];
}

float
ThermoSensors::convert_by_calibration(float T, DeviceAddress const& sensor_address) const
{
    if (are_sensor_addresses_equal(sensor_address, kSensorAddress1)) {
        return (((T - kRawLow1) * kReferenceRange1) / kRawRange1) + kReferenceLow1;
    }
    else if (are_sensor_addresses_equal(sensor_address, kSensorAddress2)) {
        return (((T - kRawLow2) * kReferenceRange2) / kRawRange2) + kReferenceLow2;
    }
    else {
        return T;
    }
}
