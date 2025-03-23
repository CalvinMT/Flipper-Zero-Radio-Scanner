#include "radio_scanner_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <gui/elements.h>
#include <furi_hal_speaker.h>
#include <subghz/devices/devices.h>

/**
 * Allocates and initializes a new instance of the RadioScannerApp.
 * Sets up GUI components, state variables, and input handlers.
 */
RadioScannerApp* radio_scanner_app_alloc() {
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_app_alloc");
#endif
    RadioScannerApp* app = malloc(sizeof(RadioScannerApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate RadioScannerApp");
        return NULL;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "RadioScannerApp allocated");
#endif

    app->view_port = view_port_alloc();
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort allocated");
#endif

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Event queue allocated");
#endif

    app->running = true;
    app->frequency = RADIO_SCANNER_DEFAULT_FREQ;
    app->rssi = RADIO_SCANNER_DEFAULT_RSSI;
    app->sensitivity = RADIO_SCANNER_DEFAULT_SENSITIVITY;
    app->scanning = true;
    app->scan_direction = ScanDirectionUp;
    app->speaker_acquired = false;
    app->radio_device = NULL;

    view_port_draw_callback_set(app->view_port, radio_scanner_draw_callback, app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Draw callback set");
#endif

    view_port_input_callback_set(app->view_port, radio_scanner_input_callback, app->event_queue);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Input callback set");
    FURI_LOG_D(TAG, "Exit radio_scanner_app_alloc");
#endif

    app->gui = furi_record_open(RECORD_GUI);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "GUI record opened");
#endif

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort added to GUI");
#endif

    return app;
}

/**
 * Frees all resources allocated by the RadioScannerApp instance.
 * Cleans up GUI, radio device, event queue, and memory.
 */
void radio_scanner_app_free(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_app_free");
#endif
    if(app->speaker_acquired && furi_hal_speaker_is_mine()) {
        subghz_devices_set_async_mirror_pin(app->radio_device, NULL);
        furi_hal_speaker_release();
        app->speaker_acquired = false;
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Speaker released");
#endif
    }

    if(app->radio_device) {
        subghz_devices_flush_rx(app->radio_device);
        subghz_devices_stop_async_rx(app->radio_device);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Asynchronous RX stopped");
#endif
        subghz_devices_idle(app->radio_device);
        subghz_devices_sleep(app->radio_device);
        subghz_devices_end(app->radio_device);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "SubGhzDevice stopped and ended");
#endif
    }

    subghz_devices_deinit();
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGHz devices de-initialized");
#endif
    gui_remove_view_port(app->gui, app->view_port);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort removed from GUI");
#endif
    view_port_free(app->view_port);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort freed");
#endif

    furi_message_queue_free(app->event_queue);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Event queue freed");
#endif

    furi_record_close(RECORD_GUI);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "GUI record closed");
#endif

    free(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "RadioScannerApp memory freed");
#endif
}

/**
 * Main entry point for the radio scanner app.
 * Handles main loop, input processing, scanning logic, and cleanup.
 */
int32_t radio_scanner_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Enter radio_scanner_app");

    RadioScannerApp* app = radio_scanner_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return 1;
    }

    if(!radio_scanner_init_subghz(app)) {
        FURI_LOG_E(TAG, "Failed to initialize SubGHz");
        radio_scanner_app_free(app);
        return 255;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGHz initialized successfully");
#endif

    InputEvent event;
    while(app->running) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Main loop iteration");
#endif
        if(app->scanning) {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning is active");
#endif
            radio_scanner_process_scanning(app);
        } else {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning is inactive, updating RSSI");
#endif
            radio_scanner_update_rssi(app);
        }

#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Checking for input events");
#endif
        if(furi_message_queue_get(app->event_queue, &event, 10) == FuriStatusOk) {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Input event received: type=%d, key=%d", event.type, event.key);
#endif
            if(event.type == InputTypeShort) {
                if(event.key == InputKeyOk) {
                    app->scanning = !app->scanning;
                    FURI_LOG_I(TAG, "Toggled scanning: %d", app->scanning);
                } else if(event.key == InputKeyUp) {
                    app->sensitivity += 1.0f;
                    FURI_LOG_I(TAG, "Increased sensitivity: %f", (double)app->sensitivity);
                } else if(event.key == InputKeyDown) {
                    app->sensitivity -= 1.0f;
                    FURI_LOG_I(TAG, "Decreased sensitivity: %f", (double)app->sensitivity);
                } else if(event.key == InputKeyLeft) {
                    app->scan_direction = ScanDirectionDown;
                    FURI_LOG_I(TAG, "Scan direction set to down");
                } else if(event.key == InputKeyRight) {
                    app->scan_direction = ScanDirectionUp;
                    FURI_LOG_I(TAG, "Scan direction set to up");
                } else if(event.key == InputKeyBack) {
                    app->running = false;
                    FURI_LOG_I(TAG, "Exiting app");
                }
            }
        }

        view_port_update(app->view_port);
        furi_delay_ms(10);
    }

    radio_scanner_app_free(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_app");
#endif
    return 0;
}
