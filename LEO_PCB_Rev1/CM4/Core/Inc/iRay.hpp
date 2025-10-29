#pragma once
#include "UartEndpoint.hpp"
#include <string>
#include <vector>

class IRay : public UartEndpoint {
public:
    explicit IRay(UART_HandleTypeDef* huart);
    void Init();


    // Palette and reticle commands
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
    void ReticleSetPosition();

private:

    std::map<std::string, std::vector<uint8_t>> irPalettes;
    void InitializePalettes();
    uint8_t rxBuffer[64];  // Adjust size as necessary for your data
    uint8_t byte_;
protected:
//    void onReceiveByte(uint8_t byte) override;
//    void processIncoming() override;
    // Override the task-based processing
    void processRxData(uint8_t byte) override;
};
