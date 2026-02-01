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
//#define SHARED_FOCUS_IPC_OWNER
#include "shared_focus_ipc.h"

//__attribute__((section(".shared_ram")))
//volatile SharedMailbox_t g_mb = {0};

//#define g_mb (*(volatile SharedMailbox_t*)0x30010000)

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


uint8_t rxByte;

/* ============================================================================
 * AUTOFOCUS CONFIGURATION
 * ============================================================================ */
#define AF_START_POSITION       0       /* Starting focus position */
#define AF_STOP_POSITINON			4000	/* Position to stop at if autofocus fails */
#define AF_STEP_INCREMENT       100      /* Focus increment per step */
#define AF_NUM_STEPS            40      /* Number of steps (matches FOCUS_STEPS) */
#define AF_CAPTURE_TIMEOUT_MS   1000    /* Timeout waiting for FPGA data */
#define AF_MOTOR_SETTLE_MS      100     /* Time to wait after motor move */


/* ============================================================================
 * AUTOFOCUS RESULTS STORAGE
 * ============================================================================ */
typedef struct {
    uint16_t focusPosition;     /* Focus position at this step */
    uint64_t  sumX;              /* Sum of X values */
    uint64_t  sumY;              /* Sum of Y values */
    uint64_t  totalScore;        /* sumX + sumY (sharpness metric) */
    uint8_t  valid;             /* 1 if capture succeeded */
} AF_StepResult_t;

static AF_StepResult_t af_results[AF_NUM_STEPS];




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
 * FORWARD DECLARATIONS
 * ============================================================================ */
void PrintAutofocusResults(void);
void PrintRawXYData(void);
void Print64(int64_t val);
int32_t FindAndMoveToBestFocus(RPLens* rpLens);
int32_t FindBestFocusAdvanced(RPLens* rpLens);
int32_t FindSharpestPeak(RPLens* rpLens);


/* ============================================================================
 * FIND BEST FOCUS AND MOVE TO IT
 * ============================================================================ */
int32_t FindAndMoveToBestFocus(RPLens* rpLens)
{
    int32_t bestStep = -1;
    uint64_t bestScore = 0;

    /* Find maximum score */
    for (uint32_t i = 0; i < AF_NUM_STEPS; i++) {
        if (!af_results[i].valid) {
            continue;
        }

        if (af_results[i].totalScore > bestScore) {
            bestScore = af_results[i].totalScore;
            bestStep = (int32_t)i;
        }
    }

    /* Check if we found any valid result */
    if (bestStep < 0) {
        printf("\r\nERROR: No valid focus positions found!\r\n");
        return -1;
    }

    /* Print result */
    printf("\r\n========================================\r\n");
    printf("       BEST FOCUS FOUND\r\n");
    printf("========================================\r\n");
    printf("  Step: %ld\r\n", bestStep);
    printf("  Position: %u\r\n", af_results[bestStep].focusPosition);
    printf("  Score: %iid", af_results[bestStep].totalScore);
    printf("\r\n========================================\r\n");

    /* Move to best position */
    if (rpLens != nullptr) {
        printf("Moving to best focus position %u...\r\n", af_results[bestStep].focusPosition);
        rpLens->SetZoomAndFocusPosition(3200, af_results[bestStep].focusPosition);
        osDelay(AF_MOTOR_SETTLE_MS);
        printf("Autofocus complete!\r\n");
    } else {
        printf("WARNING: rpLens is NULL, cannot move to best position\r\n");
    }

    return bestStep;
}

int32_t FindBestFocusAdvanced(RPLens* rpLens)
{
    int32_t peakStep = -1;           /* Where gradient changes sign */
    int32_t maxGradientStep = -1;    /* Steepest positive slope */
    int32_t maxScoreStep = -1;       /* Highest absolute score */

    int64_t maxGradient = 0;
    uint64_t maxScore = 0;
    int64_t prevGradient = 0;

    printf("\r\n========================================\r\n");
    printf("       AUTOFOCUS ANALYSIS\r\n");
    printf("========================================\r\n");
    printf("Step | Position | Score        | Gradient\r\n");
    printf("-----|----------|--------------|----------\r\n");

    for (uint32_t i = 0; i < AF_NUM_STEPS; i++) {
        if (!af_results[i].valid) {
            printf("%4lu | %8u | INVALID      | -\r\n", i, af_results[i].focusPosition);
            continue;
        }

        /* Calculate gradient (change from previous step) */
        int64_t gradient = 0;
        if (i > 0 && af_results[i-1].valid) {
            gradient = (int64_t)af_results[i].totalScore - (int64_t)af_results[i-1].totalScore;
        }

        /* Print row */
        printf("%4lu | %8u | %lld", i, af_results[i].focusPosition,af_results[i].totalScore);
        printf(" | %lld",gradient);
        printf("\r\n");

        /* Track maximum score */
        if (af_results[i].totalScore > maxScore) {
            maxScore = af_results[i].totalScore;
            maxScoreStep = (int32_t)i;
        }

        /* Track maximum positive gradient (steepest climb) */
        if (gradient > maxGradient) {
            maxGradient = gradient;
            maxGradientStep = (int32_t)i;
        }

        /* Detect peak: gradient changes from positive to negative */
        if (i > 0 && prevGradient > 0 && gradient < 0) {
            /* Peak is at previous step (where it was still rising) */
            peakStep = (int32_t)(i - 1);
        }

        prevGradient = gradient;
    }

    printf("========================================\r\n\r\n");

    /* Determine best step */
    int32_t bestStep = -1;
    const char* method = "";

    /* Priority: Peak > MaxScore > MaxGradient */
    if (peakStep >= 0 && af_results[peakStep].valid) {
        bestStep = peakStep;
        method = "PEAK (gradient sign change)";
    } else if (maxScoreStep >= 0) {
        bestStep = maxScoreStep;
        method = "MAX SCORE";
    } else if (maxGradientStep >= 0) {
        bestStep = maxGradientStep;
        method = "MAX GRADIENT";
    }

    /* Print analysis results */
    printf("Analysis Results:\r\n");
    if (maxScoreStep >= 0) {
        printf("  Max Score at step %ld (pos %u): ", maxScoreStep, af_results[maxScoreStep].focusPosition);
        printf("%lld",maxScore);
        printf("\r\n");
    }
    if (maxGradientStep >= 0) {
        printf("  Max Gradient at step %ld (pos %u): ", maxGradientStep, af_results[maxGradientStep].focusPosition);
        printf("%lld",maxGradient);
        printf("\r\n");
    }
    if (peakStep >= 0) {
        printf("  Peak detected at step %ld (pos %u)\r\n", peakStep, af_results[peakStep].focusPosition);
    }

    if (bestStep < 0) {
        printf("\r\nERROR: No valid focus position found!\r\n");
        return -1;
    }

    /* Print final decision */
    printf("\r\n========================================\r\n");
    printf("       BEST FOCUS: %s\r\n", method);
    printf("========================================\r\n");
    printf("  Step: %ld\r\n", bestStep);
    printf("  Position: %u\r\n", af_results[bestStep].focusPosition);
    printf("  Score: %lld",af_results[bestStep].totalScore);

    printf("\r\n========================================\r\n");

    /* Move to best position */
    if (rpLens != nullptr) {
        printf("Moving to focus position %u...\r\n", af_results[bestStep].focusPosition);
        rpLens->SetZoomAndFocusPosition(3200, af_results[bestStep].focusPosition);
        osDelay(AF_MOTOR_SETTLE_MS);
        printf("Autofocus complete!\r\n");
    }

    return bestStep;
}

/* ============================================================================
 * REQUEST CAPTURE FROM M7
 * ============================================================================ */
static uint8_t RequestCapture(uint8_t step, uint32_t seq)
{

	//g_mb.rsp_seq = 0;


//    printf("[DEBUG] RequestCapture: step=%d, seq=%lu\r\n", step, seq);

    /* Clear response flag */
    g_mb.rsp_ready = 0;

    /* Set request parameters */
    g_mb.req_step = step;
    g_mb.req_seq = seq;

    /* Memory barrier to ensure writes complete */
    __DMB();

    /* Send request command */
    g_mb.req_cmd = MB_REQ_CAPTURE;

    /* Another barrier to ensure command is visible */
    __DMB();

//    printf("[DEBUG] Request sent: cmd=%lu, step=%lu, seq=%lu\r\n",
//           g_mb.req_cmd, g_mb.req_step, g_mb.req_seq);

    /* Wait for response with timeout */
    uint32_t startTime = osKernelGetTickCount();
    uint32_t lastPrint = startTime;

    while (1) {
        uint32_t now = osKernelGetTickCount();

        /* Print status every 200ms */
        if ((now - lastPrint) >= 200) {
//            printf("[DEBUG] Waiting... rsp_ready=%lu, rsp_seq=%lu, elapsed=%lums\r\n",
//                   g_mb.rsp_ready, g_mb.rsp_seq, (now - startTime));
            lastPrint = now;
        }


        /* Check timeout */
        if ((now - startTime) > AF_CAPTURE_TIMEOUT_MS) {
        //    printf("  ERROR: Timeout waiting for capture (seq=%lu)\r\n", seq);
            return 0;
        }

        /* Memory barrier before reading */
        __DMB();


        /* Check if response ready with matching sequence */
        if (g_mb.rsp_ready == 1 && g_mb.rsp_seq == seq) {
      //      printf("[DEBUG] Response received!\r\n");
            return 1;
        }

  //      osDelay(10);
    }
}


/* ============================================================================
 * RUN AUTOFOCUS SCAN
 * ============================================================================ */
void RunAutofocusScan(RPLens* rpLens)
{
    uint32_t seq = 0;

//    printf("\r\n");
//    printf("========================================\r\n");
//    printf("       AUTOFOCUS SCAN STARTING\r\n");
//    printf("========================================\r\n");
//    printf("  Steps: %d\r\n", AF_NUM_STEPS);
//    printf("  Start Position: %d\r\n", AF_START_POSITION);
//    printf("  Step Increment: %d\r\n", AF_STEP_INCREMENT);
//    printf("  g_mb address: 0x%08lX\r\n", (uint32_t)&g_mb);
//    printf("========================================\r\n\r\n");

    /* Clear results */
    memset(af_results, 0, sizeof(af_results));

    /* Move to start position */
//    printf("[DEBUG] Moving to start position %d...\r\n", AF_START_POSITION);
    if (rpLens != nullptr) {
    //    printf("[DEBUG] Calling SetZoomAndFocusPosition...\r\n");
        rpLens->SetZoomAndFocusPosition(3200, AF_START_POSITION);
   //     printf("[DEBUG] SetZoomAndFocusPosition done\r\n");
    } else {
    //    printf("[DEBUG] rpLens is NULL, skipping motor\r\n");
    }
    osDelay(500);

    /* Clear mailbox request */
//    printf("[DEBUG] Clearing mailbox...\r\n");
    g_mb.req_cmd = MB_IDLE;
    g_mb.req_seq = 0;
    __DMB();
    osDelay(100);

    /* Run scan */
    for (uint8_t step = 0; step < AF_NUM_STEPS; step++) {
        uint16_t currentPosition = AF_START_POSITION + (step * AF_STEP_INCREMENT);

//        printf("\r\n----------------------------------------\r\n");
//        printf("Step %d/%d - Focus Position: %d\r\n", step + 1, AF_NUM_STEPS, currentPosition);
//        printf("----------------------------------------\r\n");

        /* Move focus (skip for step 0) */
        if (step > 0 && rpLens != nullptr) {
 //           printf("  Moving focus +%d...\r\n", AF_STEP_INCREMENT);
            //rpLens->SetFocusIncrement(AF_STEP_INCREMENT);
            rpLens->SetZoomAndFocusPosition(3200, currentPosition);
            osDelay(AF_MOTOR_SETTLE_MS);
        }

        /* Store position */
        af_results[step].focusPosition = currentPosition;

        /* Request capture */
        seq++;
 //       printf("  Requesting capture (seq=%lu)...\r\n", seq);

        HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
        if (RequestCapture(step, seq)) {
            HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
            __DMB();
            HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
            af_results[step].sumX = g_mb.sumX;
            af_results[step].sumY = g_mb.sumY;
            af_results[step].totalScore = g_mb.sumX + g_mb.sumY;
            af_results[step].valid = 1;

        	PrintRawXYData();
    //    	osDelay(500);

//            printf("  OK - Sum X: %lld, Sum Y: %lld, Score: %lld\r\n",
//                   af_results[step].sumX,
//                   af_results[step].sumY,
//                   af_results[step].totalScore);
        } else {
            af_results[step].valid = 0;
   //         printf("  FAILED - No response from M7\r\n");
        }

        g_mb.req_cmd = MB_IDLE;
        __DMB();
    }

    PrintAutofocusResults();
}




/* ============================================================================
 * RUN AUTOFOCUS SCAN
 * ============================================================================ */
void RunAutofocusScan_1(RPLens* rpLens)
{
    uint32_t seq = 0;

//    printf("\r\n");
//    printf("========================================\r\n");
//    printf("       AUTOFOCUS SCAN STARTING\r\n");
//    printf("========================================\r\n");
//    printf("  Steps: %d\r\n", AF_NUM_STEPS);
//    printf("  Start Position: %d\r\n", AF_START_POSITION);
//    printf("  Step Increment: %d\r\n", AF_STEP_INCREMENT);
//    printf("  g_mb address: 0x%08lX\r\n", (uint32_t)&g_mb);
//    printf("========================================\r\n\r\n");

    /* Clear results */
    memset(af_results, 0, sizeof(af_results));

    /* Move to start position */
//    printf("[DEBUG] Moving to start position %d...\r\n", AF_START_POSITION);
    if (rpLens != nullptr) {
    //    printf("[DEBUG] Calling SetZoomAndFocusPosition...\r\n");
        rpLens->SetZoomAndFocusPosition(3200, AF_START_POSITION);
   //     printf("[DEBUG] SetZoomAndFocusPosition done\r\n");
    } else {
    //    printf("[DEBUG] rpLens is NULL, skipping motor\r\n");
    }
    osDelay(500);



    /* Clear mailbox request */
//    printf("[DEBUG] Clearing mailbox...\r\n");
    g_mb.req_cmd = MB_IDLE;
    g_mb.req_seq = 0;
    __DMB();
    osDelay(100);

    rpLens->SetZoomAndFocusPosition(3200, AF_STOP_POSITINON);

    /* Run scan */
    for (uint8_t step = 0; step < AF_NUM_STEPS; step++) {
        uint16_t currentPosition = AF_START_POSITION + (step * AF_STEP_INCREMENT);

//        printf("\r\n----------------------------------------\r\n");
//        printf("Step %d/%d - Focus Position: %d\r\n", step + 1, AF_NUM_STEPS, currentPosition);
//        printf("----------------------------------------\r\n");

//        /* Move focus (skip for step 0) */
//        if (step > 0 && rpLens != nullptr) {
// //           printf("  Moving focus +%d...\r\n", AF_STEP_INCREMENT);
//            //rpLens->SetFocusIncrement(AF_STEP_INCREMENT);
//            rpLens->SetZoomAndFocusPosition(3200, currentPosition);
//            osDelay(AF_MOTOR_SETTLE_MS);
//        }

       // osDelay(AF_MOTOR_SETTLE_MS);
           /* Store position */
        af_results[step].focusPosition = currentPosition;

        /* Request capture */
        seq++;
 //       printf("  Requesting capture (seq=%lu)...\r\n", seq);

        HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
        if (RequestCapture(step, seq)) {
            HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
            __DMB();
            HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
            af_results[step].sumX = g_mb.sumX;
            af_results[step].sumY = g_mb.sumY;
            af_results[step].totalScore = g_mb.sumX + g_mb.sumY;
            af_results[step].valid = 1;

        //	PrintRawXYData();
    //    	osDelay(500);

//            printf("  OK - Sum X: %lld, Sum Y: %lld, Score: %lld\r\n",
//                   af_results[step].sumX,
//                   af_results[step].sumY,
//                   af_results[step].totalScore);
        } else {
            af_results[step].valid = 0;
   //         printf("  FAILED - No response from M7\r\n");
        }

        g_mb.req_cmd = MB_IDLE;
        __DMB();
    }

    PrintAutofocusResults();
}


/* ============================================================================
 * PRINT RESULTS SUMMARY
 * ============================================================================ */
void PrintAutofocusResults(void)
{
    int8_t bestStep = -1;
    int64_t bestScore = 0;

    DBG_INFO("\r\n========================================\r\n");
    DBG_INFO("       AUTOFOCUS RESULTS SUMMARY\r\n");
    DBG_INFO("========================================\r\n");

    for (uint8_t step = 0; step < AF_NUM_STEPS; step++) {
        if (af_results[step].valid) {
        	DBG_INFO("Step %d: Pos=%d, SumX=%lld, SumY=%lld, Score=%lld\r\n",
                   step, af_results[step].focusPosition,
                   (long long)af_results[step].sumX,
                   (long long)af_results[step].sumY,
                   (long long)af_results[step].totalScore);

            int64_t absScore = af_results[step].totalScore;
            if (absScore < 0) absScore = -absScore;

            if (bestStep < 0 || absScore > bestScore) {
                bestScore = absScore;
                bestStep = step;
            }
        } else {
        	DBG_INFO("Step %d: FAILED\r\n", step);
        }
    }

    if (bestStep >= 0) {
    	DBG_INFO("\r\n>>> BEST FOCUS: Step %d, Position %d <<<\r\n",
               bestStep, af_results[bestStep].focusPosition);
    }
    printf("========================================\r\n");
}

void PrintRawXYData(void)
{
	DBG_INFO("\r\n--- RAW X/Y DATA ---\r\n");
    __DMB();
    for (int i = 0; i < XY_SAMPLES; i++) {
    	DBG_INFO("%d,%u,%u\r\n", i, (uint32_t)g_mb.x[i], (uint32_t)g_mb.y[i]);
    }
    DBG_INFO("--- END ---\r\n");
}





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


	HAL_GPIO_WritePin(TMP_I2C_ADR0_GPIO_Port, TMP_I2C_ADR0_Pin,GPIO_PIN_SET);
	HAL_GPIO_WritePin(TMP_I2C_ADR1_GPIO_Port, TMP_I2C_ADR1_Pin,GPIO_PIN_SET);


    printf("\r\nI2C Bus Scan:\r\n");
    uint8_t found = 0;
    for (uint8_t a = 0x03; a < 0x78; ++a) {
        if (HAL_I2C_IsDeviceReady(hi2c, uint16_t(a << 1), 1, 10) == HAL_OK) {
            printf("  Found device at 0x%02X\r\n", a);
            found++;
        }
    }
    if (found == 0) {
        printf("  No devices found\r\n");
    }
    printf("\r\n");
}


// Global pointers to device instances (optional, for easy access)
static Host* g_host = nullptr;
static LRX20A* g_lrx20A = nullptr;
static DayCam* g_dayCam = nullptr;
static RPLens* g_rpLens = nullptr;
static IRay* g_iRay = nullptr;
static CLI* g_cli = nullptr;
//static Host* g_fpga = nullptr;

// Global temperature sensor manager
static TempSensorManager* g_tempSensors = nullptr;


void PrintResultsToHost(void)
{
    printf("step,idx,x,y\r\n");

        for (int i = 0; i < 10; i++) {
        	printf("%d,%ld,%ld\r\n", i, (long)g_mb.x[i], (long)g_mb.y[i]);
        }

}

extern  void MyTaskFunction(void *argument)
{


    printf("cpp_app_main started\r\n");
    printf("Free heap before init: %u bytes\r\n", xPortGetFreeHeapSize());

    // ================================================================================
    // UART DEVICE INITIALIZATION
    // ================================================================================
    g_host = new Host(&huart1,115200);
    g_host->Init();

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

    DebugPrintf_Init(g_host->huart_);


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

printf("\r\n=== SKIPPING SENSORS - AUTOFOCUS TEST ===\r\n");
printf("Free heap: %u bytes\r\n", xPortGetFreeHeapSize());

    printf("\r\n=== System Initialization Complete ===\r\n");
    printf("Free heap after init2: %u bytes\r\n", xPortGetFreeHeapSize());

    // ================================================================================
    // MAIN LOOP
    // ================================================================================
    uint32_t loopCount = 0;

    uint8_t tempB[10] = {1, 2, 3, 4};
 //   g_fpga->SendCommand(tempB, 4);


//    while(1){
//
//        HAL_GPIO_TogglePin(MCU_LED_2_GPIO_Port, MCU_LED_2_Pin);
//        osDelay(1000);  // 1 second delay
//     //  HAL_UART_Transmit(&huart7, tempB, 3, 100);
//    }


    static uint32_t lastFireCount = 0;
//while (1){
//
//    printf("Free heap in loop : %u bytes\r\n", xPortGetFreeHeapSize());
//    osDelay(3000);
//}

    /* Clear mailbox BEFORE releasing M4 */
    __DMB();  // Ensure writes complete
    memset((void*)&g_mb, 0, sizeof(g_mb));
    __DMB();  // Ensure writes complete
    osDelay(2000);

    printf("\r\nM4 Ready - Starting Autofocus\r\n");

    // ===== ADD THESE LINES =====
 //   osDelay(2000);              // Wait for M7 to be ready
 //   RunAutofocusScan(g_rpLens); // Run autofocus ONCE

  //  FindAndMoveToBestFocus(g_rpLens);

//    osDelay(2000);
//    FindBestFocusAdvanced(g_rpLens);
//
//
//
//    osDelay(2000);
//
//    FindSharpestPeak(g_rpLens);

    for (;;)
    {
        HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
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


#define FOCUS_STEPS  10

static int32_t X_all[FOCUS_STEPS][XY_SAMPLES];
static int32_t Y_all[FOCUS_STEPS][XY_SAMPLES];




void FocusScan_Run(void)
{
    uint32_t seq = 0;

    for (uint8_t step = 0; step < FOCUS_STEPS; step++)
    {
        // 1) Move RP focus (10 steps)
    	g_rpLens->SetFocusIncrement(10);

        // 2) Request capture from M7
        g_mb.rsp_ready = 0;
        g_mb.req_step  = step;
        g_mb.req_seq   = ++seq;
        g_mb.req_cmd   = MB_REQ_CAPTURE;

        // optional: wake M7 via HSEM / event (recommended)
        // HSEM_NotifyM7();

        // 3) Wait for response
        while (g_mb.rsp_ready == 0) {
            // if FreeRTOS: osDelay(1) or wait on thread flags
        }

        // 4) Validate response
        if (g_mb.rsp_seq != seq || g_mb.rsp_step != step) {
            // handle error: retry or abort
            step--; // simple retry
            continue;
        }

        // 5) Copy X/Y out (so mailbox can be reused next step)
        for (int i = 0; i < XY_SAMPLES; i++) {
            X_all[step][i] = g_mb.x[i];
            Y_all[step][i] = g_mb.y[i];
        }

        // Clear request (optional)
        g_mb.req_cmd = MB_IDLE;
    }

    // 6) Send to host over terminal
    PrintResultsToHost();


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

static PeakAnalysis_t peakAnalysis[AF_NUM_STEPS];

int32_t FindSharpestPeak(RPLens* rpLens)
{
    int32_t sharpestStep = -1;
    int64_t maxSharpness = 0;
    int32_t maxScoreStep = -1;
    uint64_t maxScore = 0;

    /* First pass: calculate all gradients */
    for (uint32_t i = 0; i < AF_NUM_STEPS; i++) {
        peakAnalysis[i].step = i;
        peakAnalysis[i].position = af_results[i].focusPosition;
        peakAnalysis[i].score = af_results[i].totalScore;
        peakAnalysis[i].gradientLeft = 0;
        peakAnalysis[i].gradientRight = 0;
        peakAnalysis[i].sharpness = 0;

        if (!af_results[i].valid) {
            continue;
        }

        /* Gradient from left (previous step) */
        if (i > 0 && af_results[i-1].valid) {
            peakAnalysis[i].gradientLeft = (int64_t)af_results[i].totalScore -
                                           (int64_t)af_results[i-1].totalScore;
        }

        /* Gradient to right (next step) */
        if (i < AF_NUM_STEPS - 1 && af_results[i+1].valid) {
            peakAnalysis[i].gradientRight = (int64_t)af_results[i+1].totalScore -
                                            (int64_t)af_results[i].totalScore;
        }

        peakAnalysis[i].sharpness = peakAnalysis[i].gradientLeft - peakAnalysis[i].gradientRight;

        /* Track max score as fallback */
        if (af_results[i].totalScore > maxScore) {
            maxScore = af_results[i].totalScore;
            maxScoreStep = i;
        }
    }

    /* Print analysis table */
    printf("\r\n");
    printf("================================================================\r\n");
    printf("                   PEAK SHARPNESS ANALYSIS\r\n");
    printf("================================================================\r\n");
    printf("Step | Pos  | Score      | GradL     | GradR     | Sharp\r\n");
    printf("-----|------|------------|-----------|-----------|----------\r\n");

    for (uint32_t i = 0; i < AF_NUM_STEPS; i++) {
        if (!af_results[i].valid) {
            printf("%4lu | %4u | INVALID\r\n", i, af_results[i].focusPosition);
            continue;
        }

        /* Valid peak: positive left gradient AND negative right gradient */
        char marker = ' ';
        if (peakAnalysis[i].gradientLeft > 0 && peakAnalysis[i].gradientRight < 0) {
            if (peakAnalysis[i].sharpness > maxSharpness) {
                maxSharpness = peakAnalysis[i].sharpness;
                sharpestStep = i;
            }
            marker = '*';
        }

        printf("%4lu | %4u | %10llu | %9lld | %9lld | %9lld %c\r\n",
               i,
               peakAnalysis[i].position,
               peakAnalysis[i].score,
               peakAnalysis[i].gradientLeft,
               peakAnalysis[i].gradientRight,
               peakAnalysis[i].sharpness,
               marker);
    }

    printf("================================================================\r\n");
    printf("  * = Peak candidate (+ left, - right)\r\n");
    printf("================================================================\r\n\r\n");

    /* Determine best step */
    int32_t bestStep = -1;
    const char* method = "";

    if (sharpestStep >= 0) {
        bestStep = sharpestStep;
        method = "SHARPEST PEAK";
    } else if (maxScoreStep >= 0) {
        bestStep = maxScoreStep;
        method = "MAX SCORE (no peak)";
    }

    /* Print results */
    printf("Summary:\r\n");
    if (sharpestStep >= 0) {
        printf("  Sharpest: step %d, pos %u, sharpness=%lld\r\n",
               sharpestStep,
               af_results[sharpestStep].focusPosition,
               maxSharpness);
    } else {
        printf("  No clear peak found\r\n");
    }
    printf("  Max Score: step %d, pos %u, score=%llu\r\n",
           maxScoreStep,
           af_results[maxScoreStep].focusPosition,
           maxScore);

    if (bestStep < 0) {
        printf("\r\nERROR: No valid focus position found!\r\n");
        return -1;
    }

    /* Print final decision */
    printf("\r\n========================================\r\n");
    printf("  BEST FOCUS: %s\r\n", method);
    printf("========================================\r\n");
    printf("  Step: %d\r\n", bestStep);
    printf("  Position: %u\r\n", af_results[bestStep].focusPosition);
    printf("  Score: %llu\r\n", af_results[bestStep].totalScore);
    if (sharpestStep >= 0) {
        printf("  Sharpness: %lld\r\n", maxSharpness);
    }
    printf("========================================\r\n");

    /* Move to best position */
    if (rpLens != nullptr) {
        printf("Moving to position %u...\r\n", af_results[bestStep].focusPosition);
        rpLens->SetZoomAndFocusPosition(3200, af_results[bestStep].focusPosition);
        osDelay(AF_MOTOR_SETTLE_MS);
        printf("Autofocus complete!\r\n");
    }

    return bestStep;
}




