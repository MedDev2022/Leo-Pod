// stm32_uart_freertos.cpp

#include <KX134_SPI.hpp>
#include "main.h"
#include "cmsis_os.h"

#include "Client.hpp"
#include "iRay.hpp"
#include "DayCam.hpp"
#include "LRX20A.hpp"
#include "RPLens.hpp"
#include "TempSens.hpp"
extern "C" {
#include "FreeRTOS.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
}



// tec_task.cpp
#include "tec.hpp"



#include "UartEndpoint.hpp"

extern "C" void cpp_app_main(void);
extern "C" void MyTaskFunction(void *argument);  // Forward declaration

// Provide _write syscall for printf redirection
extern "C" int _write(int file, char *ptr, int len);



//typedef union {
//    uint16_t value;   // packed 32-bit register/value
//    struct {
//        unsigned COOLING_ON        : 1;  // bit 0
//        unsigned HEATER_SDI_CONV   : 1;  // bit 1
//        unsigned HEATER_ON_IRCAM   : 1;  // bit 2
//        unsigned HEATER_MISC       : 1;  // bit 3
//        unsigned DAYCAM_TEC_ON     : 1;  // bit 4
//        unsigned DAYCAM_COOL_HEAT  : 1;  // bit 5 (0=cool,1=heat) if that's your meaning
//        unsigned IR_TEC_ON         : 1;  // bit 6
//        unsigned IR_COOL_HEAT      : 1;  // bit 7
//        unsigned RP_TEC_ON         : 1;  // bit 8
//        unsigned RP_COOL_HEAT      : 1;  // bit 9
//        unsigned _reserved         : 6; // bits 10..15
//    } bits;
//} ctrlReg_t;


//Client* clientDevice=nullptr;

extern UART_HandleTypeDef huart3;  // make sure it's defined elsewhere (e.g., main.c/.cpp)
extern UART_HandleTypeDef huart1;  // make sure it's defined elsewhere (e.g., main.c/.cpp)
extern UART_HandleTypeDef huart8;  // make sure it's defined elsewhere (e.g., main.c/.cpp)



uint8_t rxByte;



// 🚀 Application entry point called from StartDefaultTask
extern "C" void cpp_app_main(void)
{
    printf("cpp_app_main started\r\n");

    // Create a new task
    const osThreadAttr_t myTask_attributes = {
        .name = "myTask",
        .stack_size = 512 * 4,  // 2KB stack
        .priority = (osPriority_t) osPriorityNormal,
    };

    osThreadId_t myTaskHandle = osThreadNew(MyTaskFunction, nullptr, &myTask_attributes);
    if (myTaskHandle == nullptr) {
        printf("Failed to create MyTaskFunction\r\n");
    } else {
        printf("MyTaskFunction created successfully\r\n");
    }

    // You can return here — or sleep forever if this is the main task
    // osDelay(osWaitForever); // Optional if not returning
}


void i2c_scan(I2C_HandleTypeDef* hi2c) {
    for (uint8_t a = 0x03; a < 0x78; ++a) {
        if (HAL_I2C_IsDeviceReady(hi2c, uint16_t(a << 1), 1, 10) == HAL_OK) {
            printf("I2C found: 0x%02X\r\n", a);
        }
    }
}

extern  void MyTaskFunction(void *argument)
{



	Client client(&huart1);
	LRX20A* lrx20A = new LRX20A(&huart2);
	DayCam* dayCam = new DayCam(&huart3);
	RPLens* rpLens = new RPLens(&huart7);
	IRay* iRay = new IRay(&huart8);


	client.Init();

	client.setLRF(lrx20A);
	lrx20A->Init();

   	client.setRPLens(rpLens);
    rpLens->Init();

   	client.setIRay(iRay);
    iRay->Init();

    client.setDayCam(dayCam);
    dayCam->Init();


    printf("Before Cam Inint!\r\n");


    printf("After Cam Inint!\r\n");

 //   client.setLRF(lrx20A);


//	KX134 kx134;
 //   kx134.init();

  //  lrx20A->InitLRX20A();

  //  dayCam->address_command;


        STS4L::Config cfg{
            .hi2c        = &hi2c2,
            .muxAddr7bit = 0x70,   // PCA9546A default
            .muxChannel  = 0,      // you said channel 0
            .devAddr7bit = 0x44,   // <<< set your sensor’s 7-bit I2C address here
            .i2cTimeoutMs = 20
        };
       static STS4L sensor(cfg);

       i2c_scan(&hi2c2);

        // Optional: reset once on boot
        sensor.softReset();

        float tC = 0.f;
        if (sensor.readTemperature(tC) == HAL_OK) {
            printf("STS4L: %.2f C\r\n", tC);
        } else {
            printf("STS4L read failed\r\n");
        }


        KX134_SPI acc({ .hspi = &hspi2, .timeout_ms = 10 });

        uint8_t id=0;
        acc.whoAmI(id);                    // expect ~0x46
        acc.init(KX134_SPI::RANGE_8G, 0x03);
        acc.setSensitivityLSBperG(2048.0f);// tune to your datasheet
        int16_t x,y,z;


        printf("KX134 WHO=0x%02X C\r\n",
               id);



 //       extern ctrlReg_t tmpReg;

    for (;;)
    {
        printf("Hello from MyTaskFunction!\r\n");
        HAL_GPIO_TogglePin(MCU_LED_2_GPIO_Port, MCU_LED_2_Pin);  // Optional
        osDelay(1000);  // delay 1 second


        HAL_StatusTypeDef st = sensor.readTemperature(tC);

        //if (sensor.readTemperature(tC) == HAL_OK) {
        if (st == HAL_OK) {
            printf("Temperature: %.2f C\r\n", tC);
        } else {
            printf("Temperature read failed with error: %d\r\n", (uint16_t)st);
        }


        if (acc.readRaw(x,y,z)==HAL_OK) {
            float gx,gy,gz; acc.countsToG(x,y,z,gx,gy,gz);
            printf("KX134 g: %.3f %.3f %.3f\r\n", gx,gy,gz);
        }
        else printf("KX134 read failed\r\n");




    	if (client.tmpReg.bits.COOLING_ON) 			HAL_GPIO_TogglePin(	COOLING_ON_GPIO_Port, 		COOLING_ON_Pin);
    	if (client.tmpReg.bits.DAYCAM_COOL_HEAT) 	HAL_GPIO_TogglePin(	DAYCAM_COOL_HEAT_GPIO_Port ,DAYCAM_COOL_HEAT_Pin);
    	if (client.tmpReg.bits.HEATER_ON_IRCAM) 	HAL_GPIO_TogglePin(	HEATER_ON_IRCAM_GPIO_Port, 	HEATER_ON_IRCAM_Pin);
    	if (client.tmpReg.bits.HEATER_SDI_CONV) 	HAL_GPIO_TogglePin(	HEATER_SDI_CONV_GPIO_Port, 	HEATER_SDI_CONV_Pin);
    	if (client.tmpReg.bits.HEATER_MISC) 		HAL_GPIO_TogglePin(	HEATER_MISC_GPIO_Port, 		HEATER_MISC_Pin);
    	if (client.tmpReg.bits.DAYCAM_TEC_ON) 		HAL_GPIO_TogglePin(	DAYCAM_TEC_ON_GPIO_Port, 	DAYCAM_TEC_ON_Pin);
    	if (client.tmpReg.bits.IR_TEC_ON) 			HAL_GPIO_TogglePin(	IR_TEC_ON_GPIO_Port, 		IR_TEC_ON_Pin);
    	if (client.tmpReg.bits.IR_COOL_HEAT) 		HAL_GPIO_TogglePin(	IR_COOL_HEAT_GPIO_Port, 	IR_COOL_HEAT_Pin);
    	if (client.tmpReg.bits.RP_TEC_ON) 			HAL_GPIO_TogglePin(	RP_TEC_ON_GPIO_Port, 		RP_TEC_ON_Pin);
    	if (client.tmpReg.bits.RP_COOL_HEAT) 		HAL_GPIO_TogglePin(	RP_COOL_HEAT_GPIO_Port, 	RP_COOL_HEAT_Pin);

    	if (!client.tmpReg.bits.COOLING_ON) 		HAL_GPIO_WritePin(	COOLING_ON_GPIO_Port, 		COOLING_ON_Pin, 		GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.DAYCAM_COOL_HEAT) 	HAL_GPIO_WritePin(	DAYCAM_COOL_HEAT_GPIO_Port ,DAYCAM_COOL_HEAT_Pin, 	GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.HEATER_ON_IRCAM) 	HAL_GPIO_WritePin(	HEATER_ON_IRCAM_GPIO_Port, 	HEATER_ON_IRCAM_Pin, 	GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.HEATER_SDI_CONV) 	HAL_GPIO_WritePin(	HEATER_SDI_CONV_GPIO_Port, 	HEATER_SDI_CONV_Pin, 	GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.HEATER_MISC) 		HAL_GPIO_WritePin(	HEATER_MISC_GPIO_Port, 		HEATER_MISC_Pin, 		GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.DAYCAM_TEC_ON) 		HAL_GPIO_WritePin(	DAYCAM_TEC_ON_GPIO_Port, 	DAYCAM_TEC_ON_Pin, 		GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.IR_TEC_ON) 			HAL_GPIO_WritePin(	IR_TEC_ON_GPIO_Port, 		IR_TEC_ON_Pin, 			GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.IR_COOL_HEAT) 		HAL_GPIO_WritePin(	IR_COOL_HEAT_GPIO_Port, 	IR_COOL_HEAT_Pin, 		GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.RP_TEC_ON) 			HAL_GPIO_WritePin(	RP_TEC_ON_GPIO_Port, 		RP_TEC_ON_Pin, 			GPIO_PIN_RESET);
    	if (!client.tmpReg.bits.RP_COOL_HEAT) 		HAL_GPIO_WritePin(	RP_COOL_HEAT_GPIO_Port, 	RP_COOL_HEAT_Pin, 		GPIO_PIN_RESET);

    	printf("Reg: %x\r\n", client.tmpReg.value);

    }
}


// --- your temp sources (replace with STS4L / ADC+NTC) ---
static float readTempC_IR()  { return 31.2f; }
static float readTempC_Day() { return 28.9f; }

//extern  void HeatCoolControl(void *argument){
//
//
//
////
////     TecHalDriver2Pin drvIR  (
////        IR_COOL_HEAT_GPIO_Port, IR_COOL_HEAT_Pin,
////        IR_TEC_ON_GPIO_Port,    IR_TEC_ON_Pin,
////        true, true, &readTempC_IR
////    );
////    static TecHalDriver2Pin drvDay(
////        DAYCAM_COOL_HEAT_GPIO_Port, DAYCAM_COOL_HEAT_Pin,
////        DAYCAM_TEC_ON_GPIO_Port,    DAYCAM_TEC_ON_Pin,
////        true, true, &readTempC_Day
////    );
//
//
//	// configs + controllers
//	static TecConfig cfgIR, cfgDay;
////	static Tec tecIR(drvIR, cfgIR);
////	static Tec tecDay(drvDay, cfgDay);
//
//
//	    // ensure outputs start OFF
////	    drvIR.enOff();  drvDay.enOff();
//
//	    // configure
//	    cfgIR.setpointC = 30.0f; cfgIR.hysteresisC = 1.0f;
//	    cfgDay.setpointC= 32.0f; cfgDay.hysteresisC= 0.8f;
//
////	    tecIR.setMode(TecMode::COOL);
////	    tecDay.setMode(TecMode::HEAT);
//
////	    manager.add(&tecIR);
////	    manager.add(&tecDay);
//
//	    const uint32_t periodMs = 50; // 20 Hz
//	    for (;;) {
////	        manager.stepAll(HAL_GetTick());
//	        osDelay(periodMs);
//	    }
//	}
//
//}





