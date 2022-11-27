#pragma once

#include <memory>
#include <sigslot/signal.hpp>

class InputButton
{
public:
    InputButton() = default;
    virtual ~InputButton() = default;
    sigslot::signal<> OnTap;
    sigslot::signal<> OnDoubleTap;
    sigslot::signal<> OnTripleTap;
    sigslot::signal<> OnHold;
};

#ifdef LINUX_HID_CONTROLLER_SUPPORT
class UsbButton : public InputButton
{
public:
    UsbButton();
    ~UsbButton();
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};
#endif
