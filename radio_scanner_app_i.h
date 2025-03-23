#pragma once

#include <gui/gui.h>
#include <gui/view_port.h>
#include <subghz/devices/devices.h>

#define TAG "RadioScannerApp"

#define RADIO_SCANNER_DEFAULT_FREQ        433920000
#define RADIO_SCANNER_DEFAULT_RSSI        (-100.0f)
#define RADIO_SCANNER_DEFAULT_SENSITIVITY (-85.0f)
#define RADIO_SCANNER_BUFFER_SZ           32

#define SUBGHZ_FREQUENCY_MIN  300000000
#define SUBGHZ_FREQUENCY_MAX  928000000
#define SUBGHZ_FREQUENCY_STEP 10000
#define SUBGHZ_DEVICE_NAME    "cc1101_int"

typedef enum {
    ScanDirectionUp,
    ScanDirectionDown,
} ScanDirection;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    bool running;
    uint32_t frequency;
    float rssi;
    float sensitivity;
    bool scanning;
    ScanDirection scan_direction;
    const SubGhzDevice* radio_device;
    bool speaker_acquired;
} RadioScannerApp;

void radio_scanner_draw_callback(Canvas* canvas, void* context);
void radio_scanner_input_callback(InputEvent* input_event, void* context);
void radio_scanner_rx_callback(const void* data, size_t size, void* context);
void radio_scanner_update_rssi(RadioScannerApp* app);
bool radio_scanner_init_subghz(RadioScannerApp* app);
void radio_scanner_process_scanning(RadioScannerApp* app);
