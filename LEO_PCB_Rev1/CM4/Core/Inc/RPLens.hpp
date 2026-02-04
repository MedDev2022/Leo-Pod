#ifndef SRC_RPLENS_H_
#define SRC_RPLENS_H_

#pragma once
#include "UartEndpoint.hpp"
#include <queue>
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include "main.h"




class RPLens : public UartEndpoint {
public:

    explicit RPLens(UART_HandleTypeDef* huart, uint32_t baudrate = 19200);

    void Init();

    // Device commands
    void ZoomStop();
    void ZoomIn();
    void ZoomOut();
    void SetZoomPosition(int pos);
    void SetFocusNear();
    void SetFocusFar();
    void SetContinuousFocusFar();
    void SetFocusStop();
    void SetContinuousFocusNear();
    void SetFocusIncrement(int pos);
    void SetFocusDecrement(int pos);
    void FocusStop();
    void SetZoomAndFocusPosition(int zoomPos, int focusPos);
    void SetFastFocusPosition(int focusPos);
    void SetZoomFocusSpeed(uint8_t speed);

    void handleZoomIn(void);
    void handleZoomOut(void);
    void handleZoomStop(void);

    // Report mode: lens continuously sends position
    void SetReportMode(uint8_t mode);  // 0=off, 1=on-change, 2=always
    void EnableContinuousReporting();   // Shortcut for mode=2

    // Position from continuous reports (updated in processRxData)
    int16_t getLastFocusPosition() const { return lastFocusPos_; }
    int16_t getLastZoomPosition() const  { return lastZoomPos_; }
    uint32_t getLastPositionTimestamp() const { return lastPosTimestamp_; }

    // Command processing
    void ProcessData();

    // Autofocus - main entry point
    void handleAutofocus();

    // Individual phases (can be called standalone for testing)
    // Phase 1: Continuous sweep - finds peak region (~1.2s)
    int32_t sweepCapture(uint16_t startPos, uint16_t endPos,
                         uint32_t settleMs = 500, uint32_t timeoutMs = 5000);

    // Phase 2: Step-and-capture - accurate search in a range (~1.3s)
    int32_t fineSearch(uint16_t startPos, uint16_t endPos,
                       uint16_t stepSize = 25, uint32_t capturesPerStep = 2);

    // Phase 3: Golden section - converges to precise focus (~0.6s)
    int32_t goldenSearch(uint16_t startPos, uint16_t endPos,
                         uint16_t tolerance = 3, uint32_t capturesPerEval = 3);

    // Hill climbing autofocus - fast convergence from any starting point
    int32_t hillClimbFocus(uint16_t startPos, uint16_t stepSize = 200,
                           uint16_t minStep = 5, uint32_t settleMs = 40);
    void handleQuickAutofocus();  // Quick AF using hill climbing from current position

    // Smooth autofocus - two-pass sweep with deceleration
    void handleSmoothAutofocus();

    // Capture score at a specific position (move, settle, average N captures)
    uint64_t scoreAtPosition(uint16_t position, uint32_t numCaptures = 2,
                             uint32_t settleMs = 30);

    // Single/multi capture (low-level mailbox interface)
    bool requestSingleCapture(uint64_t &sumX, uint64_t &sumY);
    uint32_t requestMultiCapture(uint32_t count, uint64_t *scoresX, uint64_t *scoresY);

    // Diagnostic
    void measureMotorSpeed(uint16_t startPos, uint16_t endPos);
    void debugScanSingleStep();  // Debug: step-by-step scan with detailed XY printout
    void debugFullScan(uint16_t stepSize = 50);  // Debug: full 0->4000 scan with X/Y at each step

    // Legacy step-based scan (kept for comparison/debugging)
    void runFocusScan();
    void runFocusScanMulti(uint32_t capturesPerStep);

    // Smooth scan (mailbox-based, limited by mailbox size)
    int32_t runSmoothScan(uint16_t startPos, uint16_t endPos, uint32_t captureCount);


    // Parse responses for zoom and focus positions
    std::map<std::string, int> ParseResponse(const std::vector<uint8_t>& command);

protected:
//    void onReceiveByte(uint8_t byte) override;
//    void processIncoming() override;
    void processRxData(const uint8_t* data, uint16_t length) override;

private:

    // ---- Autofocus configuration ----
    static constexpr uint16_t AF_START_POSITION     = 0;
    static constexpr uint16_t AF_STOP_POSITION      = 4000;
    static constexpr uint16_t AF_STEP_INCREMENT     = 50;
    static constexpr uint8_t  AF_NUM_STEPS          = 60;
    static constexpr uint32_t AF_CAPTURE_TIMEOUT_MS = 200;
    static constexpr uint32_t AF_MOTOR_SETTLE_MS    = 30;

    // ---- Sweep capture storage ----
    static constexpr uint32_t MAX_SWEEP_POINTS      = 128;
    static constexpr uint32_t SWEEP_STABLE_COUNT    = 8;
    static constexpr uint64_t SWEEP_STABLE_THRESH   = 5000;

    struct SweepPoint {
        uint32_t elapsed_ms;
        int16_t  position;    // Actual position from lens feedback
        uint64_t score;
    };

    SweepPoint sweepPoints_[MAX_SWEEP_POINTS];
    uint32_t   sweepCount_ = 0;
    uint32_t   sweepMotionStart_ = 0;
    uint32_t   sweepMotionEnd_ = 0;

    // ---- Legacy step-based storage ----
    struct AF_StepResult {
        uint16_t focusPosition;
        uint64_t sumX;
        uint64_t sumY;
        uint64_t totalScore;
        uint8_t  valid;
    };

    AF_StepResult afResults_[AF_NUM_STEPS];

    // ---- Internal helpers ----
    void printAutofocusResults();
    void printRawXYData();
    int8_t findBestStep();

    uint32_t nextSeq();
    uint32_t afSeq_ = 0;

    std::deque<uint8_t> messageBuffer_;
    uint8_t byte_;

    struct InputMessageInfo {
        uint8_t sync = 0;
        uint8_t commandID = 0;
        uint8_t dataLengthLSB = 0;
        uint8_t dataLengthMSB = 0;
        std::vector<uint8_t> messageBytes;
        int messagePointer = -1;
        int dataLength = 0;

        void BuildMessageContainer();
        std::vector<uint8_t> GetMessage();
        void Reset();
    };

    uint8_t rxBuffer[64];  // Adjust size as necessary for your data

    // ---- Position cache from continuous reports ----
    volatile int16_t  lastFocusPos_    = -1;  // -1 = no data yet
    volatile int16_t  lastZoomPos_     = -1;
    volatile uint32_t lastPosTimestamp_ = 0;   // osKernelGetTickCount when last updated

    std::queue<std::vector<uint8_t>> inputDataQueue;
    InputMessageInfo inputMessageInfo;

//    void SendCommand(const std::vector<uint8_t>& command);
    void UpdateCRC(std::vector<uint8_t>& cmd);

};



#endif /* SRC_RPLENS_H_ */
