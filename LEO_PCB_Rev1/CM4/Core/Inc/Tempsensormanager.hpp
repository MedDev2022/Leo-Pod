/*
 * Tempsensormanager.hpp
 *
 *  Created on: Oct 27, 2025
 *      Author: krinm
 */

#ifndef INC_TEMPSENSORMANAGER_HPP_
#define INC_TEMPSENSORMANAGER_HPP_


#pragma once
#include "TempSens.hpp"
#include <array>

/**
 * @brief Multi-channel temperature sensor manager
 *
 * Manages up to 4 STS4L temperature sensors connected through
 * an I2C multiplexer (PCA9546A). Each sensor is on a different
 * mux channel.
 */
class TempSensorManager {
public:
    static constexpr uint8_t MAX_SENSORS = 4;

    struct SensorStatus {
        bool isConnected = false;
        bool lastReadOK = false;
        float temperatureC = 0.0f;
        uint32_t serialNumber = 0;
        uint32_t successCount = 0;
        uint32_t failCount = 0;
    };

    /**
     * @brief Configuration for the sensor manager
     */
    struct Config {
        I2C_HandleTypeDef* hi2c;
        uint8_t muxAddr7bit = 0x70;      // PCA9546A default address
        uint8_t sensorAddr7bit = 0x44;   // STS4L default address (same for all)
        uint32_t i2cTimeoutMs = 20;
        bool autoDetect = true;          // Automatically detect connected sensors
    };

    explicit TempSensorManager(const Config& cfg);

    /**
     * @brief Initialize all sensors
     * Detects which channels have sensors and performs soft reset
     * @return Number of sensors detected
     */
    uint8_t init();

    /**
     * @brief Read temperature from a specific channel
     * @param channel Channel number (0-3)
     * @param outTempC Output temperature in Celsius
     * @return HAL_OK if successful
     */
    HAL_StatusTypeDef readChannel(uint8_t channel, float& outTempC);

    /**
     * @brief Read all connected sensors
     * Updates internal temperature array
     * @return Number of successful reads
     */
    uint8_t readAll();

    /**
     * @brief Get temperature from specific channel
     * @param channel Channel number (0-3)
     * @return Temperature in Celsius (0.0 if error or not connected)
     */
    float getTemperature(uint8_t channel) const;

    /**
     * @brief Get temperature as uint16_t (temp * 100)
     * Useful for sending over UART in integer format
     * @param channel Channel number (0-3)
     * @return Temperature * 100 (e.g., 25.50°C returns 2550)
     */
    uint16_t getTemperatureScaled(uint8_t channel) const;

    /**
     * @brief Check if a sensor is connected on a channel
     * @param channel Channel number (0-3)
     * @return true if sensor detected and responding
     */
    bool isConnected(uint8_t channel) const;

    /**
     * @brief Get status for a specific channel
     * @param channel Channel number (0-3)
     * @return SensorStatus struct
     */
    const SensorStatus& getStatus(uint8_t channel) const;

    /**
     * @brief Get all temperatures as array
     * @param outTemps Output array (must be at least MAX_SENSORS size)
     */
    void getAllTemperatures(float* outTemps) const;

    /**
     * @brief Get all temperatures scaled as uint16 array
     * @param outTemps Output array (must be at least MAX_SENSORS size)
     */
    void getAllTemperaturesScaled(uint16_t* outTemps) const;

    /**
     * @brief Print status of all sensors to console
     */
    void printStatus() const;

    /**
     * @brief Get number of connected sensors
     */
    uint8_t getConnectedCount() const { return connectedCount_; }

private:
    Config cfg_;
    std::array<STS4L*, MAX_SENSORS> sensors_;
    std::array<SensorStatus, MAX_SENSORS> status_;
    uint8_t connectedCount_ = 0;

    /**
     * @brief Detect if sensor is present on a channel
     */
    bool detectSensor(uint8_t channel);
};


#endif /* INC_TEMPSENSORMANAGER_HPP_ */
