#include "ConfigService.hpp"
static auto& config = ConfigService::global;
#include "HttpService.hpp"
#include "Silvanus.hpp"

#include <unistd.h>
#include <signal.h>
#include <thread>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <sigslot/signal.hpp>

using json = nlohmann::json;

volatile bool interrupt_received = false;
volatile bool internal_exit = false;

static void InterruptHandler(int signo)
{
    interrupt_received = true;
}

std::chrono::system_clock::time_point timeAtMidnight()
{
    auto now = std::chrono::system_clock::now();
    time_t tnow = std::chrono::system_clock::to_time_t(now);
    tm *date = std::localtime(&tnow);
    date->tm_hour = 0;
    date->tm_min = 0;
    date->tm_sec = 0;
    return std::chrono::system_clock::from_time_t(std::mktime(date));;
}

std::chrono::system_clock::duration timeSinceMidnight() 
{
    auto now = std::chrono::system_clock::now();

    time_t tnow = std::chrono::system_clock::to_time_t(now);
    tm *date = std::localtime(&tnow);
    date->tm_hour = 0;
    date->tm_min = 0;
    date->tm_sec = 0;
    auto midnight = std::chrono::system_clock::from_time_t(std::mktime(date));

    return now-midnight;
}

void primeLight(std::chrono::system_clock::time_point& thisEval, 
                std::chrono::system_clock::time_point& lastEval, 
                int lightTime,
                int lightInterval,
                Silvanus& silvanus)
{
    // Start up
    thisEval = std::chrono::system_clock::now();

    // Prime the light (so that we don't lose a day's sunlight if the system reboots)
    // Figure out if the light should be on right now (and then turn it on)
    {
        auto todaysLightOn = timeAtMidnight() + std::chrono::seconds(lightTime);
        auto todaysLightOff = todaysLightOn + std::chrono::seconds(lightInterval);
        auto yesterdaysLightOn = todaysLightOn - std::chrono::hours(24);
        auto yesterdaysLightOff = todaysLightOff - std::chrono::hours(24);
        
        if (thisEval >= yesterdaysLightOn && thisEval < yesterdaysLightOff)
        {
            auto timeRemaining = yesterdaysLightOff - thisEval;
            silvanus.PulseLight(std::chrono::duration_cast<std::chrono::seconds>(timeRemaining));
            #ifndef PI_HOST
            std::cout << "[Simulator] Yesterday's sunlight period was ongoing at startup." << std::endl;
            #endif
        }
        else if (thisEval >= todaysLightOn && thisEval < todaysLightOff)
        {
            auto timeRemaining = todaysLightOff - thisEval;
            silvanus.PulseLight(std::chrono::duration_cast<std::chrono::seconds>(timeRemaining));
            #ifndef PI_HOST
            std::cout << "[Simulator] Today's sunlight period was ongoing at startup." << std::endl;
            #endif
        }
        else
        {
            silvanus.SetLight(false);
            #ifndef PI_HOST
            std::cout << "[Simulator] No sunlight period ongoing, light remains off." << std::endl;
            #endif
        }
    }

    // Setup the lastEval to the time evaluated for the light on/off priming logic
    lastEval = thisEval;
}

int main(int argc, char *argv[])
{
    // Subscribe to signal interrupts
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    // Init the config service here (since doing it in static init is disallowed)
    // and because lots of components rely on its basic vars being set
    config.Init();

    Silvanus silvanus;

    // Plant watering parameters
    int lightTime = 25200; // When the light should turn on (seconds after local midnight)
    int lightInterval = 43200; // How long the light should run (seconds)
    int waterTime = 25200; // When to water the plants (seconds after local midnight)
    float waterFlowRate = 1.3f; // milliliters per second
    float waterAmountPerDay = 100.0f; // milliliters
    std::chrono::system_clock::time_point thisEval, lastEval;

    // Subscribe to settings changes (this also runs the lambda once before subscribing)
    config.Subscribe([&](const ConfigUpdateEventArg& arg)
    {
        arg.UpdateIfChanged("waterTime", waterTime, 25200);
        arg.UpdateIfChanged("waterFlowRate", waterFlowRate, 1.3f);
        arg.UpdateIfChanged("waterAmountPerDay", waterAmountPerDay, 100.0f);
        arg.UpdateIfChanged("lightTime", lightTime, 25200); // default 7 AM
        arg.UpdateIfChanged("lightInterval", lightInterval, 43200); // default 12 hours
    });

    // Add the HTTP service to serve web requests
    HttpService httpService;
    httpService.Server().Post("/system/restart", [=](const httplib::Request& req, httplib::Response& res) 
    {
        internal_exit = true;
    });

    httpService.Server().Patch("/system/settings", [&](const httplib::Request& req, httplib::Response& res) 
    {
        auto settingsPatch = json::parse(req.body);

        for (auto& kvp : settingsPatch.items())
        {
            if (!config.HasKey(kvp.key()))
            {
                res.status = 400;
                res.body = fmt::format("Bad patch request, settings key {} is invalid.", kvp.key());
                return;
            }

            if (!config.ValueTypeMatches(kvp.key(), kvp.value()))
            {
                res.status = 400;
                res.body = fmt::format("Bad patch request, value {} was an incorrect type.", kvp.key());
                return;
            }
        }

        for (auto& kvp : settingsPatch.items())
        {
            config.SetConfigValue(kvp.key(), kvp.value());
        }

        // Save the changed config and determine if the 
        config.SaveConfig();
        primeLight(thisEval, lastEval, lightTime, lightInterval, silvanus);
    });

    httpService.Server().Get("/system/settings", [=](const httplib::Request& req, httplib::Response& res) 
    {
        std::stringstream ss;
        ss << std::setw(4) << config.GetConfigJson();
        res.body = ss.str();
    });

    httpService.Server().Get("/status", [&](const httplib::Request& req, httplib::Response& res) 
    {
        auto status = json::object();
        status["temperature"] = silvanus.GetTemperature();
        status["moisture"] = silvanus.GetMoisture();
        status["light-on"] = silvanus.GetLight();
        status["pump-on"] = silvanus.GetPump();
        std::stringstream ss;
        ss << std::setw(4) << status;
        res.body = ss.str();
    });

    httpService.Server().Post("/water-now", [&](const httplib::Request& req, httplib::Response& res) 
    {
        silvanus.PulsePump(std::chrono::seconds((int)(waterAmountPerDay * waterFlowRate)));
    });

    httpService.Server().Post("/auto-light", [&](const httplib::Request& req, httplib::Response& res) 
    {
        // Evaluate if the light should be on already
        // Resets the light behavior to auto
        primeLight(thisEval, lastEval, lightTime, lightInterval, silvanus);
    });

    httpService.Server().Put("/light", [&](const httplib::Request& req, httplib::Response& res) 
    {
        auto body = json::parse(req.body);
        if (body.is_boolean())
        {
            silvanus.SetLight(body.get<bool>());
        }
        else
        {
            res.status = 401;
            res.body = "Error: Endpoint /light expects json bool value.";
        }
    });

    httpService.Server().Get("/light", [&](const httplib::Request& req, httplib::Response& res) 
    {
        json body = silvanus.GetLight();
        std::stringstream ss;
        ss << std::setw(4) << body;
        res.body = ss.str();
    });

    httpService.Server().Put("/pump", [&](const httplib::Request& req, httplib::Response& res) 
    {
        auto body = json::parse(req.body);
        if (body.is_boolean())
        {
            silvanus.SetPump(body.get<bool>());
        }
        else
        {
            res.status = 401;
            res.body = "Error: Endpoint /pump expects json bool value.";
        }
    });

    httpService.Server().Get("/pump", [&](const httplib::Request& req, httplib::Response& res) 
    {
        json body = silvanus.GetPump();
        std::stringstream ss;
        ss << std::setw(4) << body;
        res.body = ss.str();
    });

    // Save the config after startup
    config.SaveConfig();

    // Evaluate if the light should be on already
    primeLight(thisEval, lastEval, lightTime, lightInterval, silvanus);

    // Start the main logic loop
    while (!interrupt_received && !internal_exit)
    {
        thisEval = std::chrono::system_clock::now();
        auto midnight = timeAtMidnight();
        auto todaysLightOn = midnight + std::chrono::seconds(lightTime);
        auto todaysPumpOn = midnight + std::chrono::seconds(waterTime);

        if (todaysLightOn > lastEval && todaysLightOn <= thisEval)
        {
            #ifndef PI_HOST
            std::cout << "[Simulator] Starting sun period." << std::endl;
            #endif
            silvanus.PulseLight(std::chrono::seconds(lightInterval));
        }
        if (todaysPumpOn > lastEval && todaysPumpOn <= thisEval)
        {
            #ifndef PI_HOST
            std::cout << "[Simulator] Starting rain period." << std::endl;
            #endif
            silvanus.PulsePump(std::chrono::seconds((int)(waterAmountPerDay * waterFlowRate)));
        }

        // Regulate update rate
        std::this_thread::sleep_for(std::chrono::seconds(1));
        lastEval = thisEval;
    }

    config.SaveConfig();

    if (interrupt_received)
    {
        fprintf(stderr, "Main thread caught exit signal.\n");
    }

    if (internal_exit)
    {
        fprintf(stderr, "Main thread got internal exit request.\n");
    }

    return 0;
}
