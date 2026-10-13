#ifndef __COMPONENTS_H__
#define __COMPONENTS_H__

#include <ctype.h>
#include <gtk/gtk.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>

#include "../../timer.h"
#include "../utils.h"

typedef struct LSComponentOps LSComponentOps; // forward declaration

typedef enum LSComponentSettingType {
    LS_COMPONENT_SETTING_BOOLEAN,
    LS_COMPONENT_SETTING_INTEGER,
    LS_COMPONENT_SETTING_NUMBER,
    LS_COMPONENT_SETTING_STRING,
    LS_COMPONENT_SETTING_ARRAY,
    LS_COMPONENT_SETTING_INVALID,
} LSComponentSettingType;

typedef struct LSComponentSettingValue LSComponentSettingValue;
typedef struct LSComponentSetting LSComponentSetting;

struct LSComponentSettingValue {
    LSComponentSettingType type;
    union {
        bool bool_val;
        long int_val;
        double num_val;
        const char* string_val;
        struct {
            const LSComponentSetting* const* values;
            size_t count;
        } array_val;
    };
};

typedef struct LSComponentSetting {
    const char* key;
    const char* label; // for defaults only
    LSComponentSettingValue val;
} LSComponentSetting;

typedef struct LSComponent {
    LSComponentOps* ops;
    LSComponentSetting* settings;
} LSComponent;

typedef struct LSComponentOps {
    const LSComponentSetting* settings;

    void (*delete)(LSComponent* self);
    GtkWidget* (*widget)(LSComponent* self);

    void (*resize)(LSComponent* self, int win_width, int win_height);
    void (*show_game)(LSComponent* self, const ls_game* game, const ls_timer* timer);
    void (*clear_game)(LSComponent* self);
    void (*draw)(LSComponent* self, const ls_game* game, const ls_timer* timer);

    void (*start_split)(LSComponent* self, const ls_timer* timer);
    void (*skip)(LSComponent* self, const ls_timer* timer);
    void (*unsplit)(LSComponent* self, const ls_timer* timer);
    void (*stop_reset)(LSComponent* self, ls_timer* timer);
    void (*pause)(LSComponent* self, ls_timer* timer);
    void (*unpause)(LSComponent* self, ls_timer* timer);
    void (*cancel_run)(LSComponent* self, ls_timer* timer);
} LSComponentOps;

typedef struct LSComponentAvailable {
    char* name;
    LSComponent* (*new)(void);
} LSComponentAvailable;

// A NULL-terminated array of all available components
extern LSComponentAvailable ls_components[];

bool ls_component_is_valid(const char* name);
LSComponentAvailable* ls_component_get(const char* name);
void ls_component_settings_load(LSComponent* component, const json_t* settings);
const LSComponentSettingValue* ls_component_settings_get(const LSComponent* component, const char* key);
const LSComponentSettingValue* ls_component_settings_array_get(const LSComponent* component, const char* array_key, size_t index, const char* key);
void ls_component_destroy(gpointer data);

#endif /* __COMPONENTS_H__ */
