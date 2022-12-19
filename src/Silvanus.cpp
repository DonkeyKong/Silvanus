#include "Silvanus.hpp"

#include <iostream>
#include <chrono>
#include <thread>

#if PI_HOST
#include <unistd.h>				//Needed for I2C port
#include <fcntl.h>				//Needed for I2C port
#include <sys/ioctl.h>			//Needed for I2C port
#include <linux/i2c-dev.h>		//Needed for I2C port
#include <minimal_gpio.h>
#endif

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
    #define SILVANUS_BIG_ENDIAN 1
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
    #define SILVANUS_BIG_ENDIAN 0
#else
#error "Error: cannot determine system endianness"
#endif

static const int _STATUS_BASE = 0x00;
static const int _TOUCH_BASE = 0x0F;
static const int _TOUCH_CHANNEL_OFFSET = 0x10;
static const int _STATUS_TEMP = 0x04;

static const int MOISTURE_SENSOR_I2C_DEVICE = 0x36;
static const int LIGHT_GPIO = 26;
static const int PUMP_GPIO = 20;
static const int EXPANSION_GPIO = 21;

namespace 
{
    template <typename T>
    static inline T min(T a, T b) { return a<b?a:b;}
    template <typename T>
    static inline T max(T a, T b) { return a>b?a:b;}

    template <typename T>
    static T unpack(const std::vector<uint8_t>& buf, int offset = 0)
    {
        #if SILVANUS_BIG_ENDIAN == 1
        T val = 0;
        for (int i = 0; i < sizeof(T); ++i)
        {
            val = val | (T)buf[offset+i] << (8*i);
        }
        #else
        T val = 0;
        for (int i = 0; i < sizeof(T); ++i)
        {
            val = val | (T)buf[offset+i] << (8*(sizeof(T)-i-1));
        }
        return val;
        #endif
    }

    template <typename InputType, typename OutputType>
    static OutputType remap(InputType value, InputType inMin, InputType inMax, OutputType outMin, OutputType outMax, bool clamp = true)
    {
        OutputType outValue = (OutputType)(((double)(value - inMin) / (double)(inMax - inMax)) * (double)(outMax-outMin) + outMin);
        if (clamp)
        {
            outValue = min(max(outValue, outMin),outMax);
        }
        return (OutputType) outValue;
    }
}

Silvanus::Silvanus()
{
    #if PI_HOST
    std::string filename = "/dev/i2c-1";
    if ((i2cFile_ = open(filename.c_str(), O_RDWR)) < 0)
    {
        throw std::runtime_error("Failed to open the i2c bus");
    }
    if (ioctl(i2cFile_, I2C_SLAVE, MOISTURE_SENSOR_I2C_DEVICE) < 0)
    {
        throw std::runtime_error("Failed to acquire bus access and/or talk to device.\n");
    }
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
    #if PI_HOST
    close(i2cFile_);
    #endif
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

float Silvanus::GetMoisture()
{
    const std::lock_guard<std::mutex> lock(ioMutex_);
    uint16_t moisture = 0;
    std::vector<uint8_t> buf(2);
    int count = 0;
    while (true)
    {
        readI2C(_TOUCH_BASE, _TOUCH_CHANNEL_OFFSET, buf, 0.005);
        moisture = unpack<uint16_t>(buf);
        if (moisture > 4095)
        {
            ++count;
            if (count > 5)
            {
                return 0; // Best to say the plant is dry if sensor is broken
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::duration<double>(0.001));
            }
        }
        else
        {
            return remap<uint16_t, float>(moisture,400,1000,0.0f,1.0f);
        }
    }
}

double Silvanus::GetTemperature()
{
    const std::lock_guard<std::mutex> lock(ioMutex_);
    std::vector<uint8_t> buf(4);
    readI2C(_STATUS_BASE, _STATUS_TEMP, buf, 0.005);
    buf[0] = buf[0] & 0x3F;
    int32_t temp = unpack<int32_t>(buf);
    return (1.0 / 65536.0) * (double)temp;
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

bool Silvanus::writeI2C(uint8_t baseAddr, uint8_t regAddr, const std::vector<uint8_t>& buf)
{
    #if PI_HOST
    std::vector<uint8_t> bufWithAddr(2+buf.size());
    bufWithAddr[0] = baseAddr;
    bufWithAddr[1] = regAddr;
    for (int i=0; i < buf.size(); i++)
    {
        bufWithAddr[i+2] = buf[i];
    }
    if (write(i2cFile_, bufWithAddr.data(), bufWithAddr.size()) != bufWithAddr.size())		//write() returns the number of bytes actually written, if it doesn't match then an error occurred (e.g. no response from the device)
    {
        return false;
    }
    #endif
    return true;
}

bool Silvanus::readI2C(uint8_t baseAddr, uint8_t regAddr, std::vector<uint8_t>& buf, double delay)
{
    #if PI_HOST
    writeI2C(baseAddr, regAddr);
    if (delay > 0.0)
    {
        std::this_thread::sleep_for(std::chrono::duration<double>(delay));
    }
    if (read(i2cFile_, buf.data(), buf.size()) != buf.size())		//read() returns the number of bytes actually read, if it doesn't match then an error occurred (e.g. no response from the device)
    {
        return false;
    }
    #endif
    return true;
}
