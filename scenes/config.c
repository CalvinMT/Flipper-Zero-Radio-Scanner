#include "../radio_scanner_app_i.h"

/**
 * Enumeration of configuration indices used for accessing specific settings.
 */
enum ConfigIndex {
    null
};

/**
 * Handler called when entering the config scene.
 * Sets the config view callback and switches the view to the config view.
 */
void config_scene_on_enter(void* context) {
    RadioScannerApp* app = context;

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
