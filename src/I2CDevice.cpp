#include "I2CDevice.hpp"

#include <thread>
#include <stdexcept>

#ifdef PI_HOST
#include <unistd.h>        //Needed for I2C port
#include <fcntl.h>         //Needed for I2C port
#include <sys/ioctl.h>     //Needed for I2C port
#include <linux/i2c-dev.h> //Needed for I2C port
#endif

I2CDevice::I2CDevice(uint8_t deviceId, std::string i2cDeviceName)
{
  #ifdef PI_HOST
  if ((i2cFile_ = open(i2cDeviceName.c_str(), O_RDWR)) < 0)
  {
    i2cFile_ = -1;
    throw std::runtime_error("Failed to open the i2c bus");
  }
  if (ioctl(i2cFile_, I2C_SLAVE, deviceId) < 0)
  {
    close(i2cFile_);
    i2cFile_ = -1;
    throw std::runtime_error("Failed to acquire bus access and/or talk to device.\n");
  }
  #endif
}

I2CDevice::~I2CDevice()
{
  #ifdef PI_HOST
  if (i2cFile_ != -1)
  {
    close(i2cFile_);
  }
  #endif
}

bool I2CDevice::writeI2C(uint16_t addr, const std::vector<uint8_t> &buf)
{
  return writeI2C(addr, buf.data(), buf.size());
}

bool I2CDevice::readI2C(uint16_t addr, std::vector<uint8_t> &buf, double delayMs)
{
  return readI2C(addr, buf.data(), buf.size(), delayMs);
}

bool I2CDevice::writeI2C(uint16_t addr, const uint8_t* buf, size_t len)
{
  std::vector<uint8_t> bufWithAddr(2 + len);
  bufWithAddr[0] = addr >> 8;
  bufWithAddr[1] = addr & 0x00FF;
  for (int i = 0; i < len; i++)
  {
    bufWithAddr[i + 2] = buf[i];
  }
  #ifdef PI_HOST
  if (write(i2cFile_, bufWithAddr.data(), bufWithAddr.size()) != bufWithAddr.size()) // write() returns the number of bytes actually written, if it doesn't match then an error occurred (e.g. no response from the device)
  {
    return false;
  }
  #endif
  return true;
}

bool I2CDevice::readI2C(uint16_t addr, uint8_t* buf, size_t len, double delayMs)
{
  writeI2C(addr);
  delay(delayMs);
  #ifdef PI_HOST
  if (read(i2cFile_, buf, len) != len) // read() returns the number of bytes actually read, if it doesn't match then an error occurred (e.g. no response from the device)
  {
    return false;
  }
  #endif
  return true;
}

void I2CDevice::delay(double milliseconds)
{
  if (milliseconds > 0.0)
  {
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(milliseconds));
  }
}
