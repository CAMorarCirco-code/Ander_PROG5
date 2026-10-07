// Prints one BME280 measurement per second over Serial.
// Wiring: VIN -> 3V3, GND -> GND, SDA -> SDA, SCL -> SCL.
// SDO open or to GND: address 0x76; SDO to 3V3: 0x77.

#include <Wire.h>
#include <Bme280Lib.h>

static bme280::ArduinoI2cBus bus(Wire, static_cast<uint8_t>(bme280::I2cAddress::Low));
static bme280::ArduinoClock sensorClock;
static bme280::Bme280 sensor(bus, sensorClock);

static const char* toString(bme280::Error e)
{
    switch (e) {
        case bme280::Error::None:           return "None";
        case bme280::Error::NotInitialised: return "NotInitialised";
        case bme280::Error::BusFailure:     return "BusFailure";
        case bme280::Error::WrongChipId:    return "WrongChipId";
        case bme280::Error::InvalidConfig:  return "InvalidConfig";
        default:                            return "Unknown";
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        // wait for USB serial on native-USB boards (SAMD21)
    }

    Wire.begin();   // the sketch owns the bus; the sensor only uses it

    const bme280::Error err = sensor.init();   // default Config: weather monitoring
    if (err != bme280::Error::None) {
        Serial.print("init failed: ");
        Serial.println(toString(err));
        while (true) {}
    }
    Serial.println("BME280 ready");
}

void loop()
{
    bme280::Measurement m;
    const bme280::Error err = sensor.readForced(m);
    if (err == bme280::Error::None) {
        Serial.print("T = ");
        Serial.print(m.temperatureC, 2);
        Serial.print(" C   p = ");
        Serial.print(m.pressurePa, 0);
        Serial.print(" Pa   RH = ");
        Serial.print(m.humidityPct, 1);
        Serial.println(" %");
    } else {
        Serial.print("read failed: ");
        Serial.println(toString(err));
    }
    delay(1000);
}
