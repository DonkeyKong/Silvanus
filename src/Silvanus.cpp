#include "Silvanus.hpp"

#include <iostream>
#include <chrono>
#include <thread>

#if PI_HOST
#include <minimal_gpio.h>
#endif

static const int LIGHT_GPIO = 26;
static const int PUMP_GPIO = 20;
static const int EXPANSION_GPIO = 21;

Silvanus::Silvanus()
{
    #if PI_HOST
    if (gpioInitialise() < 0)
    {
        throw std::runtime_error("Failed to setup GPIO\n");
    }
    gpioSetMode(LIGHT_GPIO, PI_OUTPUT); // Setup light as output
    gpioWrite(LIGHT_GPIO, 0);
    gpioSetMode(PUMP_GPIO, PI_OUTPUT); // Setup pump as output
    gpioWrite(PUMP_GPIO, 0);
    //gpioSetMode(EXPANSION_GPIO, PI_OUTPUT); // Setup TBD extra output
    //gpioWrite(EXPANSION_GPIO, 0);
    #else
    lightState_ = false;
    pumpState_ = false;
    #endif

    exit_ = false;
    lightOffTime_ = std::chrono::steady_clock::time_point::max();
    pumpOffTime_ = std::chrono::steady_clock::time_point::max();
    pulseThread_ = std::make_unique<std::thread>(&Silvanus::pulseThreadFunc, this);
}

Silvanus::~Silvanus()
{
    {
        const std::lock_guard<std::mutex> lock(threadMutex_);
        exit_ = true;
    }
    if (pulseThread_ != nullptr)
    {
        pulseThread_->join();
        pulseThread_ = nullptr;
    }
}

void Silvanus::SetLight(bool state)
{
    const std::lock_guard<std::mutex> lock(ioMutex_);
    #if PI_HOST
    gpioWrite(LIGHT_GPIO, state ? 1 : 0);
    #else
    lightState_ = state;
    std::cout << "[Simulator] Set Light: " << (state ? "ON" : "OFF") << std::endl;
    #endif
}

void Silvanus::SetPump(bool state)
{
    const std::lock_guard<std::mutex> lock(ioMutex_);
    #if PI_HOST
    gpioWrite(PUMP_GPIO, state ? 1 : 0);
    #else
    pumpState_ = state;
    std::cout << "[Simulator] Set Pump: " << (state ? "ON" : "OFF") << std::endl;
    #endif
}

bool Silvanus::GetLight()
{
    const std::lock_guard<std::mutex> lock(ioMutex_);
    #if PI_HOST
    return gpioRead(LIGHT_GPIO) == 0 ? false : true;
    #else
    return lightState_;
    #endif
}

bool Silvanus::GetPump()
{
    const std::lock_guard<std::mutex> lock(ioMutex_);
    #if PI_HOST
    return gpioRead(PUMP_GPIO) == 0 ? false : true;
    #else
    return pumpState_;
    #endif
}

float Silvanus::GetHumidity()
{
  const std::lock_guard<std::mutex> lock(ioMutex_);
  return tempHumSensor_.readHumidity();
}

float Silvanus::GetTemperature()
{
  const std::lock_guard<std::mutex> lock(ioMutex_);
  return tempHumSensor_.readTemperature();
}

void Silvanus::PulseLight(std::chrono::seconds duration)
{
    const std::lock_guard<std::mutex> lock(threadMutex_);
    SetLight(true);
    lightOffTime_ = std::chrono::steady_clock::now() + duration;
}

void Silvanus::PulsePump(std::chrono::seconds duration)
{
    const std::lock_guard<std::mutex> lock(threadMutex_);
    SetPump(true);
    pumpOffTime_ = std::chrono::steady_clock::now() + duration;
}

void  Silvanus::pulseThreadFunc()
{
    while (true)
    {
        {
            const std::lock_guard<std::mutex> lock(threadMutex_);
            if (exit_) break;
            auto thisEval = std::chrono::steady_clock::now();
            if (lightOffTime_ <= thisEval)
            {
                #ifndef PI_HOST
                std::cout << "[Simulator] Ending sun period." << std::endl;
                #endif
                SetLight(false);
                lightOffTime_ = std::chrono::steady_clock::time_point::max();
            }
            if (pumpOffTime_ <= thisEval)
            {
                #ifndef PI_HOST
                std::cout << "[Simulator] Ending rain period." << std::endl;
                #endif
                SetPump(false);
                pumpOffTime_ = std::chrono::steady_clock::time_point::max();
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    SetLight(false);
    SetPump(false);
}

