#pragma once

#include <vector>
#include <chrono>
#include <thread>

#if PI_HOST
#include <unistd.h>				//Needed for I2C port
#include <fcntl.h>				//Needed for I2C port
#include <sys/ioctl.h>			//Needed for I2C port
#include <linux/i2c-dev.h>		//Needed for I2C port
#endif

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
    #define SILVANUS_BIG_ENDIAN
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
    #define SILVANUS_LITTLE_ENDIAN
#else
#error "Error: cannot determine system endianness"
#endif


class SilvanusIO
{
private:
    static const int _STATUS_BASE = 0x00;
    static const int _TOUCH_BASE = 0x0F;
    static const int _TOUCH_CHANNEL_OFFSET = 0x10;
    static const int _STATUS_TEMP = 0x04;

    int i2cFile;

    bool writeI2C(uint8_t baseAddr, uint8_t regAddr, const std::vector<uint8_t>& buf = std::vector<uint8_t>())
    {
        #if PI_HOST
        std::vector<uint8_t> bufWithAddr(2+buf.size());
        bufWithAddr[0] = baseAddr;
        bufWithAddr[1] = regAddr;
        for (int i=0; i < buf.size() i++)
        {
            bufWithAddr[i+2] = buf[i];
        }
        if (write(i2cFile, bufWithAddr.data(), bufWithAddr.size()) != bufWithAddr.size())		//write() returns the number of bytes actually written, if it doesn't match then an error occurred (e.g. no response from the device)
        {
            return false;
        }
        #endif
        return true;
    }

    bool readI2C(uint8_t baseAddr, uint8_t regAddr, std::vector<uint8_t>& buf, double delay = 0.008)
    {
        #if PI_HOST
        writeI2C(baseAddr, regAddr);
        if (delay > 0.0)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(delay));
        }
        if (read(i2cFile, buf.data(), buf.size()) != buf.size())		//read() returns the number of bytes actually read, if it doesn't match then an error occurred (e.g. no response from the device)
        {
            return false;
        }
        #endif
        return true;
    }

    template <typename T>
    T unpack(const std::vector<uint8_t>& buf, int offset = 0)
    {
        #ifdef SILVANUS_BIG_ENDIAN
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

public:
    SilvanusIO(int i2cDeviceAddr = 0x36)
    {
        #if PI_HOST
        std::string filename = "/dev/i2c-1";
        if ((i2cFile = open(filename.c_str(), O_RDWR)) < 0)
        {
            printf("Failed to open the i2c bus");
        }
        if (ioctl(i2cFile, I2C_SLAVE, i2cDeviceAddr) < 0)
        {
            printf("Failed to acquire bus access and/or talk to device.\n");
        }
        #endif
    }

    virtual ~SilvanusIO()
    {
        #if PI_HOST
        close(i2cFile);
        #endif
    }

    void SetLight(bool state)
    {
        #if PI_HOST
        #endif
    }

    void SetPump(bool state)
    {
        #if PI_HOST
        #endif
    }

    uint16_t GetMoisture()
    {
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
                return moisture;
            }
        }
    }

    double GetTemperature()
    {
        std::vector<uint8_t> buf(4);
        readI2C(_STATUS_BASE, _STATUS_TEMP, buf, 0.005);
        buf[0] = buf[0] & 0x3F;
        int32_t temp = unpack<int32_t>(buf);
        return (1.0 / 65536.0) * (double)temp;
    }
};