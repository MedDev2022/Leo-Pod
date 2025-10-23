/*
 * IRay.h
 *
 *  Created on: Oct 13, 2024
 *      Author: krinm
 */

#ifndef SRC_CLIENT_H_
#define SRC_CLIENT_H_

#include "UartDevice.h"
#include <map>
#include <string>
#include <vector>
#include "comm.h"

using namespace comm;
class Client : public UartDevice {
public:



	Client(USART_TypeDef * portName, int defaultBaudRate = 9600);

    // Connection handling
    bool StartReceive();

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
    Message rxMsg;
#define MY_ID  0x70

};

static Client client(USART3, 115200);

#endif /* SRC_CLIENT_H_ */
