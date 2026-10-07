#include "../include/bme280/Bme280.hpp"

#if !defined(BME280_32BIT_ENABLE)
#error "Bme280.cpp converts the Bosch 32-bit integer output; build with BME280_32BIT_ENABLE"
#endif

namespace bme280 {

namespace {

uint8_t toBosch(Oversampling osr)
{
    switch (osr) {
        case Oversampling::Skip: return BME280_NO_OVERSAMPLING;
        case Oversampling::x1:   return BME280_OVERSAMPLING_1X;
        case Oversampling::x2:   return BME280_OVERSAMPLING_2X;
        case Oversampling::x4:   return BME280_OVERSAMPLING_4X;
        case Oversampling::x8:   return BME280_OVERSAMPLING_8X;
        case Oversampling::x16:  return BME280_OVERSAMPLING_16X;
    }
    return BME280_OVERSAMPLING_1X;
}

uint8_t toBosch(Filter filter)
{
    switch (filter) {
        case Filter::Off:     return BME280_FILTER_COEFF_OFF;
        case Filter::Coeff2:  return BME280_FILTER_COEFF_2;
        case Filter::Coeff4:  return BME280_FILTER_COEFF_4;
        case Filter::Coeff8:  return BME280_FILTER_COEFF_8;
        case Filter::Coeff16: return BME280_FILTER_COEFF_16;
    }
    return BME280_FILTER_COEFF_OFF;
}

uint8_t toBosch(Standby standby)
{
    switch (standby) {
        case Standby::ms0_5:  return BME280_STANDBY_TIME_0_5_MS;
        case Standby::ms62_5: return BME280_STANDBY_TIME_62_5_MS;
        case Standby::ms125:  return BME280_STANDBY_TIME_125_MS;
        case Standby::ms250:  return BME280_STANDBY_TIME_250_MS;
        case Standby::ms500:  return BME280_STANDBY_TIME_500_MS;
        case Standby::ms1000: return BME280_STANDBY_TIME_1000_MS;
        case Standby::ms10:   return BME280_STANDBY_TIME_10_MS;
        case Standby::ms20:   return BME280_STANDBY_TIME_20_MS;
    }
    return BME280_STANDBY_TIME_1000_MS;
}

uint8_t toBosch(Mode mode)
{
    switch (mode) {
        case Mode::Sleep:  return BME280_POWERMODE_SLEEP;
        case Mode::Forced: return BME280_POWERMODE_FORCED;
        case Mode::Normal: return BME280_POWERMODE_NORMAL;
    }
    return BME280_POWERMODE_SLEEP;
}

} // namespace

// ---------------------------------------------------------------- bridge --
// The only place where C and C++ meet. Static members have no `this`, so
// they are valid C function pointers; `intf` carries this Bme280 back to us,
// which gives access to both the Bus and the Clock.

int8_t Bme280::readCb(uint8_t reg, uint8_t* data, uint32_t len, void* intf)
{
    auto* self = static_cast<Bme280*>(intf);
    return self->bus_.read(reg, data, len) ? BME280_OK : BME280_E_COMM_FAIL;
}

int8_t Bme280::writeCb(uint8_t reg, const uint8_t* data, uint32_t len, void* intf)
{
    auto* self = static_cast<Bme280*>(intf);
    return self->bus_.write(reg, data, len) ? BME280_OK : BME280_E_COMM_FAIL;
}

void Bme280::delayCb(uint32_t us, void* intf)
{
    static_cast<Bme280*>(intf)->clock_.delayUs(us);
}

// ----------------------------------------------------------- translation --

Error Bme280::toError(int8_t bosch_result)
{
    switch (bosch_result) {
        case BME280_OK:                 return Error::None;
        case BME280_E_COMM_FAIL:        return Error::BusFailure;
        case BME280_E_DEV_NOT_FOUND:    return Error::WrongChipId;
        case BME280_E_INVALID_LEN:      return Error::InvalidConfig;
        case BME280_E_SLEEP_MODE_FAIL:  return Error::InvalidConfig;
        default:                        return Error::Unknown;
    }
}

void Bme280::toBoschSettings(const Config& in, bme280_settings& out)
{
    out.osr_t        = toBosch(in.temperature);
    out.osr_p        = toBosch(in.pressure);
    out.osr_h        = toBosch(in.humidity);
    out.filter       = toBosch(in.filter);
    out.standby_time = toBosch(in.standby);
}

// ------------------------------------------------------------- lifecycle --

Bme280::Bme280(Bus& bus, Clock& clock) : bus_(bus), clock_(clock)
{
    dev_.intf     = BME280_I2C_INTF;
    dev_.intf_ptr = this;
    dev_.read     = &Bme280::readCb;
    dev_.write    = &Bme280::writeCb;
    dev_.delay_us = &Bme280::delayCb;
}

Error Bme280::init()
{
    return init(Config{});
}

Error Bme280::init(const Config& config)
{
    initialised_ = false;
    const Error err = toError(bme280_init(&dev_));   // chip ID, soft reset, calibration
    if (err != Error::None) {
        return err;
    }
    initialised_ = true;
    return configure(config);
}

Error Bme280::configure(const Config& config)
{
    if (!initialised_) {
        return Error::NotInitialised;
    }
    toBoschSettings(config, settings_);
    return toError(bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings_, &dev_));
}

Error Bme280::setMode(Mode mode)
{
    if (!initialised_) {
        return Error::NotInitialised;
    }
    return toError(bme280_set_sensor_mode(toBosch(mode), &dev_));
}

Error Bme280::readForced(Measurement& out)
{
    const Error err = setMode(Mode::Forced);
    if (err != Error::None) {
        return err;
    }
    clock_.delayUs(measurementTimeUs());
    return read(out);
}

Error Bme280::read(Measurement& out)
{
    if (!initialised_) {
        return Error::NotInitialised;
    }
    bme280_data data{};
    const Error err = toError(bme280_get_sensor_data(BME280_ALL, &data, &dev_));
    if (err != Error::None) {
        return err;
    }
    // BME280_32BIT_ENABLE units: temperature 0.01 degC, pressure Pa,
    // humidity 1/1024 %RH.
    out.temperatureC = static_cast<float>(data.temperature) / 100.0F;
    out.pressurePa   = static_cast<float>(data.pressure);
    out.humidityPct  = static_cast<float>(data.humidity) / 1024.0F;
    return Error::None;
}

uint32_t Bme280::measurementTimeUs() const
{
    uint32_t us = 0;
    bme280_cal_meas_delay(&us, &settings_);
    return us;
}

} // namespace bme280
