#pragma once
#include <cstdint>
#include "main.h"

class STS4L {
public:
    struct Config {
        I2C_HandleTypeDef* hi2c;
        uint8_t            muxAddr7bit = 0x70; // PCA9546A default
        uint8_t            muxChannel  = 0;    // 0..3
        uint8_t            devAddr7bit;        // sensor I2C 7-bit address
        uint32_t           i2cTimeoutMs = 20;
    };

    explicit STS4L(const Config& cfg);

    // Select mux channel (no-op if already selected)
    HAL_StatusTypeDef selectMux();

    // Soft reset (sensor-specific command; see .cpp)
    HAL_StatusTypeDef softReset();

    // Read serial number (sensor-specific; fills out param if supported)
    HAL_StatusTypeDef readSerial(uint32_t& outSerial);

    // Single-shot high-repeatability measurement (blocking).
    // Returns HAL_OK and fills out temperature in °C on success.
    HAL_StatusTypeDef readTemperature(float& outTempC);

    // Low-level helper: write two-byte command then read N bytes
    HAL_StatusTypeDef commandRead(const uint8_t cmd[2], uint8_t* rx, uint16_t rxLen);

private:
    Config cfg_;
    int    lastMux_ = -1;

    // CRC-8 (poly 0x31) used by STS/SHT families
    static uint8_t crc8(const uint8_t* data, uint16_t len);

    // Convert raw 16b to °C (STS4x/SHT4x formula). Adjust if your variant differs.
    static float rawToC(uint16_t raw);
};
