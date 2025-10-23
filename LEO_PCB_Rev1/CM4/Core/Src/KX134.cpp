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

//
//
//    writeRegisterOneByte(Register::CNTL1,0x00);
//	writeRegisterOneByte(Register::ODCNTL,0x08);
//	writeRegisterOneByte(Register::INC1,0x30);//Enable active High physical interrupt pi
//	writeRegisterOneByte(Register::INC4,0x0A); //Enable wake up motion detect on physical pin INT1
//	writeRegisterOneByte(Register::INC2,0x03); //All direction motion detection enabled with OR operator
//	writeRegisterOneByte(Register::CNTL3,0xAE);
//	writeRegisterOneByte(Register::CNTL4,0x36);//0x76
//	writeRegisterOneByte(Register::CNTL5,0x01);
//	writeRegisterOneByte(Register::BTSC,0x05);
//	writeRegisterOneByte(Register::WUFC,0x00);//Debounce counter
//	writeRegisterOneByte(Register::WUFTH,(gHLimit/15 & 0xFF));
//	writeRegisterOneByte(Register::BTSWUFTH,((gLLimit/15 >>4) & 0x70) | ((gHLimit/15 & 0x0700) >> 8));
//	writeRegisterOneByte(Register::BTSTH,(gLLimit/15 & 0xFF));



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

//#include <KX134.hpp>
//#include "math.h"
////"#include "main.h"
//
//
//KX134::KX134(){//(SPI_HandleTypeDef* hspi) {
//    this->hspi = &hspi2;
//};
//void KX134::init(){
//
//	initSPI();
//	reset();
//
//	setAccelRange(KX134::Range::RANGE_32G);
//
//   // enableTapEngine(true);
//    enableInterrupt();
//   // disableRegisterWriting();
//
//
//
//}
//void KX134::initSPI() {
//    // Initialize SPI peripheral as needed
//	    /* SPI1 parameter configuration*/
//	    hspi->Instance = SPI2;
//	    hspi->Init.Mode = SPI_MODE_MASTER;
//	    hspi->Init.Direction = SPI_DIRECTION_2LINES;
//	    hspi->Init.DataSize = SPI_DATASIZE_8BIT;
//	    hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
//	    hspi->Init.CLKPhase = SPI_PHASE_1EDGE;
//	    hspi->Init.NSS = SPI_NSS_SOFT;
//	    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; // Adjust as needed
//	    hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;
//	    hspi->Init.TIMode = SPI_TIMODE_DISABLE;
//	    hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
//	    hspi->Init.CRCPolynomial = 7;
//	    hspi->Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
//	    hspi->Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
//
//	    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
//	        // Initialization Error
//	       // Error_Handler();
//	    }
//
//	    GPIO_InitTypeDef GPIO_InitStruct = {0};
//
//	    /* Configure GPIO pins for SPI1 */
//	    /* SPI1 SCK */
//	    GPIO_InitStruct.Pin = GPIO_PIN_5;
//	    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//	    GPIO_InitStruct.Pull = GPIO_NOPULL;
//	    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
//	    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//
//	    /* SPI1 MISO */
//	    GPIO_InitStruct.Pin = GPIO_PIN_6;
//	    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
//	    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//
//	    /* SPI1 MOSI */
//	    GPIO_InitStruct.Pin = GPIO_PIN_7;
//	    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
//	    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//
//	    /* SPI1 NSS */
//	    GPIO_InitStruct.Pin = GPIO_PIN_4;
//		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;//GPIO_MODE_AF_PP;
//		GPIO_InitStruct.Pull = GPIO_NOPULL;
//		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//		GPIO_InitStruct.Alternate = GPIO_AF5_SPI1; // Set alternate function to SPI1 NSS
//		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//
//	    if(reset()) __NOP();
//	    	//Utils::DebugMsg("KX134 Exists");
//
//}
//
//static uint8_t defaultBuffer[2] = {0}; // allows calling writeRegisterOneByte
//                                       // without buf argument
///* Writes one byte to a register
// */
//void KX134::writeRegisterOneByte(Register addr, uint8_t data, uint8_t *buf = defaultBuffer)
//{
//    uint8_t _data[1] = {data};
//    writeRegister(addr, _data, buf);
//}
//
//bool KX134::reset()
//{
//    // write registers to start reset
//    writeRegisterOneByte(Register::INTERNAL_0X7F, 0x00);
//    writeRegisterOneByte(Register::CNTL2, 0x00);
//    writeRegisterOneByte(Register::CNTL2, 0x80);
//
//
//
//    // software reset time
//    HAL_Delay(2);
//    // check existence
//    return checkExistence();
//
//
//}
//void KX134::deselect()
//{
//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
//}
//
//void KX134::select()
//{
//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
//}
//
//void KX134::setGLimitThreshold(uint8_t threshold) {
//    gLimitThreshold = threshold;
//    writeRegisterOneByte(Register::TTH, threshold);
//
//}
//bool KX134::enableTapEngine(bool enable)
//{
//
//  uint8_t tempVal;
//
//  tempVal = readRegisterOneByte(Register::CNTL1);
//
//  tempVal |= (0x01 <<2 ); //set TDTE
//
//  writeRegisterOneByte(Register::CNTL1, tempVal);
//
//  return true;
//}
//void KX134::enableInterrupt() {
//
//	const unsigned int gHLimit = 4000; //limits are in mg with 15mg resolution
//	const unsigned int gLLimit = 2000;
//
//    // Enable interrupts as needed
//  //  enableRegisterWriting();
//	writeRegisterOneByte(Register::CNTL1,0x00);
//    writeRegisterOneByte(Register::ODCNTL,0x08);
//    writeRegisterOneByte(Register::INC1,0x30);//Enable active High physical interrupt pi
//    writeRegisterOneByte(Register::INC4,0x0A); //Enable wake up motion detect on physical pin INT1
//    writeRegisterOneByte(Register::INC2,0x03); //All direction motion detection enabled with OR operator
//    writeRegisterOneByte(Register::CNTL3,0xAE);
//    writeRegisterOneByte(Register::CNTL4,0x36);//0x76
//    writeRegisterOneByte(Register::CNTL5,0x01);
//    writeRegisterOneByte(Register::BTSC,0x05);
//    writeRegisterOneByte(Register::WUFC,0x00);//Debounce counter
//    writeRegisterOneByte(Register::WUFTH,(gHLimit/15 & 0xFF));
//    writeRegisterOneByte(Register::BTSWUFTH,((gLLimit/15 >>4) & 0x70) | ((gHLimit/15 & 0x0700) >> 8));
//    writeRegisterOneByte(Register::BTSTH,(gLLimit/15 & 0xFF));
////    writeRegisterOneByte(Register::WUFTH,0x20);
////    writeRegisterOneByte(Register::BTSWUFTH,0);
////    writeRegisterOneByte(Register::BTSTH,0x20);
//
////    char buff[10];
////    buff[0] = (gHLimit/15 & 0xFF);
////    buff[1] = (((gLLimit/15 >>4) & 0x70) | ((gHLimit/15 & 0x0700) >> 8));
////    buff[2] = (gLLimit/15 & 0xFF);
////    Utils::DebugMsg("Limit Settings");
////    Utils::DebugHexMsg(buff, 3);
//
//
//
//
//    writeRegisterOneByte(Register::CNTL1,0xE0);
//  // writeRegisterOneByte(Register::CNTL5,0x01);
//
////    writeRegisterOneByte(Register::TTH,0xFF);
////    writeRegisterOneByte(Register::TTL,0x07);
////    writeRegisterOneByte(Register::TDTRC,0x01);
//    //writeRegisterOneByte(Register::TDTC,0x78);
//   // disableRegisterWriting();
//
//
//
////    uint8_t tempVal;
////
////    tempVal = readRegisterOneByte(Register::TDTRC);
////    tempVal |= 0x01; //set STRE
////
////    writeRegisterOneByte(Register::TDTRC, tempVal);
//
//
//}
//
//void KX134::disableInterrupt() {
//    // Disable interrupts as needed
//}
//
//void KX134::configureInterrupt() {
//    // Configure interrupts as needed
//}
//
//int16_t KX134::convertTo16BitValue(uint8_t low, uint8_t high)
//{
//    // combine low & high words
//    uint16_t val2sComplement = (static_cast<uint16_t>(high << 8)) | low;
//    int16_t value = static_cast<int16_t>(val2sComplement);
//
//    return value;
//}
//
//float KX134::convertRawToGravs(int16_t lsbValue)
//{
//    if(gsel1Status && gsel0Status)
//    {
//        // +-64g
//        return (float)lsbValue * 0.00195f;
//    }
//    else if(gsel1Status && !gsel0Status)
//    {
//        // +-32g
//        return (float)lsbValue * 0.00098f;
//    }
//    else if(!gsel1Status && gsel0Status)
//    {
//        // +-16g
//        return (float)lsbValue * 0.00049f;
//    }
//    else if(!gsel1Status && !gsel0Status)
//    {
//        // +-8g
//        return (float)lsbValue * 0.00024f;
//    }
//    else
//    {
//        return 0;
//    }
//}
//
//bool KX134::gDetect(){
//
//	uint8_t status = readRegisterOneByte(Register::INS3);
//
//	if (status & 0x80) //Motion detected the g is above the limit (see WUFTH, BTSWUFTH, BTSC settings)
//	{
//		//Utils::DebugMsg("Motion detected");
//	}
//
//	if (status & 0x40) // Back to sleep is detected, the g is below the limit (see WUFTH, BTSWUFTH, BTSC settings)
//	{
//		//Utils::DebugMsg("Back to sleep detected");
//	}
//
//	return (status & 0x80);
//}
//
//bool KX134::resetInt(){
//
//	return readRegisterOneByte(Register::INT_REL);
//
//}
//
//bool KX134::tapDetected()
//{
//
//  uint8_t tempVal,tempVal_1;
//
//  tempVal = readRegisterOneByte(Register::INS2);
//
//  tempVal_1 = readRegisterOneByte(Register::INT_REL);
//  uint8_t sByte;
//  sByte=tempVal;
//  //Utils::DebugHexMsg((char*)&sByte, sizeof(byte), false);
//
//  return tempVal & 0x0f;//((tempVal & 0b110) == 0b010); // Single tap
//}
//
//void KX134::getAccelerations(int16_t *output)
//{
//    uint8_t words[7];
//
//    readRegister(Register::XOUT_L, words, 7);
//
//    output[0] = convertTo16BitValue(words[1], words[2]);
//    output[1] = convertTo16BitValue(words[3], words[4]);
//    output[2] = convertTo16BitValue(words[5], words[6]);
//
//}
//void KX134::setAccelRange(Range range)
//{
//    gsel0Status = (uint8_t)range & 0b01;
//    gsel1Status = (uint8_t)range & 0b10;
//
//    uint8_t _cntl1 = readRegisterOneByte(Register::CNTL1);
//
//    _cntl1 = ((_cntl1 & 0b11100111) | (((uint8_t)range & 0b11)  << 3));
//    _cntl1 |= 0x80; //Set PC1 to enable
//
////    uint8_t writeByte = (1 << 7) | (resStatus << 6) | (drdyeStatus << 5) |
////                        (gsel1Status << 4) | (gsel0Status << 3) |
////                        (tdteStatus << 2) | (tpeStatus);
//    // reserved bit 1, PC1 bit must be enabled
//
// //   writeRegisterOneByte(Register::CNTL1, writeByte);
//    writeRegisterOneByte(Register::CNTL1, _cntl1);
//
//    registerWritingEnabled = 0;
//}
//void KX134::setOutputDataRateBytes(uint8_t byteHz)
//{
//    if(!registerWritingEnabled)
//    {
//        return;
//    }
//
//    osa0 = byteHz & 0b0001;
//    osa1 = byteHz & 0b0010;
//    osa2 = byteHz & 0b0100;
//    osa3 = byteHz & 0b1000;
//
//    uint8_t writeByte = (iirBypass << 7) | (lpro << 6) | (fstup << 5) |
//                        (osa3 << 3) | (osa2 << 2) | (osa1 << 1) | osa0;
//    // reserved bit 4
//
//    writeRegisterOneByte(Register::ODCNTL, writeByte);
//}
//bool KX134::checkExistence()
//{
//    // verify WHO_I_AM
//    uint8_t whoami[2];
//    readRegister(Register::WHO_AM_I, whoami);
//
//    if(whoami[1] != 0x46)
//    {
//        return false; // WHO_AM_I is incorrect
//    }
//
//    // verify COTR
//    uint8_t cotr[2];
//    readRegister(Register::COTR, cotr);
//    if(cotr[1] != 0x55)
//    {
//        return false; // COTR is incorrect
//    }
//
//    return true;
//}
//
//
//void KX134::writeRegister(Register addr, uint8_t *data, uint8_t *rx_buf, int size)
//{
//
//	uint8_t  tx_buf = (uint8_t)addr;
//
//    select();
//    HAL_SPI_TransmitReceive(hspi, &tx_buf, rx_buf, 1, HAL_MAX_DELAY);
//    HAL_SPI_TransmitReceive(hspi, data, rx_buf, size, HAL_MAX_DELAY);
//    deselect();
//
//}
//
//int16_t KX134::readAxis(uint8_t reg_l, uint8_t reg_h) {
//    // Implement SPI read operation to read axis data
//	return 0;
//}
//
//int16_t KX134::readX() {
//
//    uint8_t words[3];
//    readRegister(Register::XOUT_L, words, 3);
//    return convertTo16BitValue(words[1], words[2]);
//
//}
//
//int16_t KX134::readY() {
//    uint8_t words[3];
//    readRegister(Register::YOUT_L, words, 3);
//    return convertTo16BitValue(words[1], words[2]);
//}
//
//int16_t KX134::readZ() {
//    uint8_t words[3];
//    readRegister(Register::ZOUT_L, words, 3);
//    return convertTo16BitValue(words[1], words[2]);
//}
//uint8_t KX134::readRegisterOneByte(Register addr)
//{
//    uint8_t _data[2] = {0};
//    readRegister(addr,_data,2);
//    return _data[1];
//}
//
//void KX134::readRegister(Register addr, uint8_t *rx_buf, int size)
//{
//
//	uint8_t  tx_buf[size]={0};
//	tx_buf[0] = (uint8_t)addr | (1 << 7);
//	select();
//    HAL_SPI_TransmitReceive(hspi, tx_buf, rx_buf, size, HAL_MAX_DELAY);
//    deselect();
//
//}
//
//void KX134::setOutputDataRateHz(uint32_t hz)
//{
//    // calculate byte representation from new polling rate
//    // bytes = log2(32*rate/25)
//
//    double new_rate = (double)hz;
//
//    double bytes_double = log2((32.f / 25.f) * new_rate);
//    uint8_t bytes_int = (uint8_t)ceil(bytes_double);
//
//    setOutputDataRateBytes(bytes_int);
//}
//
//void KX134::enableRegisterWriting()
//{
//  //  writeRegisterOneByte(Register::CNTL1, 0x00);
//
//
//    uint8_t _cntl1 = readRegisterOneByte(Register::CNTL1);
//
//     _cntl1 = (_cntl1 & 0b01111111);
//
//     writeRegisterOneByte(Register::CNTL1, _cntl1);
//    registerWritingEnabled = 1;
//}
//
//void KX134::disableRegisterWriting()
//{
//    if(!registerWritingEnabled)
//    {
//        return;
//    }
//
//    uint8_t writeByte = (0 << 7) | (resStatus << 6) | (drdyeStatus << 5) |
//                        (gsel1Status << 4) | (gsel0Status << 3) |
//                        (tdteStatus << 2) | (tpeStatus);
//    // reserved bit 1, PC1 bit must be enabled
//
//    writeRegisterOneByte(Register::CNTL1, writeByte);
//
//    registerWritingEnabled = 0;
//}
//
//bool KX134::dataReady()
//{
//    uint8_t buf[2];
//    readRegister(Register::INS2, buf);
//    return (buf[1] == 0x10);
//}



