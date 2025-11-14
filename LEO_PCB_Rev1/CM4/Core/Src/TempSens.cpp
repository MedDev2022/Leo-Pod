#include "TempSens.hpp"
#include <cstring>
#include <cstdio>
#include "cmsis_os2.h"


// ---------- TUNE THESE IF YOUR VARIANT DIFFERS ----------
// Sensirion STS4x / SHT4x-style commands:
static const uint8_t CMD_MEASURE_HIGHREP[1] = { 0xFD }; // 0xFD, 1-byte in some docs; we send as 2 with 0x00
static const uint8_t CMD_SOFT_RESET[1]      = { 0x94 };
static const uint8_t CMD_READ_SERIAL[1]     = { 0x89 };
// Response formats assumed below: temp is 2 bytes + CRC (1 byte).
// --------------------------------------------------------

STS4L::STS4L(const Config& cfg) : cfg_(cfg) {}

HAL_StatusTypeDef STS4L::selectMux() {
//    if (lastMux_ == cfg_.muxChannel) return HAL_OK;
    uint8_t sel = (1u << (cfg_.muxChannel & 0x03));
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(
        cfg_.hi2c,
        static_cast<uint16_t>(cfg_.muxAddr7bit << 1),
        &sel, 1,
        cfg_.i2cTimeoutMs
   );
    if (st == HAL_OK) lastMux_ = cfg_.muxChannel;
    return st;
}

HAL_StatusTypeDef STS4L::softReset() {
    HAL_StatusTypeDef st = selectMux();
    if (st != HAL_OK) return st;

    uint8_t cmd = 0x94;
    st = HAL_I2C_Master_Transmit(cfg_.hi2c, uint16_t(cfg_.devAddr7bit << 1),
                                 &cmd, 1, cfg_.i2cTimeoutMs);
    osDelay(2);
    return st;
}

HAL_StatusTypeDef STS4L::readSerial(uint32_t& outSerial) {
    outSerial = 0;
    HAL_StatusTypeDef st = selectMux();
    if (st != HAL_OK) return st;

    uint8_t rx[6] = {0}; // Many Sensirion parts return 2bytes+CRC, 2bytes+CRC
    st = commandRead(CMD_READ_SERIAL, rx, sizeof(rx));
    if (st != HAL_OK) return st;

    // Validate CRCs
    if (crc8(&rx[0], 2) != rx[2]) return HAL_ERROR;
    if (crc8(&rx[3], 2) != rx[5]) return HAL_ERROR;

    outSerial = (static_cast<uint32_t>(rx[0]) << 24) |
                (static_cast<uint32_t>(rx[1]) << 16) |
                (static_cast<uint32_t>(rx[3]) << 8)  |
                (static_cast<uint32_t>(rx[4]) << 0);
    return HAL_OK;
}

HAL_StatusTypeDef STS4L::readTemperature(float& outTempC) {
    outTempC = 0.f;

    HAL_StatusTypeDef st = selectMux();
    if (st != HAL_OK) return st;

    // 1-byte command for STS4x high repeatability: 0xFD
    uint8_t cmd = 0xFD;
    st = HAL_I2C_Master_Transmit(
        cfg_.hi2c,
        static_cast<uint16_t>(cfg_.devAddr7bit << 1), // 7-bit addr
        &cmd, 1,
        cfg_.i2cTimeoutMs
    );
    if (st != HAL_OK) return st;

    // conversion time ~5–10ms; be safe
    osDelay(10);

    uint8_t rx[3] = {0};
    st = HAL_I2C_Master_Receive(
        cfg_.hi2c,
        static_cast<uint16_t>(cfg_.devAddr7bit << 1),
        rx, sizeof(rx),
        cfg_.i2cTimeoutMs
    );
    if (st != HAL_OK) return st;

    if (crc8(rx, 2) != rx[2]) return HAL_ERROR;

    uint16_t raw = (uint16_t(rx[0]) << 8) | rx[1];
    outTempC = rawToC(raw);
    return HAL_OK;
}

HAL_StatusTypeDef STS4L::commandRead(const uint8_t cmd[1], uint8_t* rx, uint16_t rxLen) {
    HAL_StatusTypeDef st = selectMux();
    if (st != HAL_OK) return st;

    st = HAL_I2C_Master_Transmit(
        cfg_.hi2c,
        static_cast<uint16_t>(cfg_.devAddr7bit << 1),
        const_cast<uint8_t*>(cmd), 1,
        cfg_.i2cTimeoutMs
    );
    if (st != HAL_OK) return st;

    // Some commands require small wait; keep conservative 2 ms
    HAL_Delay(2);

    return HAL_I2C_Master_Receive(
        cfg_.hi2c,
        static_cast<uint16_t>(cfg_.devAddr7bit << 1),
        rx, rxLen,
        cfg_.i2cTimeoutMs
    );
}

uint8_t STS4L::crc8(const uint8_t* data, uint16_t len) {
    uint8_t crc = 0xFF;          // init (Sensirion convention)
    const uint8_t poly = 0x31;   // x^8 + x^5 + x^4 + 1

    for (uint16_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            if (crc & 0x80) crc = (crc << 1) ^ poly;
            else            crc <<= 1;
        }
    }
    return crc;
}

float STS4L::rawToC(uint16_t raw) {
    // STS4x/SHT4x temperature conversion
    return -45.0f + (175.0f * (static_cast<float>(raw) / 65535.0f));
}
