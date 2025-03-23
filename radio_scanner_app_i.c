#include "radio_scanner_app_i.h"

/**
 * Draw callback for updating the canvas UI.
 * Displays the current frequency, RSSI, sensitivity, and scanning status.
 */
void radio_scanner_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_draw_callback");
#endif
    RadioScannerApp* app = (RadioScannerApp*)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Radio Scanner");

    canvas_set_font(canvas, FontSecondary);
    char freq_str[RADIO_SCANNER_BUFFER_SZ + 1] = {0};
    snprintf(freq_str, RADIO_SCANNER_BUFFER_SZ, "Freq: %.2f MHz", (double)app->frequency / 1000000);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, freq_str);

    char rssi_str[RADIO_SCANNER_BUFFER_SZ + 1] = {0};
    snprintf(rssi_str, RADIO_SCANNER_BUFFER_SZ, "RSSI: %.2f", (double)app->rssi);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, rssi_str);

    char sensitivity_str[RADIO_SCANNER_BUFFER_SZ + 1] = {0};
    snprintf(sensitivity_str, RADIO_SCANNER_BUFFER_SZ, "Sens: %.2f", (double)app->sensitivity);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, sensitivity_str);

    canvas_draw_str_aligned(
        canvas, 64, 54, AlignCenter, AlignTop, app->scanning ? "Scanning..." : "Locked");
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_draw_callback");
#endif
}

/**
 * Input callback for handling button events.
 * Passes input events into the event queue for processing.
 */
void radio_scanner_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_input_callback");
    FURI_LOG_D(TAG, "Input event: type=%d, key=%d", input_event->type, input_event->key);
#endif
    FuriMessageQueue* event_queue = context;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
    FURI_LOG_D(TAG, "Exit radio_scanner_input_callback");
}

/**
 * RX callback triggered on radio packet reception.
 * Currently unused beyond debug logging.
 */
void radio_scanner_rx_callback(const void* data, size_t size, void* context) {
    UNUSED(data);
    UNUSED(context);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "radio_scanner_rx_callback called with size: %zu", size);
#else
    UNUSED(size);
#endif
}

/**
 * Updates the RSSI (signal strength) value from the radio device.
 */
void radio_scanner_update_rssi(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_update_rssi");
#endif
    if(app->radio_device) {
        app->rssi = subghz_devices_get_rssi(app->radio_device);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Updated RSSI: %f", (double)app->rssi);
#endif
    } else {
        FURI_LOG_E(TAG, "Radio device is NULL");
        app->rssi = RADIO_SCANNER_DEFAULT_RSSI;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_update_rssi");
#endif
}

/**
 * Initializes the SubGHz radio device with appropriate settings.
 * Sets frequency, loads preset, and begins asynchronous reception.
 */
bool radio_scanner_init_subghz(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_init_subghz");
#endif
    subghz_devices_init();
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGHz devices initialized");
#endif

    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_NAME);
    if(!device) {
        FURI_LOG_E(TAG, "Failed to get SubGhzDevice");
        return false;
    }
    FURI_LOG_I(TAG, "SubGhzDevice obtained: %s", subghz_devices_get_name(device));

    app->radio_device = device;

    subghz_devices_begin(device);
    subghz_devices_reset(device);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGhzDevice begun");
#endif
    if(!subghz_devices_is_frequency_valid(device, app->frequency)) {
        FURI_LOG_E(TAG, "Invalid frequency: %lu", app->frequency);
        return false;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Frequency is valid: %lu", app->frequency);
#endif
    subghz_devices_load_preset(device, FuriHalSubGhzPreset2FSKDev238Async, NULL);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Preset loaded");
#endif
    subghz_devices_set_frequency(device, app->frequency);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Frequency set to %lu", app->frequency);
#endif
    subghz_devices_start_async_rx(device, radio_scanner_rx_callback, app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Asynchronous RX started");
#endif
    if(furi_hal_speaker_acquire(30)) {
        app->speaker_acquired = true;
        subghz_devices_set_async_mirror_pin(device, &gpio_speaker);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Speaker acquired and async mirror pin set");
#endif
    } else {
        app->speaker_acquired = false;
        FURI_LOG_E(TAG, "Failed to acquire speaker");
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_init_subghz");
#endif
    return true;
}

/**
 * Core logic for scanning radio frequencies.
 * Adjusts frequency up/down and checks for valid signal above sensitivity threshold.
 */
void radio_scanner_process_scanning(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_process_scanning");
#endif
    radio_scanner_update_rssi(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "RSSI after update: %f", (double)app->rssi);
#endif
    bool signal_detected = (app->rssi > app->sensitivity);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Signal detected: %d", signal_detected);
#endif

    if(signal_detected) {
        if(app->scanning) {
            app->scanning = false;
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning stopped");
#endif
        }
    } else {
        if(!app->scanning) {
            app->scanning = true;
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning started");
#endif
        }
    }

    if(!app->scanning) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Exit radio_scanner_process_scanning");
        return;
#endif
    }
    uint32_t new_frequency = (app->scan_direction == ScanDirectionUp) ?
                                 app->frequency + SUBGHZ_FREQUENCY_STEP :
                                 app->frequency - SUBGHZ_FREQUENCY_STEP;

    if(!subghz_devices_is_frequency_valid(app->radio_device, new_frequency)) {
        if(app->scan_direction == ScanDirectionUp) {
            if(new_frequency < 387000000) {
                new_frequency = 387000000;
            } else if(new_frequency < 779000000) {
                new_frequency = 779000000;
            } else if(new_frequency > SUBGHZ_FREQUENCY_MAX) {
                new_frequency = SUBGHZ_FREQUENCY_MIN;
            }
        } else {
            if(new_frequency > 464000000) {
                new_frequency = 464000000;
            } else if(new_frequency > 348000000) {
                new_frequency = 348000000;
            } else if(new_frequency < SUBGHZ_FREQUENCY_MIN) {
                new_frequency = SUBGHZ_FREQUENCY_MAX;
            }
        }
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Adjusted frequency to next valid range: %lu", new_frequency);
#endif
    }

    subghz_devices_flush_rx(app->radio_device);
    subghz_devices_stop_async_rx(app->radio_device);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Asynchronous RX stopped");
#endif

    subghz_devices_idle(app->radio_device);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Device set to idle");
#endif
    app->frequency = new_frequency;
    subghz_devices_set_frequency(app->radio_device, app->frequency);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Frequency set to %lu", app->frequency);
#endif

    subghz_devices_start_async_rx(app->radio_device, radio_scanner_rx_callback, app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Asynchronous RX restarted");
    FURI_LOG_D(TAG, "Exit radio_scanner_process_scanning");
#endif
}
