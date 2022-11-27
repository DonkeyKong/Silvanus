#include "ConfigService.hpp"

#include <math.h>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <fstream>

using json = nlohmann::json;

#ifdef PI_HOST
static const std::string CONFIG_PATH = "/boot/SilvanusConfig.json";
#else
static const std::string CONFIG_PATH = "SilvanusConfig.json";
#endif
static const std::string DEFAULT_RESOURCES_PATH = "resources";

ConfigService ConfigService::global;

ConfigService::ConfigService()
{
    _initDone = false;
}

ConfigService::~ConfigService()
{
  
}

std::vector<std::string> ConfigService::splitKeyPath(const std::string& path)
{
    std::vector<std::string> pathParts;
    constexpr char delim = '.';
    std::string::size_type start = 0;
    std::string::size_type end = 0;

    while (start < path.size() && end != std::string::npos)
    {
        end = path.find(delim, start);
        if (end == std::string::npos)
            pathParts.push_back(path.substr(start));
        else
            pathParts.push_back(path.substr(start, end-start));
        start = end+1; // skip over delimeter
    }

    return pathParts;
}

static bool hasJsonValueRecursive(const std::vector<std::string>& path, int currentElement, const json& currentObj)
{
    if (path.size() <= currentElement)
    {
        return true;
    }
    else if (currentObj.contains(path[currentElement]))
    {
        return hasJsonValueRecursive(path, currentElement+1, currentObj[path[currentElement]]);
    }

    return false;
}

bool ConfigService::hasJsonValue(const std::string& pathStr)
{
    auto path = splitKeyPath(pathStr);
    if (path.size() == 0)
        return false;
    return hasJsonValueRecursive(path, 0, _config);
}

static nlohmann::json& getJsonValueRecursive(const std::vector<std::string>& path, int currentElement, json& currentObj, bool createIfMissing)
{
    if (path.size() <= currentElement)
    {
        return currentObj;
    }
    else if (currentObj.contains(path[currentElement]))
    {
        return getJsonValueRecursive(path, currentElement+1, currentObj[path[currentElement]], createIfMissing);
    }
    else if (createIfMissing)
    {
        currentObj[path[currentElement]] = json::object();
        return getJsonValueRecursive(path, currentElement+1, currentObj[path[currentElement]], createIfMissing);
    }

    throw std::runtime_error("Key cannot be found!");
}

static const nlohmann::json& getJsonValueRecursive(const std::vector<std::string>& path, int currentElement, const json& currentObj)
{
    if (path.size() <= currentElement)
    {
        return currentObj;
    }
    else if (currentObj.contains(path[currentElement]))
    {
        return getJsonValueRecursive(path, currentElement+1, currentObj[path[currentElement]]);
    }

    throw std::runtime_error("Key cannot be found!");
}

bool ConfigService::HasKey(const std::string& key)
{
    if (!_initDone) throw std::runtime_error("Config service is not initialized!");
    return hasJsonValue(key);
}

bool ConfigService::ValueTypeMatches(const std::string& key, const nlohmann::json& value)
{
    if (!_initDone) throw std::runtime_error("Config service is not initialized!");
    if (!HasKey(key)) return false;
    return getJsonValue(key, false).type() == value.type();
}

nlohmann::json& ConfigService::getJsonValue(const std::string& pathStr, bool createIfMissing)
{
    auto path = splitKeyPath(pathStr);
    return getJsonValueRecursive(path, 0, _config, createIfMissing);
}

const nlohmann::json& ConfigService::getJsonValue(const std::string& pathStr) const
{
    auto path = splitKeyPath(pathStr);
    return getJsonValueRecursive(path, 0, _config);
}

void ConfigService::Init()
{
    if (!_initDone)
    {
        _settingsReadOK = readConfig();

        resourcePath_ = getConfigValueInternal("resourcePath", DEFAULT_RESOURCES_PATH);

        SaveConfig();
        _initDone = true;
    }
}

void ConfigService::Subscribe(const std::function <void (const ConfigUpdateEventArg&)>& handler) 
{
    if (!_initDone) throw std::runtime_error("Config service is not initialized!");
    ConfigUpdateEventArg arg(*this, "", true);
    handler(arg);
    OnSettingChanged.connect(handler);
}

std::string ConfigService::resourcePath() const
{
    if (!_initDone) throw std::runtime_error("Config service is not initialized!");
    return resourcePath_;
}

const json& ConfigService::GetConfigJson(const std::string& key) const
{
    if (!_initDone) throw std::runtime_error("Config service is not initialized!");

    return getJsonValue(key);
}

template <>
void ConfigService::SetConfigValue(const std::string& key, const json& value)
{
    try
    {
        json& entry = getJsonValue(key, true);
        if (entry != value)
        {
            entry = value;
            if (_initDone)
            {
                OnSettingChanged(ConfigUpdateEventArg(*this, key, false));
            }
        }
    }
    catch (...)
    {
    }
}

void ConfigService::SaveConfig()
{
  if (_settingsReadOK) writeConfig();
}

std::string ConfigService::GetSharedResourcePath(const std::string& resourceName) const
{
  if (!_initDone) throw std::runtime_error("Config service is not initialized!");
  auto filePath = std::filesystem::path(resourcePath_) / "Shared" / resourceName;
  if (std::filesystem::exists(filePath))
  {
    return filePath;
  }
  return std::string();
}

bool ConfigService::readConfig()
{
  // Clear the existing config and set it up as a json object
  _config = json::object();
  
  // If the config file doesn't exist, then we are done. 
  // Consider it "read" so we can overwrite the file.
  if (!std::filesystem::exists(CONFIG_PATH))
  {
    std::cout << "Config file missing, using defaults." << std::endl;
    return true;
  }
  
  // Try to read the file
  try
  {
    std::ifstream ifs(CONFIG_PATH);
    _config = json::parse(ifs);
    ifs.close();
    std::cout << "Read and parsed config file!" << std::endl;
  }
  catch (...)
  {
    std::cout << "Failed to read or parse config file!" << std::endl;
    std::cout << "Delete it or fix permissions to generate a new one." << std::endl;
    return false;
  }
  
  // If the config is some nonsense, clear it
  if (!_config.is_object()) 
  {
    std::cout << "Config was parsed but invalid!" << std::endl;
    std::cout << "Delete it to generate a new one." << std::endl;
    _config = json::object();
    return false;
  }	

  return true;
}

void ConfigService::writeConfig()
{
  try
  {
    std::ofstream ofs(CONFIG_PATH, std::ofstream::out | std::ofstream::trunc);
    ofs << std::setw(4) << _config;
    ofs.flush();
    ofs.close();
    
    if ( (ofs.rdstate() & std::ifstream::failbit ) != 0 )
    {
      std::cout << "Failed to write config file!" << std::endl;
    }
    else
    {
      std::cout << "Wrote config file!" << std::endl;
    }
    //std::cout << _config << std::endl;
  }
  catch (...)
  {
    std::cout << "Failed to write config file!" << std::endl;
  }
}


