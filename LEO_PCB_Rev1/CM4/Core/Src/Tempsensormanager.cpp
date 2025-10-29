/*
 * Tempsensormanager.cpp
 *
 *  Created on: Oct 27, 2025
 *      Author: krinm
 */

#include "TempSensorManager.hpp"
#include <cstdio>
#include "cmsis_os2.h"

TempSensorManager::TempSensorManager(const Config& cfg) : cfg_(cfg) {
    // Initialize all sensor pointers to nullptr
    for (auto& sensor : sensors_) {
        sensor = nullptr;
    }
}

uint8_t TempSensorManager::init() {
    printf("TempSensorManager: Initializing...\r\n");
    connectedCount_ = 0;

    // Scan all 4 channels for sensors
    for (uint8_t ch = 0; ch < MAX_SENSORS; ++ch) {
        printf("  Channel %u: ", ch);

        if (detectSensor(ch)) {
            // Create sensor configuration for this channel
            STS4L::Config sensorCfg = {
                .hi2c = cfg_.hi2c,
                .muxAddr7bit = cfg_.muxAddr7bit,
                .muxChannel = ch,
                .devAddr7bit = cfg_.sensorAddr7bit,
                .i2cTimeoutMs = cfg_.i2cTimeoutMs
            };

            // Create sensor instance
            sensors_[ch] = new STS4L(sensorCfg);

            if (sensors_[ch] != nullptr) {
                // Soft reset the sensor
                osDelay(10);
                HAL_StatusTypeDef st = sensors_[ch]->softReset();
                osDelay(2);

                if (st == HAL_OK) {
                    // Try to read serial number
                    uint32_t serial = 0;
                    if (sensors_[ch]->readSerial(serial) == HAL_OK) {
                        status_[ch].serialNumber = serial;
                        printf("STS4L detected! Serial: 0x%08lX\r\n", serial);
                    } else {
                        printf("STS4L detected (serial read failed)\r\n");
                    }

                    status_[ch].isConnected = true;
                    connectedCount_++;
                } else {
                    printf("Sensor detected but reset failed\r\n");
                    delete sensors_[ch];
                    sensors_[ch] = nullptr;
                }
            } else {
                printf("Memory allocation failed\r\n");
            }
        } else {
            printf("No sensor\r\n");
            sensors_[ch] = nullptr;
            status_[ch].isConnected = false;
        }
    }

    printf("TempSensorManager: Found %u sensor(s)\r\n", connectedCount_);
    return connectedCount_;
}

bool TempSensorManager::detectSensor(uint8_t channel) {
    if (channel >= MAX_SENSORS) return false;

    // Select mux channel
    uint8_t sel = (1u << (channel & 0x03));
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(
        cfg_.hi2c,
        static_cast<uint16_t>(cfg_.muxAddr7bit << 1),
        &sel, 1,
        cfg_.i2cTimeoutMs
    );

    if (st != HAL_OK) return false;

    osDelay(1);

    // Try to communicate with sensor
    st = HAL_I2C_IsDeviceReady(
        cfg_.hi2c,
        static_cast<uint16_t>(cfg_.sensorAddr7bit << 1),
        3,  // 3 trials
        cfg_.i2cTimeoutMs
    );

    return (st == HAL_OK);
}

HAL_StatusTypeDef TempSensorManager::readChannel(uint8_t channel, float& outTempC) {
    outTempC = 0.0f;

    // Validate channel
    if (channel >= MAX_SENSORS) {
        return HAL_ERROR;
    }

    // Check if sensor exists
    if (sensors_[channel] == nullptr || !status_[channel].isConnected) {
        return HAL_ERROR;
    }

    // Read temperature
    HAL_StatusTypeDef st = sensors_[channel]->readTemperature(outTempC);

    // Update status
    if (st == HAL_OK) {
        status_[channel].temperatureC = outTempC;
        status_[channel].lastReadOK = true;
        status_[channel].successCount++;
    } else {
        status_[channel].lastReadOK = false;
        status_[channel].failCount++;
    }

    return st;
}

uint8_t TempSensorManager::readAll() {
    uint8_t successCount = 0;

    for (uint8_t ch = 0; ch < MAX_SENSORS; ++ch) {
        if (sensors_[ch] != nullptr && status_[ch].isConnected) {
            float tempC = 0.0f;
            if (readChannel(ch, tempC) == HAL_OK) {
                successCount++;
            }
        }
    }

    return successCount;
}

float TempSensorManager::getTemperature(uint8_t channel) const {
    if (channel >= MAX_SENSORS) return 0.0f;
    return status_[channel].temperatureC;
}

uint16_t TempSensorManager::getTemperatureScaled(uint8_t channel) const {
    if (channel >= MAX_SENSORS) return 0;

    // Convert to scaled integer (temp * 100)
    // 25.50°C becomes 2550
    float temp = status_[channel].temperatureC;

    // Clamp to reasonable range to prevent overflow
    if (temp < -100.0f) temp = -100.0f;
    if (temp > 200.0f) temp = 200.0f;

    int32_t scaled = static_cast<int32_t>(temp * 100.0f);

    // Return as uint16_t (will wrap if negative, but that's ok for protocol)
    return static_cast<uint16_t>(scaled);
}

bool TempSensorManager::isConnected(uint8_t channel) const {
    if (channel >= MAX_SENSORS) return false;
    return status_[channel].isConnected;
}

const TempSensorManager::SensorStatus& TempSensorManager::getStatus(uint8_t channel) const {
    static const SensorStatus emptyStatus;
    if (channel >= MAX_SENSORS) return emptyStatus;
    return status_[channel];
}

void TempSensorManager::getAllTemperatures(float* outTemps) const {
    for (uint8_t ch = 0; ch < MAX_SENSORS; ++ch) {
        outTemps[ch] = status_[ch].temperatureC;
    }
}

void TempSensorManager::getAllTemperaturesScaled(uint16_t* outTemps) const {
    for (uint8_t ch = 0; ch < MAX_SENSORS; ++ch) {
        outTemps[ch] = getTemperatureScaled(ch);
    }
}

void TempSensorManager::printStatus() const {
    printf("\r\n=== Temperature Sensor Status ===\r\n");
    printf("Connected sensors: %u/%u\r\n\r\n", connectedCount_, MAX_SENSORS);

    for (uint8_t ch = 0; ch < MAX_SENSORS; ++ch) {
        printf("Channel %u: ", ch);

        if (status_[ch].isConnected) {
            printf(" Connected\r\n");
            printf("  Serial: 0x%08lX\r\n", status_[ch].serialNumber);
            printf("  Temp:   %.2f°C (scaled: %u)\r\n",
                   status_[ch].temperatureC,
                   getTemperatureScaled(ch));
            printf("  Last:   %s\r\n", status_[ch].lastReadOK ? "OK" : "FAIL");
            printf("  Stats:  %lu success, %lu fail\r\n",
                   status_[ch].successCount,
                   status_[ch].failCount);
        } else {
            printf(" Not connected\r\n");
        }
        printf("\r\n");
    }
    printf("================================\r\n\r\n");
}


