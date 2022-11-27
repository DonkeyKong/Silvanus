#include "InputButton.hpp"

#ifdef LINUX_HID_CONTROLLER_SUPPORT

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <atomic>
#include <thread>
#include <memory>

/**
 * Reads a joystick event from the joystick device.
 *
 * Returns 0 on success. Otherwise -1 is returned.
 */
int read_event(int fd, struct js_event *event)
{
    ssize_t bytes;
    fd_set set;
    int rv;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;

    FD_ZERO(&set);    /* clear the set */
    FD_SET(fd, &set); /* add our file descriptor to the set */
    rv = select(fd + 1, &set, NULL, NULL, &timeout);

    if (rv == -1)
        return -1;
    else if (rv == 0)
        return -1;
    else
        bytes = read(fd, event, sizeof(*event));

    FD_ZERO(&set); /* clear the set */

    if (bytes == sizeof(*event))
        return 0;

    /* Error, could not read full event. */
    return -1;
}

/**
 * Returns the number of axes on the controller or 0 if an error occurs.
 */
size_t get_axis_count(int fd)
{
    __u8 axes;

    if (ioctl(fd, JSIOCGAXES, &axes) == -1)
        return 0;

    return axes;
}

/**
 * Returns the number of buttons on the controller or 0 if an error occurs.
 */
size_t get_button_count(int fd)
{
    __u8 buttons;
    if (ioctl(fd, JSIOCGBUTTONS, &buttons) == -1)
        return 0;

    return buttons;
}

/**
 * Current state of an axis.
 */
struct axis_state
{
    short x, y;
};

/**
 * Keeps track of the current axis state.
 *
 * NOTE: This function assumes that axes are numbered starting from 0, and that
 * the X axis is an even number, and the Y axis is an odd number. However, this
 * is usually a safe assumption.
 *
 * Returns the axis that the event indicated.
 */
size_t get_axis_state(struct js_event *event, struct axis_state axes[3])
{
    size_t axis = event->number / 2;

    if (axis < 3)
    {
        if (event->number % 2 == 0)
            axes[axis].x = event->value;
        else
            axes[axis].y = event->value;
    }

    return axis;
}

struct UsbButton::Impl
{
    int buttonFd = -1;
    struct js_event event;
    std::atomic<bool> stopThread;
    std::unique_ptr<std::thread> thread;
};

UsbButton::UsbButton()
{
    pImpl_ = std::make_unique<Impl>();
    // Connect to button device
    pImpl_->buttonFd = open("/dev/input/js1", O_RDONLY);
    if (pImpl_->buttonFd == -1)
    {
        printf("Could not open joystick\n");
    }
    else
    {
        // flush
        while (pImpl_->buttonFd != -1 && read_event(pImpl_->buttonFd, &pImpl_->event) == 0);

        pImpl_->thread = std::make_unique<std::thread>([&]()
        {
            while (!pImpl_->stopThread && pImpl_->buttonFd != -1)
            {
                if (read_event(pImpl_->buttonFd, &pImpl_->event) == 0)
                {
                    if (pImpl_->event.type == JS_EVENT_BUTTON && !pImpl_->event.value)
                    {
                        OnTap();
                    }
                }
            }
            // Close the joystick connection
            if (pImpl_->buttonFd != -1)
                close(pImpl_->buttonFd);
        });
    }
}

UsbButton::~UsbButton()
{
    pImpl_->stopThread = true;
    if (pImpl_->thread)
    {
        pImpl_->thread->join();
        pImpl_->thread = nullptr;
    }
}

#endif
