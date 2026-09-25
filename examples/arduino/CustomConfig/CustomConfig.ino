// Same as ReadEverySecond, but overrides one field of the default Config:
// pressure oversampling x16 for a less noisy pressure reading.

#include <Wire.h>
#include <Bme280Lib.h>

static bme280::ArduinoI2cBus bus(Wire, static_cast<uint8_t>(bme280::I2cAddress::Low));
static bme280::Bme280 sensor(bus);

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    Wire.begin();

    bme280::Config config;                                // weather-monitoring defaults...
    config.pressure = bme280::Oversampling::x16;          // ...except this one field
    if (sensor.init(config) != bme280::Error::None) {
        Serial.println("init failed");
        while (true) {}
    }
    Serial.print("BME280 ready, one measurement takes ");
    Serial.print(sensor.measurementTimeUs());
    Serial.println(" us");
}

void loop()
{
    bme280::Measurement m;
    if (sensor.readForced(m) == bme280::Error::None) {
        Serial.print("T = ");
        Serial.print(m.temperatureC, 2);
        Serial.print(" C   p = ");
        Serial.print(m.pressurePa, 0);
        Serial.print(" Pa   RH = ");
        Serial.print(m.humidityPct, 1);
        Serial.println(" %");
    } else {
        Serial.println("read failed");
    }
    delay(1000);
}
