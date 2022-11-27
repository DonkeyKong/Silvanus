#include "ConfigService.hpp"
static auto& config = ConfigService::global;
#include "HttpService.hpp"
#include "InputButton.hpp"

#include <unistd.h>
#include <signal.h>
#include <thread>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <iostream>

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

static void addButton(InputButton& b)
{
    b.OnTap.connect([&]()
    {

    });

    b.OnHold.connect([&]()
    {

    });
}

void setupSystemHttpEndpoints(httplib::Server& srv)
{
    srv.Post("/system/restart", [=](const httplib::Request& req, httplib::Response& res) 
    {
        internal_exit = true;
    });

    srv.Patch("/system/settings", [=](const httplib::Request& req, httplib::Response& res) 
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
    });

    srv.Get("/system/settings", [=](const httplib::Request& req, httplib::Response& res) 
    {
        std::stringstream ss;
        ss << std::setw(4) << config.GetConfigJson();
        res.body = ss.str();
    });
}

int main(int argc, char *argv[])
{
    // Subscribe to signal interrupts
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    // Init the config service here (since doing it in static init is disallowed)
    // and because lots of components rely on its basic vars being set
    config.Init();
    
    // Subscribe to settings changes (this also runs the lambda once before subscribing)
    config.Subscribe([&](const ConfigUpdateEventArg& arg)
    {
        // arg.UpdateIfChanged("defaultScene", defaultScene, DEFAULT_SCENE_NAME);
        
        // if (arg.UpdateIfChanged("fpsLimit", fpsLimit, DEFAULT_FPS))
        // {
        //     expectedFrameTime = std::chrono::high_resolution_clock::duration(
        //     std::chrono::nanoseconds((int)(1.0/(double)fpsLimit * 1000000000.0))    );
        // }
    });

    // Add the HTTP service to serve web requests
    HttpService httpService;
    setupSystemHttpEndpoints(httpService.Server());

    // Save the config after startup
    config.SaveConfig();

// Create the button we listen to for commands
#ifdef LINUX_HID_CONTROLLER_SUPPORT
    UsbButton usbButton;
    addButton(usbButton);
#endif

    auto thisEvalComplete = std::chrono::high_resolution_clock::now();
    auto lastEvalComplete = std::chrono::high_resolution_clock::now();
    auto evalInterval = std::chrono::high_resolution_clock::duration(std::chrono::seconds(1));

    // Start the main logic loop
    while (!interrupt_received && !internal_exit)
    {
        // Evaluate logic

        // Regulate update rate
        thisEvalComplete = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_until(lastEvalComplete + evalInterval);
        lastEvalComplete = thisEvalComplete;
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
