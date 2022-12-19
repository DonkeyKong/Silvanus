#pragma once

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
    float GetMoisture();
    double GetTemperature();
private:
    int i2cFile_;
    std::mutex ioMutex_;
    #ifndef PI_HOST
    bool lightState_;
    bool pumpState_;
    #endif
    
    bool writeI2C(uint8_t baseAddr, uint8_t regAddr, const std::vector<uint8_t>& buf = std::vector<uint8_t>());
    bool readI2C(uint8_t baseAddr, uint8_t regAddr, std::vector<uint8_t>& buf, double delay = 0.008);
    void pulseThreadFunc();
    bool exit_;
    std::chrono::steady_clock::time_point lightOffTime_;
    std::chrono::steady_clock::time_point pumpOffTime_;
    std::mutex threadMutex_;
    std::unique_ptr<std::thread> pulseThread_;
};
