// stm32_uart_freertos.cpp

#include <Host.hpp>
#include <KX134_SPI.hpp>
#include "main.h"

#include "iRay.hpp"
#include "DayCam.hpp"
#include "LRX20A.hpp"
#include "RPLens.hpp"
#include "CLI.hpp"
#include "TempSens.hpp"
#include "TempSensorManager.hpp"
#include "debug_printf.hpp"

#include "timers.h"  // FreeRTOS software timers

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


// This is the global active debug UART for printf
UART_HandleTypeDef* g_DebugUart = &huart4;  // default (pick whatever you want)


extern I2C_HandleTypeDef hi2c2;  // make sure it's defined elsewhere (e.g., main.c/.cpp)
extern UART_HandleTypeDef huart1;  // make sure it's defined elsewhere (e.g., main.c/.cpp)
extern UART_HandleTypeDef huart3;  // make sure it's defined elsewhere (e.g., main.c/.cpp)
extern UART_HandleTypeDef huart4;  // make sure it's defined elsewhere (e.g., main.c/.cpp)
extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart8;  // make sure it's defined elsewhere (e.g., main.c/.cpp)

/* ============================================================================
 * GLOBAL DEVICE INSTANCES
 * ============================================================================ */
static Host*              g_host        = nullptr;
static LRX20A*            g_lrx20A      = nullptr;
static DayCam*            g_dayCam      = nullptr;
static RPLens*            g_rpLens      = nullptr;
static IRay*              g_iRay        = nullptr;
static CLI*               g_cli         = nullptr;
static TempSensorManager* g_tempSensors = nullptr;


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

/* ============================================================================
 * I2C BUS SCAN UTILITY
 * ============================================================================ */
void i2c_scan(I2C_HandleTypeDef* hi2c) {
    HAL_GPIO_WritePin(TMP_I2C_ADR0_GPIO_Port, TMP_I2C_ADR0_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TMP_I2C_ADR1_GPIO_Port, TMP_I2C_ADR1_Pin, GPIO_PIN_SET);

    DBG_INFO("\r\nI2C Bus Scan:\r\n");
    uint8_t found = 0;
    for (uint8_t a = 0x03; a < 0x78; ++a) {
        if (HAL_I2C_IsDeviceReady(hi2c, uint16_t(a << 1), 1, 10) == HAL_OK) {
        	DBG_INFO("  Found device at 0x%02X\r\n", a);
            found++;
        }
    }
    if (found == 0) {
    	DBG_INFO("  No devices found\r\n");
    }
    DBG_INFO("\r\n");
}


/* ============================================================================
 * APPLICATION ENTRY POINT (called from StartDefaultTask)
 * ============================================================================ */
extern "C" void cpp_app_main(void)
{
	DBG_INFO("cpp_app_main started\r\n");

    // Create a new task
    const osThreadAttr_t myTask_attributes = {
        .name = "myTask",
        .stack_size = 512 * 4,  // 2KB stack
        .priority = (osPriority_t) osPriorityNormal,
    };

    osThreadId_t myTaskHandle = osThreadNew(MyTaskFunction, nullptr, &myTask_attributes);
    if (myTaskHandle == nullptr) {
    	DBG_INFO("Failed to create MyTaskFunction\r\n");
    } else {
    	DBG_INFO("MyTaskFunction created successfully\r\n");
    }


    // You can return here — or sleep forever if this is the main task
    // osDelay(osWaitForever); // Optional if not returning
}



/* ============================================================================
 * MAIN APPLICATION TASK
 * ============================================================================ */

extern  void MyTaskFunction(void *argument)
{

	DBG_INFO("cpp_app_main started\r\n");
	DBG_INFO("Free heap before init: %u bytes\r\n", xPortGetFreeHeapSize());

    // ================================================================================
    // UART DEVICE INITIALIZATION
    // ================================================================================
    g_host = new Host(&huart1,115200);
    g_lrx20A = new LRX20A(&huart2, 115200);
    g_dayCam = new DayCam(&huart3, 9600);
    g_rpLens = new RPLens(&huart7, 19200);
    g_iRay = new IRay(&huart8, 115200);
    g_cli = new CLI(&huart4, 115200);

    // Set up device relationships
    g_host->setDayCam(g_dayCam);
    g_host->setLRF(g_lrx20A);
    g_host->setRPLens(g_rpLens);
    g_host->setIRay(g_iRay);
    g_host->setCli(g_cli);

    // Initialize each device (this starts UART reception)
    g_dayCam->Init();
    g_rpLens->Init();
    g_iRay->Init();
    g_cli->Init();
    g_lrx20A->Init();
    g_host->Init();

    g_iRay->NUC_Shutter();

    //DebugPrintf_Init(g_host->huart_);
    DebugPrintf_Init(g_cli->huart_);
    DBG_INFO("All UART devices initialized\r\n");

    // ================== SKIP FOR AUTOFOCUS TEST ==================
    #if 0  // Temporarily disabled

    // ================================================================================
    // TEMPERATURE SENSOR INITIALIZATION (ALL 4 CHANNELS)
    // ================================================================================
    printf("\r\n=== Initializing Temperature Sensors ===\r\n");

    // Scan I2C bus first
    i2c_scan(&hi2c2);

    // Create temperature sensor manager
    TempSensorManager::Config tempCfg = {
        .hi2c = &hi2c2,
        .muxAddr7bit = 0x73,        // PCA9546A multiplexer address
        .sensorAddr7bit = 0x44,     // STS4L sensor address (same on all channels)
        .i2cTimeoutMs = 20,
        .autoDetect = true          // Automatically detect connected sensors
    };

    g_tempSensors = new TempSensorManager(tempCfg);

    if (g_tempSensors != nullptr) {
        // Initialize all channels - this will detect which sensors are connected
        uint8_t sensorsFound = g_tempSensors->init();
        printf("Temperature sensors initialized: %u found\r\n", sensorsFound);

        // Read all sensors once to populate initial values
        osDelay(50);
        uint8_t readSuccess = g_tempSensors->readAll();
        printf("Initial temperature read: %u/%u successful\r\n", readSuccess, sensorsFound);

        // Print detailed status
        g_tempSensors->printStatus();

        // Copy temperatures to Host for UART transmission
        g_tempSensors->getAllTemperaturesScaled(g_host->Temp);


        for (uint8_t i = 0; i < 4; i++) {
            if (g_tempSensors->isConnected(i)) {
                printf("  Temp[%u] = %.2f degC (0x%04X)\r\n",
                       i,
                       g_tempSensors->getTemperature(i),
                       g_host->Temp[i]);
            } else {
                printf("  Temp[%u] = Not connected\r\n", i);
            }
        }
    } else {
        printf("ERROR: Failed to create temperature sensor manager\r\n");
    }


    // ================================================================================
    // ACCELEROMETER INITIALIZATION
    // ================================================================================
    printf("\r\n=== Initializing Accelerometer ===\r\n");

    KX134_SPI acc({ .hspi = &hspi2, .timeout_ms = 10 });

    uint8_t id = 0;
    if (acc.whoAmI(id) == HAL_OK) {
        printf("KX134 WHO_AM_I: 0x%02X\r\n", id);

        if (acc.init(KX134_SPI::RANGE_8G, 0x03) == HAL_OK) {
            acc.setSensitivityLSBperG(2048.0f);
            printf("KX134 initialized successfully\r\n");

            int16_t x, y, z;
            if (acc.readRaw(x, y, z) == HAL_OK) {
                g_host->Imu[0] = x;
                g_host->Imu[1] = y;
                g_host->Imu[2] = z;

                float gx, gy, gz;
                acc.countsToG(x, y, z, gx, gy, gz);
                printf("Initial reading: %.3f, %.3f, %.3f g\r\n", gx, gy, gz);
            }
        } else {
            printf("KX134 initialization failed\r\n");
        }
    } else {
        printf("KX134 communication failed\r\n");
    }


#endif
// =============================================================


	DBG_INFO("\r\n=== System Initialization Complete ===\r\n");
	DBG_INFO("Free heap after init2: %u bytes\r\n", xPortGetFreeHeapSize());

    // ================================================================================
    // MAIN LOOP
    // ================================================================================
    uint32_t loopCount = 0;


    for (;;)
    {
      //  HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
        osDelay(2000);
    }

//    for (;;)
//    {
//
//
// //       g_fpga->SendCommand(tempB, 4);
//
//        HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
//
//
//        // Read all temperature sensors every loop
//        if (g_tempSensors != nullptr) {
//            uint8_t readCount = g_tempSensors->readAll();
//
//            // Update Host temperature array for UART transmission
//            g_tempSensors->getAllTemperaturesScaled(g_host->Temp);
//
//            // Print every 10 loops
//            if ((loopCount % 10) == 0) {
//                printf("\r\n--- Temperature Reading (Loop %lu) ---\r\n", loopCount);
//                for (uint8_t i = 0; i < 4; i++) {
//                    if (g_tempSensors->isConnected(i)) {
//                        printf("Ch%u: %.2f degC (0x%04X) %s\r\n",// °C
//                               i,
//                               g_tempSensors->getTemperature(i),
//                               g_host->Temp[i],
//                               g_tempSensors->getStatus(i).lastReadOK ? "v" : "x");
//                    }
//                }
//                printf("Read: %u/%u successful\r\n",
//                       readCount,
//                       g_tempSensors->getConnectedCount());
//            }
//        }
//
//
//
//        //  Read accelerometer periodically
//        if ((loopCount % 5) == 0) {
//            int16_t x, y, z;
//            if (acc.readRaw(x, y, z) == HAL_OK) {
//                g_host->Imu[0] = x;
//                g_host->Imu[1] = y;
//                g_host->Imu[2] = z;
//
//                float gx, gy, gz;
//                acc.countsToG(x, y, z, gx, gy, gz);
//                printf("IMU: %.3f, %.3f, %.3f g\r\n", gx, gy, gz);
//            }
//        }
//
//
//        loopCount++;
//        osDelay(2000);  // 1 second delay
//        }
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

/* ============================================================================
 * AUTOFOCUS - FIND SHARPEST PEAK
 * ============================================================================ */

typedef struct {
    int32_t step;
    uint16_t position;
    uint64_t score;
    int64_t gradientLeft;
    int64_t gradientRight;
    int64_t sharpness;
} PeakAnalysis_t;





