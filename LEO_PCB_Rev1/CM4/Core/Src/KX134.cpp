/*
 * KX134.cpp
 *
 *  Created on: Apr 19, 2024
 *      Author: krinm
 */

#include "KX134_SPI.hpp"
#include <cstring>
#include "cmsis_os.h"

KX134_SPI::KX134_SPI(const Config& c) : cfg_(c) {}

HAL_StatusTypeDef KX134_SPI::writeReg(uint8_t reg, uint8_t val) {
    // Write frame: [addr(write)][data]
    uint8_t tx[2];
    tx[0] = uint8_t(reg & 0x3F);        // bit7=0 write, bit6=0 no auto-inc
    tx[1] = val;
    return HAL_SPI_Transmit(cfg_.hspi, tx, 2, cfg_.timeout_ms);
}

HAL_StatusTypeDef KX134_SPI::readReg(uint8_t reg, uint8_t& val) {
    // Read frame: [addr(read)][dummy] -> rx[0]=don't care, rx[1]=data
    uint8_t tx[2];
    uint8_t rx[2] = {0,0};
    tx[0] = uint8_t(0x80 | (reg & 0x3F)); // bit7=1 read, bit6=0 no auto-inc
    tx[1] = 0x00;
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(cfg_.hspi, tx, rx, 2, cfg_.timeout_ms);
    if (st == HAL_OK) val = rx[1];
    return st;
}

HAL_StatusTypeDef KX134_SPI::readMulti(uint8_t startReg, uint8_t* buf, uint16_t len) {
    if (len == 0 || buf == nullptr) return HAL_OK;

    // Use single-call frames so HW NSS stays asserted.
    // We’ll chunk large reads to keep stack usage small.
    static constexpr uint16_t CHUNK_MAX = 64;

    uint16_t remaining = len;
    uint8_t*  p        = buf;
    uint8_t   reg      = startReg;

    while (remaining) {
        uint16_t chunk = (remaining > CHUNK_MAX) ? CHUNK_MAX : remaining;

        // TX: [addr(read+auto-inc)][0 ... 0]  (1 + chunk bytes)
        uint8_t tx[1 + CHUNK_MAX];
        uint8_t rx[1 + CHUNK_MAX];
        tx[0] = uint8_t(0xC0 | (reg & 0x3F)); // bit7=read, bit6=auto-inc
        std::memset(&tx[1], 0x00, chunk);

        HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(cfg_.hspi, tx, rx, 1 + chunk, cfg_.timeout_ms);
        if (st != HAL_OK) return st;

        // Skip rx[0] (dummy after address); data start at rx[1]
        std::memcpy(p, &rx[1], chunk);

        p        += chunk;
        reg      = uint8_t(reg + chunk); // next block start (device will wrap internally as needed)
        remaining-= chunk;
    }
    return HAL_OK;
}

HAL_StatusTypeDef KX134_SPI::whoAmI(uint8_t& id) {
    id = 0;
    return readReg(REG_WHO_AM_I, id);
}

HAL_StatusTypeDef KX134_SPI::writeRegMasked(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t v = 0;
    HAL_StatusTypeDef st = readReg(reg, v);
    if (st != HAL_OK) return st;
    v = (uint8_t)((v & ~mask) | (value & mask));
    return writeReg(reg, v);
}

HAL_StatusTypeDef KX134_SPI::init(Range range, uint8_t odr_code) {
    // 1) Standby (PC1=0)
//    if (writeRegMasked(REG_CNTL1, (1u<<7), 0x00) != HAL_OK) return HAL_ERROR;
//
//    // 2) Set range (GSEL bits [3:2] — verify mapping for KX134)
//    uint8_t gsel = (uint8_t(range) & 0x3) << 2;
//    if (writeRegMasked(REG_CNTL1, (uint8_t)(0b11u<<2), gsel) != HAL_OK) return HAL_ERROR;
//
//    // 3) Set ODR/filter
//    if (writeReg(REG_ODCNTL, odr_code) != HAL_OK) return HAL_ERROR;
//
//    // 4) Power on (PC1=1)
//    if (writeRegMasked(REG_CNTL1, (1u<<7), (1u<<7)) != HAL_OK) return HAL_ERROR;
//
//    if (writeRegMasked(REG_CNTL3, (0xFFu), 0xAE) != HAL_OK) return HAL_ERROR;

	if (writeReg(REG_CNTL1, 0x00) != HAL_OK) return HAL_ERROR;

//	if (writeReg(REG_INC1,  0x30) != HAL_OK) return HAL_ERROR;
//
//	if (writeReg(REG_INC4,  0x10) != HAL_OK) return HAL_ERROR;

	if (writeReg(REG_ODCNTL,  0x06) != HAL_OK) return HAL_ERROR;

	if (writeReg(REG_CNTL1,  0xC0) != HAL_OK) return HAL_ERROR;

    osDelay(2);

    // Sensitivity (counts per g). Tune from datasheet LSB/g table.
    switch (range) {
        case RANGE_8G:  lsbPerG_ = 4096.0f; break;
        case RANGE_16G: lsbPerG_ = 2048.0f; break;
        case RANGE_32G: lsbPerG_ = 1024.0f; break;
        case RANGE_64G: lsbPerG_ = 512.0f;  break;
    }
    return HAL_OK;
}

HAL_StatusTypeDef KX134_SPI::readRaw(int16_t& x, int16_t& y, int16_t& z) {
    uint8_t b[6] = {0};
    HAL_StatusTypeDef st = readMulti(REG_XOUT_L, b, 6);
    if (st != HAL_OK) return st;


    readReg(REG_XOUT_L,b[0]);
    readReg(REG_XOUT_H,b[1]);

    readReg(REG_YOUT_L,b[2]);
    readReg(REG_YOUT_H,b[3]);

    readReg(REG_ZOUT_L,b[4]);
    readReg(REG_ZOUT_H,b[5]);

    // Little-endian: L then H
    x = (int16_t)((uint16_t)b[1] << 8 | b[0]);
    y = (int16_t)((uint16_t)b[3] << 8 | b[2]);
    z = (int16_t)((uint16_t)b[5] << 8 | b[4]);

    return HAL_OK;
}

void KX134_SPI::countsToG(int16_t x, int16_t y, int16_t z, float& gx, float& gy, float& gz) const {
    gx = float(x) / lsbPerG_;
    gy = float(y) / lsbPerG_;
    gz = float(z) / lsbPerG_;
}

HAL_StatusTypeDef KX134_SPI::enableDataReadyINT1(bool activeHigh, bool pushPull) {
    // Typical KX13x pattern (verify bits for KX134-1211 revision!):
    //  - INC4: bit0 DRDY enable, bit1 polarity (1=active-high), bit2 drive (1=push-pull)
    //  - INC1: route DRDY to INT1 (e.g., set bit5)
    uint8_t inc4 = 0, inc1 = 0;
    if (readReg(REG_INC4, inc4) != HAL_OK) return HAL_ERROR;
    if (readReg(REG_INC1, inc1) != HAL_OK) return HAL_ERROR;

    inc4 |= (1u<<0);                 // DRDY enable
    if (activeHigh) inc4 |= (1u<<1); else inc4 &= ~(1u<<1);
    if (pushPull)   inc4 |= (1u<<2); else inc4 &= ~(1u<<2);

    inc1 |= (1u<<5);                 // route DRDY -> INT1 (check exact bit in datasheet)

    if (writeReg(REG_INC4, inc4) != HAL_OK) return HAL_ERROR;
    if (writeReg(REG_INC1, inc1) != HAL_OK) return HAL_ERROR;

    // Clear any latched interrupt
//    uint8_t dummy=0;
//    (void)readReg(REG_INT_REL, dummy);
    return HAL_OK;
}


