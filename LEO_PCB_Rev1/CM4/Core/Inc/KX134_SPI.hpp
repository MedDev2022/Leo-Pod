/*
 * KX134.h
 *
 *  Created on: Apr 19, 2024
 *      Author: krinm
 */

#ifndef SRC_KX134_H_
#define SRC_KX134_H_

#pragma once
#include "stm32h7xx_hal.h"
#include <cstdint>
#include <cstddef>
#include "main.h"

class KX134_SPI {
public:
    struct Config {
        SPI_HandleTypeDef* hspi;     // e.g., &hspi2
        uint32_t           timeout_ms = 10;
    };

    // KX134 supports ±8/±16/±32/±64 g
    enum Range : uint8_t { RANGE_8G=0, RANGE_16G=1, RANGE_32G=2, RANGE_64G=3 };

    explicit KX134_SPI(const Config& c);

    // Device ID (expected 0x46 for KX134-1211; verify on your chip)
    HAL_StatusTypeDef whoAmI(uint8_t& id);

    // Configure range + ODR.
    // NOTE: odr_code mapping depends on datasheet; 0x03 ~100 Hz on many KX13x.
    HAL_StatusTypeDef init(Range range = RANGE_16G, uint8_t odr_code = 0x03);

    // Raw 16-bit acceleration (2’s complement), little-endian L/H pairs
    HAL_StatusTypeDef readRaw(int16_t& x, int16_t& y, int16_t& z);

    // Convert to g using current sensitivity (counts per g)
    void setSensitivityLSBperG(float lsbPerG) { lsbPerG_ = lsbPerG; }
    void countsToG(int16_t x, int16_t y, int16_t z, float& gx, float& gy, float& gz) const;

    // Optional: enable data-ready on INT1 (polarity/drive configurable)
    // Verify INCx bit definitions with your datasheet revision.
    HAL_StatusTypeDef enableDataReadyINT1(bool activeHigh = true, bool pushPull = true);

private:
    Config cfg_;
    float  lsbPerG_ = 2048.0f; // placeholder for ±16g (tune per datasheet)

    // ---- Register map (KX13x family—verify) ----
    static constexpr uint8_t REG_XOUT_L   = 0x08;
    static constexpr uint8_t REG_XOUT_H   = 0x09;
    static constexpr uint8_t REG_YOUT_L   = 0x0A;
    static constexpr uint8_t REG_YOUT_H   = 0x0B;
    static constexpr uint8_t REG_ZOUT_L   = 0x0C;
    static constexpr uint8_t REG_ZOUT_H   = 0x0D;


    static constexpr uint8_t REG_WHO_AM_I = 0x00; // expect 0x46 on KX134
    static constexpr uint8_t REG_CNTL1    = 0x1B; // PC1 (bit7), GSEL bits [3:2]
    static constexpr uint8_t REG_CNTL2    = 0x1C; // (reserved/features)
    static constexpr uint8_t REG_CNTL3    = 0x1D; // ODR Output Data Rate
    static constexpr uint8_t REG_CNTL4    = 0x1E; //
    static constexpr uint8_t REG_CNTL5    = 0x1F; //
    static constexpr uint8_t REG_CNTL6    = 0x20; //
    static constexpr uint8_t REG_ODCNTL   = 0x21; // ODR/filter
    static constexpr uint8_t REG_INC1     = 0x22; //
    static constexpr uint8_t REG_INC2     = 0x23; //
    static constexpr uint8_t REG_INC3 	  = 0x24;
    static constexpr uint8_t REG_INC4 		= 0x25;
    static constexpr uint8_t REG_INC5 		= 0x26;
    static constexpr uint8_t REG_INC6 = 0x27;
	static constexpr uint8_t REG_TILT_TIMER = 0x29;
	static constexpr uint8_t REG_TDTRC = 0x2A;
	static constexpr uint8_t REG_TDTC = 0x2B;
	static constexpr uint8_t REG_TTH = 0x2C;
	static constexpr uint8_t REG_TTL = 0x2D;
	static constexpr uint8_t REG_FTD = 0x2E;
	static constexpr uint8_t REG_STD = 0x2F;
	static constexpr uint8_t REG_TLT = 0x30;
	static constexpr uint8_t REG_TWS = 0x31;
	static constexpr uint8_t REG_FFTH = 0x32;
	static constexpr uint8_t REG_FFC = 0x33;
	static constexpr uint8_t REG_FFCNTL = 0x34;
	static constexpr uint8_t REG_TILT_ANGLE_LL = 0x37;
	static constexpr uint8_t REG_TILT_ANGLE_HL = 0x38;
	static constexpr uint8_t REG_HYST_SET = 0x39;
	static constexpr uint8_t REG_LP_CNTL1 = 0x3A;
	static constexpr uint8_t REG_LP_CNTL2 = 0x3B;
	static constexpr uint8_t REG_WUFTH = 0x49;
	static constexpr uint8_t REG_BTSWUFTH = 0x4A;
	static constexpr uint8_t REG_BTSTH = 0x4B;
	static constexpr uint8_t REG_BTSC = 0x4C;
	static constexpr uint8_t REG_WUFC = 0x4D;
	static constexpr uint8_t REG_SELF_TEST = 0x5D;
	static constexpr uint8_t REG_BUF_CNTL1 = 0x5E;
	static constexpr uint8_t REG_BUF_CNTL2 = 0x5F;
	static constexpr uint8_t REG_BUF_STATUS_1 = 0x60;
	static constexpr uint8_t REG_BUF_STATUS_2 = 0x61;
	static constexpr uint8_t REG_BUF_CLEAR = 0x62;
	static constexpr uint8_t REG_BUF_READ = 0x63;
	static constexpr uint8_t REG_ADP_CNTL1 = 0x64;
	static constexpr uint8_t REG_ADP_CNTL2 = 0x65;
	static constexpr uint8_t REG_ADP_CNTL3 = 0x66;
	static constexpr uint8_t REG_ADP_CNTL4 = 0x67;
	static constexpr uint8_t REG_ADP_CNTL5 = 0x68;
	static constexpr uint8_t REG_ADP_CNTL6 = 0x69;
	static constexpr uint8_t REG_ADP_CNTL7 = 0x6A;
	static constexpr uint8_t REG_ADP_CNTL8 = 0x6B;
	static constexpr uint8_t REG_ADP_CNTL9 = 0x6C;
	static constexpr uint8_t REG_ADP_CNTL10 = 0x6D;
	static constexpr uint8_t REG_ADP_CNTL11 = 0x6E;
	static constexpr uint8_t REG_ADP_CNTL12 = 0x6F;
	static constexpr uint8_t REG_ADP_CNTL13 = 0x70;
	static constexpr uint8_t REG_ADP_CNTL14 = 0x71;
	static constexpr uint8_t REG_ADP_CNTL15 = 0x72;
	static constexpr uint8_t REG_ADP_CNTL16 = 0x73;
	static constexpr uint8_t REG_ADP_CNTL17 = 0x74;
	static constexpr uint8_t REG_ADP_CNTL18 = 0x75;
	static constexpr uint8_t REG_ADP_CNTL19 = 0x76;
	static constexpr uint8_t REG_INTERNAL_0X7F = 0x7F;

    // ---- SPI helpers (HW NSS): one frame per transaction ----
    HAL_StatusTypeDef writeReg(uint8_t reg, uint8_t val);
    HAL_StatusTypeDef readReg(uint8_t reg, uint8_t& val);
    HAL_StatusTypeDef readMulti(uint8_t startReg, uint8_t* buf, uint16_t len);

    // Utility: safe RMW on a register
    HAL_StatusTypeDef writeRegMasked(uint8_t reg, uint8_t mask, uint8_t value);
};
//
//class KX134 {
//public:
//	//KX134(SPI_HandleTypeDef* hspi);
//	KX134();
//
//    void init();
//    void initSPI();
//    int16_t readX();
//    int16_t readY();
//    int16_t readZ();
//    void setGLimitThreshold(uint8_t threshold);
//    void enableInterrupt();
//    void disableInterrupt();
//    void writeRegister(uint8_t reg, uint8_t value);
//
//    uint8_t requestID();
//    void getAccelerations(int16_t *output);
//    bool checkExistence();
//    float convertRawToGravs(int16_t lsbValue);
//
//
//
//    /**
//     * @List of registers
//     */
//    enum class Register : uint8_t
//    {
//        MAN_ID = 0x00,
//        PART_ID = 0x01,
//        XADP_L = 0x02,
//        XADP_H = 0x03,
//        YADP_L = 0x04,
//        YADP_H = 0x05,
//        ZADP_L = 0x06,
//        ZADP_H = 0x07,
//        XOUT_L = 0x08,
//        XOUT_H = 0x09,
//        YOUT_L = 0x0A,
//        YOUT_H = 0x0B,
//        ZOUT_L = 0x0C,
//        ZOUT_H = 0x0D,
//        COTR = 0x12,
//        WHO_AM_I = 0x13,
//        TSCP = 0x14,
//        TSPP = 0x15,
//        INS1 = 0x16,
//        INS2 = 0x17,
//        INS3 = 0x18,
//        STATUS_REG = 0x19,
//        INT_REL = 0x1A,
//        CNTL1 = 0x1B,
//        CNTL2 = 0x1C,
//        CNTL3 = 0x1D,
//        CNTL4 = 0x1E,
//        CNTL5 = 0x1F,
//        CNTL6 = 0x20,
//        ODCNTL = 0x21,
//        INC1 = 0x22,
//        INC2 = 0x23,
//        INC3 = 0x24,
//        INC4 = 0x25,
//        INC5 = 0x26,
//        INC6 = 0x27,
//        TILT_TIMER = 0x29,
//        TDTRC = 0x2A,
//        TDTC = 0x2B,
//        TTH = 0x2C,
//        TTL = 0x2D,
//        FTD = 0x2E,
//        STD = 0x2F,
//        TLT = 0x30,
//        TWS = 0x31,
//        FFTH = 0x32,
//        FFC = 0x33,
//        FFCNTL = 0x34,
//        TILT_ANGLE_LL = 0x37,
//        TILT_ANGLE_HL = 0x38,
//        HYST_SET = 0x39,
//        LP_CNTL1 = 0x3A,
//        LP_CNTL2 = 0x3B,
//        WUFTH = 0x49,
//        BTSWUFTH = 0x4A,
//        BTSTH = 0x4B,
//        BTSC = 0x4C,
//        WUFC = 0x4D,
//        SELF_TEST = 0x5D,
//        BUF_CNTL1 = 0x5E,
//        BUF_CNTL2 = 0x5F,
//        BUF_STATUS_1 = 0x60,
//        BUF_STATUS_2 = 0x61,
//        BUF_CLEAR = 0x62,
//        BUF_READ = 0x63,
//        ADP_CNTL1 = 0x64,
//        ADP_CNTL2 = 0x65,
//        ADP_CNTL3 = 0x66,
//        ADP_CNTL4 = 0x67,
//        ADP_CNTL5 = 0x68,
//        ADP_CNTL6 = 0x69,
//        ADP_CNTL7 = 0x6A,
//        ADP_CNTL8 = 0x6B,
//        ADP_CNTL9 = 0x6C,
//        ADP_CNTL10 = 0x6D,
//        ADP_CNTL11 = 0x6E,
//        ADP_CNTL12 = 0x6F,
//        ADP_CNTL13 = 0x70,
//        ADP_CNTL14 = 0x71,
//        ADP_CNTL15 = 0x72,
//        ADP_CNTL16 = 0x73,
//        ADP_CNTL17 = 0x74,
//        ADP_CNTL18 = 0x75,
//        ADP_CNTL19 = 0x76,
//        INTERNAL_0X7F = 0x7F
//    };
//
//    enum class Range : uint8_t
//    {
//        RANGE_8G = 0b00,
//        RANGE_16G = 0b01,
//        RANGE_32G = 0b10,
//        RANGE_64G = 0b11
//    };
//
//    /* To enable writing to settings registers, this function must be called.
//     * After writing settings, register writing is automatically disabled, and
//     * this function must be called again to enable it.
//     */
//    void enableRegisterWriting();
//
//    /* Saves settings as currently set and disables register writing.
//     * Useful for state changes
//     */
//    void disableRegisterWriting();
//
//    // Set acceleration range (8, 16, 32, or 64 gs)
//    void setAccelRange(Range range);
//    // Set Output Data Rate Bitwise
//    void setOutputDataRateBytes(uint8_t byteHz);
//
//    // Set Output Data Rate from Hz
//    void setOutputDataRateHz(uint32_t hz);
//    bool dataReady();
//    void writeRegisterOneByte(Register addr, uint8_t data, uint8_t *buf);
//    bool enableTapEngine(bool enable);
//    bool tapDetected();
//    bool gDetect();
//    bool resetInt();
//private:
//    SPI_HandleTypeDef* hspi;
//    uint8_t gLimitThreshold;
//
// //   uint8_t readRegister(uint8_t reg);
//
// //   void writeRegister(uint8_t reg, uint8_t value);
//    int16_t readAxis(uint8_t reg_l, uint8_t reg_h);
//    void configureInterrupt();
//    bool reset();
//    void deselect();
//    void select();
//    uint8_t readRegisterOneByte(Register addr);
//    void readRegister(Register addr, uint8_t *rx_buf, int size = 2);
//    void writeRegister(Register addr, uint8_t *data, uint8_t *rx_buf, int size = 1);
//
//    int16_t read16BitValue(Register lowAddr, Register highAddr);
//    int16_t convertTo16BitValue(uint8_t low, uint8_t high);
//
//    /* Reset function
//     * Should be called on initial start (init()) and every software reset
//     */
//
//
//    // Settings variables
//
//    // CNTL1 vars
//    bool resStatus;
//    bool drdyeStatus;
//    bool gsel1Status;
//    bool gsel0Status;
//    bool tdteStatus;
//    bool tpeStatus;
//
//    // ODCNTL vars
//    bool iirBypass;
//    bool lpro;
//    bool fstup;
//    bool osa3;
//    bool osa2;
//    bool osa1;
//    bool osa0;
//
//    bool registerWritingEnabled;
//};

#endif /* SRC_KX134_H_ */
