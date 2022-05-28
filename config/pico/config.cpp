#ifndef _CONFIG_HPP
#define _CONFIG_HPP

#include "comms/B0XXInputViewer.hpp"
#include "comms/DInputBackend.hpp"
#include "comms/GamecubeBackend.hpp"
// #include "comms/N64Backend.hpp"
#include "config/mode_selection.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/InputMode.hpp"
#include "core/KeyboardMode.hpp"
#include "core/pinout.hpp"
#include "core/socd.hpp"
#include "core/state.hpp"
#include "input/GpioButtonInput.hpp"
#include "input/NunchukInput.hpp"
#include "modes/Melee20Button.hpp"
#include "stdlib.hpp"

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <pico/bootrom.h>

CommunicationBackend **backends;
size_t backend_count;
KeyboardMode *current_kb_mode = nullptr;

GpioButtonMapping button_mappings[] = {
    {&InputState::l,            5 },
    { &InputState::left,        4 },
    { &InputState::down,        3 },
    { &InputState::right,       2 },

    { &InputState::mod_x,       6 },
    { &InputState::mod_y,       7 },

    { &InputState::start,       0 },

    { &InputState::c_left,      13},
    { &InputState::c_up,        12},
    { &InputState::c_down,      15},
    { &InputState::a,           14},
    { &InputState::c_right,     16},

    { &InputState::b,           26},
    { &InputState::x,           21},
    { &InputState::z,           19},
    { &InputState::up,          17},

    { &InputState::r,           27},
    { &InputState::y,           22},
    { &InputState::lightshield, 20},
    { &InputState::midshield,   18},
};
size_t button_count = sizeof(button_mappings) / sizeof(GpioButtonMapping);

const Pinout pinout = {
    .joybus_data = 28,
    .mux = -1,
    .nunchuk_detect = -1,
    .nunchuk_sda = 8,
    .nunchuk_scl = 9,
};

void setup() {
    // Create GPIO input source and use it to read button states for checking button holds.
    GpioButtonInput *gpio_input = new GpioButtonInput(button_mappings, button_count);

    InputState button_holds;
    gpio_input->UpdateInputs(button_holds);

    // Bootsel button hold as early as possible for safety.
    if (button_holds.start) {
        reset_usb_boot(0, 0);
    }

    // Turn on LED to indicate firmware booted.
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);

    // Create Nunchuk input source.
    NunchukInput *nunchuk =
        new NunchukInput(Wire, pinout.nunchuk_detect, pinout.nunchuk_sda, pinout.nunchuk_scl);

    // Create array of input sources to be used.
    static InputSource *input_sources[] = { gpio_input, nunchuk };
    size_t input_source_count = sizeof(input_sources) / sizeof(InputSource *);

    // USB autodetection.
    bool usb_connected = false;
    for (int i = 0; i < 100; i++) {
        if (USBDevice.mounted()) {
            usb_connected = true;
            break;
        }
    }

    /* Select communication backend. */
    CommunicationBackend *primary_backend;
    if (usb_connected) {
        // Default to DInput mode if USB is connected.
        // Input viewer only used when connected to PC i.e. when using DInput mode.
        primary_backend = new DInputBackend(input_sources, input_source_count);
        backends = new CommunicationBackend *[2] {
            primary_backend, new B0XXInputViewer(input_sources, input_source_count)
        };
        backend_count = 2;
    } else {
        if (button_holds.c_left) {
            // Hold C-Left on plugin for N64.
            // primary_backend =
            //     new N64Backend(input_sources, input_source_count, 60, pinout.joybus_data);
        } else {
            // Default to GameCube.
            primary_backend =
                new GamecubeBackend(input_sources, input_source_count, pinout.joybus_data);
        }

        // If not DInput then only using 1 backend (no input viewer).
        backends = new CommunicationBackend *[1] { primary_backend };
        backend_count = 1;
    }

    // Default to Melee mode.
    primary_backend->SetGameMode(new Melee20Button(socd::SOCD_2IP_NO_REAC));
}

void loop() {
    select_mode(backends[0]);

    for (size_t i = 0; i < backend_count; i++) {
        backends[i]->SendReport();
    }

    if (current_kb_mode != nullptr) {
        current_kb_mode->SendReport(backends[0]->GetInputs());
    }
}

#endif
