#pragma once
#include <cstdint>
#include "main.h"

enum class TecMode  : uint8_t { OFF=0, COOL, HEAT };
enum class TecState : uint8_t { IDLE=0, COOLING, HEATING, DEADTIME };

struct TecConfig {
    float    setpointC        = 30.0f;
    float    hysteresisC      = 1.0f;     // +/- band
    uint32_t minOnMs          = 2000;
    uint32_t minOffMs         = 2000;
    uint32_t reverseDeadMs    = 200;      // EN=OFF delay before flipping MODE
    float    maxSafeC         = 70.0f;
    float    minSafeC         = 0.0f;
};

// Hardware abstraction for 2-pin control (MODE + EN)
struct ITecDriver {
    virtual ~ITecDriver() {}
    virtual void enOff() = 0;             // EN = 0 (disable Tec output)
    virtual void enOn()  = 0;             // EN = 1 (enable with current MODE)
    virtual void setCoolPolarity() = 0;   // MODE = COOL direction
    virtual void setHeatPolarity() = 0;   // MODE = HEAT direction
    virtual float readProcessTempC() = 0; // measured temp
};

struct TecHalDriver2Pin : public ITecDriver {
    GPIO_TypeDef* modePort; uint16_t modePin;
    GPIO_TypeDef* enPort;   uint16_t enPin;
    bool modeHighIsCool = true;
    bool enActiveHigh   = true;
    float (*readTempC_cb)() = nullptr;

    // ADD THIS CONSTRUCTOR:
    TecHalDriver2Pin(GPIO_TypeDef* modePort, uint16_t modePin,
                     GPIO_TypeDef* enPort,   uint16_t enPin,
                     bool modeHighIsCool, bool enActiveHigh,
                     float (*readTempC_cb)())
    : modePort(modePort), modePin(modePin),
      enPort(enPort), enPin(enPin),
      modeHighIsCool(modeHighIsCool), enActiveHigh(enActiveHigh),
      readTempC_cb(readTempC_cb) {}

    // ... existing methods enOff(), enOn(), setCoolPolarity(), setHeatPolarity(), readProcessTempC() ...
};

class Tec {
public:
    explicit Tec(ITecDriver& drv, const TecConfig& cfg = TecConfig())
        : drv_(drv), cfg_(cfg) {}

    Tec(const Tec&) = delete; Tec& operator=(const Tec&) = delete;

    void setMode(TecMode m)      { mode_ = m; }
    void setSetpoint(float sp)   { cfg_.setpointC = sp; }
    void setHysteresis(float hb) { cfg_.hysteresisC = hb; }

    TecMode  mode()   const { return mode_; }
    TecState state()  const { return st_; }
    bool     isOn()   const { return outputOn_; }
    int8_t   polarity() const { return lastPolarity_; } // +1 cool, -1 heat, 0 off
    float    lastTempC() const { return lastT_; }

    // call at ~10–20 Hz
    void step(uint32_t nowMs);

private:
    ITecDriver& drv_;
    TecConfig   cfg_;
    TecMode     mode_           = TecMode::OFF;
    TecState    st_             = TecState::IDLE;

    bool        outputOn_       = false;  // EN state
    int8_t      lastPolarity_   = 0;      // -1 heat, +1 cool, 0 off
    uint32_t    tLastOn_        = 0;
    uint32_t    tLastOff_       = 0;
    uint32_t    tDeadStart_     = 0;

    float       lastT_          = 25.0f;

    inline bool minOnOK(uint32_t now)  const { return (now - tLastOn_)  >= cfg_.minOnMs; }
    inline bool minOffOK(uint32_t now) const { return (now - tLastOff_) >= cfg_.minOffMs; }
};
