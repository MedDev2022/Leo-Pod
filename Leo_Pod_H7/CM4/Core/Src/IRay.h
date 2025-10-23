/*
 * IRay.h
 *
 *  Created on: Oct 13, 2024
 *      Author: krinm
 */

#ifndef SRC_IRAY_H_
#define SRC_IRAY_H_

#include "UartDevice.h"
#include <map>
#include <string>
#include <vector>

class IRay : public UartDevice {
public:

	IRay(USART_TypeDef * portName, int defaultBaudRate = 9600);

    // Connection handling
    bool Connect(USART_TypeDef * portName, int baudRate = 19200) override;

    // Commands for device modes
//    void StartTechnicianModeCMD() override;
//    void StartTransparentModeCMD() override;
//    void StartBootloaderModeCMD() override;
//    void StartWorkingModeCMD() override;

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

    void HandleReceivedData(const std::vector<uint8_t>& data) override;  // Implement this function


protected:
  //  void OnDataReceivedHandler(const std::vector<uint8_t>& command) override;

private:
    std::map<std::string, std::vector<uint8_t>> irPalettes;
    void InitializePalettes();
    uint8_t rxBuffer[64];  // Adjust size as necessary for your data
};


#endif /* SRC_IRAY_H_ */
