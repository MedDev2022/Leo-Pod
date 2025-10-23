#include "tec.hpp"
#include "stm32h7xx_hal.h" // for HAL_GetTick()

void Tec::step(uint32_t nowMs) {
    lastT_ = drv_.readProcessTempC();
    const float SP = cfg_.setpointC;
    const float HB = cfg_.hysteresisC;

    // Safety or external OFF mode
    const bool unsafe = (lastT_ > cfg_.maxSafeC) || (lastT_ < cfg_.minSafeC) || (mode_ == TecMode::OFF);
    if (unsafe) {
        if (outputOn_) {
            drv_.enOff();
            outputOn_     = false;
            lastPolarity_ = 0;
            tLastOff_     = nowMs;
        }
        st_ = TecState::IDLE;
        return;
    }

    const bool wantCool = (mode_ == TecMode::COOL) && (lastT_ > SP + HB);
    const bool wantHeat = (mode_ == TecMode::HEAT) && (lastT_ < SP - HB);

    switch (st_) {
    case TecState::IDLE: {
        if (!minOffOK(nowMs)) break;

        if (wantCool || wantHeat) {
            const int8_t desiredPol = wantCool ? +1 : -1;

            // If changing direction while currently enabled → enforce deadtime
            if (outputOn_ && (lastPolarity_ != 0) && (lastPolarity_ != desiredPol)) {
                drv_.enOff();
                outputOn_   = false;
                tLastOff_   = nowMs;
                tDeadStart_ = nowMs;
                st_         = TecState::DEADTIME;
                break;
            }

            // Set direction, then enable
            if (wantCool) { drv_.setCoolPolarity(); lastPolarity_ = +1; }
            else          { drv_.setHeatPolarity(); lastPolarity_ = -1; }

            drv_.enOn();
            outputOn_ = true;
            tLastOn_  = nowMs;
            st_       = wantCool ? TecState::COOLING : TecState::HEATING;
        }
    } break;

    case TecState::COOLING: {
        const bool stop = (lastT_ < SP - HB) && minOnOK(nowMs);
        if (stop) {
            drv_.enOff();
            outputOn_ = false;
            tLastOff_ = nowMs;
            st_       = TecState::IDLE;
        }
    } break;

    case TecState::HEATING: {
        const bool stop = (lastT_ > SP + HB) && minOnOK(nowMs);
        if (stop) {
            drv_.enOff();
            outputOn_ = false;
            tLastOff_ = nowMs;
            st_       = TecState::IDLE;
        }
    } break;

    case TecState::DEADTIME: {
        // After EN=OFF for reverseDeadMs, set new MODE if still demanded, then EN=ON
        if ((nowMs - tDeadStart_) >= cfg_.reverseDeadMs && minOffOK(nowMs)) {
            if ((mode_ == TecMode::COOL) && (lastT_ > SP + HB)) {
                drv_.setCoolPolarity(); lastPolarity_ = +1;
                drv_.enOn();  outputOn_ = true; tLastOn_ = nowMs; st_ = TecState::COOLING;
            } else if ((mode_ == TecMode::HEAT) && (lastT_ < SP - HB)) {
                drv_.setHeatPolarity(); lastPolarity_ = -1;
                drv_.enOn();  outputOn_ = true; tLastOn_ = nowMs; st_ = TecState::HEATING;
            } else {
                st_ = TecState::IDLE;
            }
        }
    } break;
    }
}
