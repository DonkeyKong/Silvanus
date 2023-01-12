#pragma once

#include "Adafruit_SHT31.hpp"

#include <vector>
#include <mutex>
#include <thread>
#include <memory>

class Silvanus
{
public:
    Silvanus();
    ~Silvanus();
    void SetLight(bool state);
    void SetPump(bool state);
    bool GetLight();
    bool GetPump();
    void PulseLight(std::chrono::seconds duration);
    void PulsePump(std::chrono::seconds duration);
    float GetHumidity();
    float GetTemperature();
private:
    std::mutex ioMutex_;
    #ifndef PI_HOST
    bool lightState_;
    bool pumpState_;
    #endif

    void pulseThreadFunc();
    bool exit_;
    std::chrono::steady_clock::time_point lightOffTime_;
    std::chrono::steady_clock::time_point pumpOffTime_;
    std::mutex threadMutex_;
    std::unique_ptr<std::thread> pulseThread_;
    Adafruit_SHT31 tempHumSensor_;
};
