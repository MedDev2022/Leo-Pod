/*
 * FPGA.h
 *
 *  Created on: Oct 13, 2024
 *      Author: krinm
 */

#ifndef SRC_FPGA_H_
#define SRC_FPGA_H_

#include "UartDevice.h"
#include <map>
#include <string>
#include <vector>
#include "comm.h"

using namespace comm;
class Fpga : public UartDevice {
public:



	Fpga(USART_TypeDef * portName, int defaultBaudRate = 230400);

    // Connection handling
    bool StartReceive();

    // Commands for device modes
//    void StartTechnicianModeCMD() override;
//    void StartTransparentModeCMD() override;
//    void StartBootloaderModeCMD() override;
//    void StartWorkingModeCMD() override;



    void HandleReceivedData(const std::vector<uint8_t>& data) override;  // Implement this function


protected:
  //  void OnDataReceivedHandler(const std::vector<uint8_t>& command) override;

private:
    uint8_t rxBuffer[64];  // Adjust size as necessary for your data
    Message rxMsg;
#define MY_ID  0x70

};

static Fpga fpga(USART1,460800); //230400);

#endif /* SRC_FPGA_H_ */
