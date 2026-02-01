#pragma once
#include "UartEndpoint.hpp"
#include <string>
#include <deque>
#include <vector>
#include <map>

class IRay : public UartEndpoint {
public:
    explicit IRay(UART_HandleTypeDef* huart, uint32_t baudrate = 115200);
    void Init();

    void setProtocol();
    void SetPalette(const std::string& palette);
    void SetReticlePosition(int x, int y);
    void GetReticlePosition();
    void ReticleOn();
    void CalibReticleOFF();
    void ReticleOFF();
    void Reticle1();
    void Reticle2();
    void Reticle3();
    void Reticle4();
    void NUC_Shutter();

protected:
    void processRxData(const uint8_t* data, uint16_t length) override;

private:
    std::deque<uint8_t> messageBuffer_;
    std::map<std::string, std::vector<uint8_t>> irPalettes;
    void InitializePalettes();
};
