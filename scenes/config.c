#include "../radio_scanner_app_i.h"

/**
 * Enumeration of configuration indices used for accessing specific settings.
 */
enum ConfigIndex {
    ConfigIndexSound,
};

// Text labels for the sound state values.
const char* const sound_state_text[SoundStateNum] = {
    "OFF",
    "ON",
};

// Corresponding enum/int values for sound states.
const uint32_t sound_state_value[SoundStateNum] = {
    SoundStateOFF,
    SoundStateON,
};

/**
 * Callback to update the sound state when changed in the config UI.
 */
static void config_scene_set_sound_state(VariableItem* item) {
    RadioScannerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    variable_item_set_current_value_text(item, sound_state_text[index]);

    app->sound_state = sound_state_value[index];

    if(app->speaker_acquired) {
        // Stop RX before changing the mirror pin
        subghz_devices_stop_async_rx(app->radio_device);
        // Update the mirror pin based on the sound state
        if(app->sound_state == SoundStateON) {
            subghz_devices_set_async_mirror_pin(app->radio_device, &gpio_speaker);
        } else {
            subghz_devices_set_async_mirror_pin(app->radio_device, NULL);
        }
        // Restart RX
        subghz_devices_start_async_rx(app->radio_device, radio_scanner_rx_callback, app);
    }
}

/**
 * Utility function to get the index of a sound state value.
 */
uint8_t config_scene_sound_value_index(const uint32_t value, const uint32_t values[], uint8_t values_count, void* context) {
    furi_assert(context);
    UNUSED(values_count);
    UNUSED(context);

    if(value == values[0]) {
        return 0;
    } else {
        return 1;
    }
}

/**
 * Handler called when entering the config scene.
 * Sets the config view callback and switches the view to the config view.
 */
void config_scene_on_enter(void* context) {
    RadioScannerApp* app = context;
    VariableItem* item;
    uint8_t value_index;

    // Sound
    item = variable_item_list_add(
        app->config,
        "Sound:",
        SoundStateNum,
        config_scene_set_sound_state,
        app
    );
    value_index = config_scene_sound_value_index(app->sound_state, sound_state_value, SoundStateNum, app);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, sound_state_text[value_index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, RadioScannerViewConfig);
}

/**
 * Handles events for the config scene.
 * Processes custom events.
 */
bool config_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

/**
 * Handler called when exiting the config scene.
 */
void config_scene_on_exit(void* context) {
    RadioScannerApp* app = context;
    variable_item_list_set_selected_item(app->config, 0);
    variable_item_list_reset(app->config);
}
