#pragma once

#include <chrono>
#include <string>
#include <unordered_set>
#include <ctime>
#include <iostream>
#include <sigslot/signal.hpp>
#include <nlohmann/json.hpp>

class ConfigService;

class ConfigUpdateEventArg
{
    friend class ConfigService;
public:
    template <typename T>
    bool UpdateIfChanged(const std::string& key, T& val, const T& defaultValue) const;
private:
    std::string key;
    ConfigService& cs;
    bool all;
    ConfigUpdateEventArg() = delete;
    ConfigUpdateEventArg(const ConfigUpdateEventArg&) = delete;
    ConfigUpdateEventArg(ConfigUpdateEventArg&&) = delete;
    ConfigUpdateEventArg(ConfigService& cs, std::string key, bool all):
        cs(cs), key(key), all(all) { }
};

class ConfigService
{
  public:
    // Singleton
    static ConfigService global;

    // Constructor
    ConfigService();
    ~ConfigService();

    // Load the config from a file and get the sercive ready to use
    // MUST be called before any other methods may be called
    void Init();

    // Const property accessors
    std::string resourcePath() const;
    
    // Resource path utils
    std::string GetSharedResourcePath(const std::string& resourceName) const;

    void SaveConfig();
    bool HasKey(const std::string& key);
    bool ValueTypeMatches(const std::string& key, const nlohmann::json& value);
    const nlohmann::json& GetConfigJson(const std::string& key = "") const;
    void Subscribe(const std::function <void (const ConfigUpdateEventArg&)>& handler);

    // Configuration functions
    template <typename T>
    T GetConfigValue(const std::string& key, const T& defaultValue)
    {
        if (!_initDone) throw std::runtime_error("Config service is not initialized!");
        return getConfigValueInternal(key, defaultValue);
    }
    
    template <typename T>
    void SetConfigValue(const std::string& key, const T& value)
    {
      try
      {
        nlohmann::json& entry = getJsonValue(key, true);
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

  private: 
    // Internal config
    bool readConfig();
    void writeConfig();

    bool hasJsonValue(const std::string& path);
    nlohmann::json& getJsonValue(const std::string& path, bool createIfMissing);
    const nlohmann::json& getJsonValue(const std::string& path) const;
    static std::vector<std::string> splitKeyPath(const std::string& path);
    nlohmann::json _config;

    bool _settingsReadOK;
    bool _initDone;

    // If the event is raised with ConfigService::AllSettings, that means a full file refresh
    sigslot::signal<const ConfigUpdateEventArg&> OnSettingChanged;

    // These cannot be changed after boot and must be editied in
    // the config file
    std::string resourcePath_;

    // Configuration functions
    template <typename T>
    T getConfigValueInternal(const std::string& key, const T& defaultValue)
    {
        T configValue;
        try
        {
            
            if (hasJsonValue(key))
            {
                configValue = getJsonValue(key, false);  // Try to read this value from the config
                //std::cout << "Read " << key << std::endl;
            }
            else
            {
                SetConfigValue(key, defaultValue);
                configValue = defaultValue;
                //std::cout << "Setting " << key << std::endl;
            }
        }
        catch (...)
        {
            SetConfigValue(key, defaultValue);
            configValue = defaultValue;
            //std::cout << "Setting " << key << std::endl;
        }
        
        return configValue;
    }
};

template <typename T>
bool ConfigUpdateEventArg::UpdateIfChanged(const std::string& key, T& val, const T& defaultValue) const
{
    if (key == this->key || all)
    {
        val = cs.GetConfigValue(key, defaultValue);
        return true;
    }
    return false;
}
