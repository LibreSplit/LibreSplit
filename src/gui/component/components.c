/** \file components.c
 *
 * Available Components and related utilities
 */
#include "components.h"
#include "src/logging.h"

LSComponent* ls_component_title_new(json_t*);
LSComponent* ls_component_splits_new(json_t*);
LSComponent* ls_component_timer_new(json_t*);
LSComponent* ls_component_detailed_timer_new(json_t*);
LSComponent* ls_component_prev_segment_new(json_t*);
LSComponent* ls_component_run_predict_new(json_t*);
LSComponent* ls_component_best_sum_new(json_t*);
LSComponent* ls_component_pb_new(json_t*);
LSComponent* ls_component_wr_new(json_t*);

LSComponentAvailable ls_components[] = {
    { "title", ls_component_title_new },
    { "splits", ls_component_splits_new },
    { "timer", ls_component_timer_new },
    { "detailed-timer", ls_component_detailed_timer_new },
    { "prev-segment", ls_component_prev_segment_new },
    { "best-sum", ls_component_best_sum_new },
    { "run-predict", ls_component_run_predict_new },
    { "pb", ls_component_pb_new },
    { "wr", ls_component_wr_new },
    { NULL, NULL }
};

/**
 * @brief Validates if the component name is a valid component that has been registered in `ls_components`
 *
 * @param name The name of the component.
 * @return bool Whether or not the component is valid.
 */
bool ls_component_is_valid(const char* name)
{
    // No string or empty string is not valid
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    // A loop over `ls_components` is fine, we could have 1000 components and O(1) would still be the same as O(n) realistically.
    for (size_t i = 0; i < G_N_ELEMENTS(ls_components) - 1; ++i) {
        // here we do ls_components count - 1 since we can ignore the null entry terminator as we have the actual count.
        if (strcmp(name, ls_components[i].name) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Gets the component entry from `ls_components` by the provided name
 * or NULL if the name does not exist.
 *
 * @param name The name of the component.
 * @return LSComponentAvailable* Pointer to the component if found or NULL.
 */
LSComponentAvailable* ls_component_get(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(ls_components) - 1; ++i) {
        if (strcmp(name, ls_components[i].name) == 0) {
            return &ls_components[i];
        }
    }

    return NULL;
}

int ls_component_time_accuracy_decimals(LSTimeAccuracy accuracy)
{
    switch (accuracy) {
        case LS_TIME_ACCURACY_DEFAULT:
            break;
        case LS_TIME_ACCURACY_SECONDS:
            return 0;
        case LS_TIME_ACCURACY_TENTHS:
            return 1;
        case LS_TIME_ACCURACY_HUNDREDTHS:
            return 2;
        case LS_TIME_ACCURACY_MILLISECONDS:
            return 3;
    }

    return CLAMP(cfg.libresplit.decimals.value.i, 0, 6);
}

static void setting_value_clear(const LSComponentSettingValue* value)
{
    if (value->type == LS_COMPONENT_SETTING_STRING) {
        g_free((void*)value->string_val);
    } else if (value->type == LS_COMPONENT_SETTING_ARRAY) {
        for (size_t i = 0; i < value->array_val.count; ++i) {
            for (const LSComponentSetting* setting = value->array_val.values[i]; setting && setting->key; ++setting) {
                if (setting->val.type == LS_COMPONENT_SETTING_STRING) {
                    g_free((void*)setting->val.string_val);
                }
            }

            g_free((void*)value->array_val.values[i]);
        }

        g_free((void*)value->array_val.values);
    }
}

static void settings_clear(const LSComponentSetting* settings)
{
    for (const LSComponentSetting* setting = settings; setting && setting->key; ++setting) {
        setting_value_clear(&setting->val);
    }

    g_free((void*)settings);
}

static bool read_setting(const json_t* json, LSComponentSettingType type, LSComponentSettingValue* value, const LSComponentSetting* defaults)
{
    if (type == LS_COMPONENT_SETTING_ARRAY && json_is_array(json)) {
        size_t count = json_array_size(json);
        size_t settings_count = 0;
        while (defaults->val.array_val.values[0] && defaults->val.array_val.values[0][settings_count++].key);

        const LSComponentSetting** items = g_new0(const LSComponentSetting*, count);
        *value = (LSComponentSettingValue) { .type = LS_COMPONENT_SETTING_ARRAY, .array_val = { .values = items, .count = count } };
        for (size_t i = 0; i < count; ++i) {
            const json_t* entry = json_array_get(json, i);
            if (!json_is_object(entry)) {
                char* value_text = json_dumps(entry, JSON_COMPACT | JSON_ENCODE_ANY);
                LOG_WARNF("Setting %s[%zu]: %s invalid", defaults->key, i, value_text ? value_text : "<unavailable>");
                free(value_text);
                continue;
            }

            size_t loaded_count = 0;
            LSComponentSetting* settings = settings_count ? g_new0(LSComponentSetting, settings_count + 1) : NULL;
            for (size_t j = 0; j < settings_count; ++j) {
                const json_t* setting = json_object_get(entry, defaults->val.array_val.values[0][j].key);
                if (!setting) {
                    continue;
                }

                if (json_is_array(setting)) {
                    char* value_text = json_dumps(setting, JSON_COMPACT | JSON_ENCODE_ANY);
                    LOG_WARNF("Setting %s[%zu].%s: %s invalid", defaults->key, i, defaults->val.array_val.values[0][j].key, value_text ? value_text : "<unavailable>");
                    free(value_text);
                    continue;
                }

                LSComponentSettingValue value = { 0 };
                if (read_setting(setting, defaults->val.array_val.values[0][j].val.type, &value, &defaults->val.array_val.values[0][j])) {
                    settings[loaded_count++] = (LSComponentSetting) { .key = defaults->val.array_val.values[0][j].key, .val = value };
                } else {
                    char* value_text = json_dumps(setting, JSON_COMPACT | JSON_ENCODE_ANY);
                    LOG_WARNF("Setting %s[%zu].%s: %s invalid", defaults->key, i, defaults->val.array_val.values[0][j].key, value_text ? value_text : "<unavailable>");
                    free(value_text);
                }
            }

            items[i] = settings;
        }
    } else if (json_is_boolean(json)) {
        *value = (LSComponentSettingValue) { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = json_is_true(json) };
    } else if (json_is_integer(json)) {
        *value = (LSComponentSettingValue) { .type = LS_COMPONENT_SETTING_INTEGER, .int_val = json_integer_value(json) };
        if (type == LS_COMPONENT_SETTING_OPTIONS) {
            if (value->int_val < 0) {
                return false;
            }

            bool valid = false;
            for (const LSComponentOption* option = defaults->options; option && option->label; ++option) {
                if ((size_t)value->int_val == option->key) {
                    value->type = LS_COMPONENT_SETTING_OPTIONS;
                    valid = true;
                    break;
                }
            }

            if (!valid) {
                return false;
            }
        }
    } else if (json_is_real(json)) {
        *value = (LSComponentSettingValue) { .type = LS_COMPONENT_SETTING_NUMBER, .num_val = json_real_value(json) };
    } else if (json_is_string(json)) {
        *value = (LSComponentSettingValue) { .type = LS_COMPONENT_SETTING_STRING, .string_val = g_strdup(json_string_value(json)) };
    } else {
        return false;
    }

    if (value->type != type) {
        setting_value_clear(value);
        return false;
    }

    return true;
}

void ls_component_settings_load(LSComponent* component, const json_t* settings)
{
    size_t count = 0;
    const LSComponentSetting* defaults = component->ops->settings;
    while (defaults && defaults[count++].key);

    size_t loaded_count = 0;
    component->settings = count ? g_new0(LSComponentSetting, count + 1) : NULL;
    for (size_t i = 0; i < count; ++i) {
        const json_t* setting = json_object_get(settings, defaults[i].key);
        if (!setting) {
            continue;
        }

        LSComponentSettingValue value = { 0 };
        if (read_setting(setting, defaults[i].val.type, &value, &defaults[i])) {
            component->settings[loaded_count++] = (LSComponentSetting) { .key = defaults[i].key, .val = value };
        } else {
            char* val = json_dumps(setting, JSON_COMPACT | JSON_ENCODE_ANY);
            LOG_WARNF("Setting %s: %s invalid", defaults[i].key, val ? val : "<unavailable>");
            free(val);
        }
    }
}

static const LSComponentSettingValue* setting_find(const LSComponentSetting* settings, const char* key)
{
    for (const LSComponentSetting* setting = settings; setting && setting->key; ++setting) {
        if (strcmp(setting->key, key) == 0) {
            return &setting->val;
        }
    }

    return NULL;
}

const LSComponentSettingValue* ls_component_settings_get(const LSComponent* component, const char* key)
{
    const LSComponentSettingValue* value = setting_find(component->settings, key);
    return value ? value : setting_find(component->ops->settings, key);
}

const LSComponentSettingValue* ls_component_settings_array_get(const LSComponent* component, const char* array_key, size_t index, const char* key)
{
    const LSComponentSettingValue* array = ls_component_settings_get(component, array_key);
    if (!array || array->type != LS_COMPONENT_SETTING_ARRAY || index >= array->array_val.count) {
        return NULL;
    }

    const LSComponentSettingValue* value = setting_find(array->array_val.values[index], key);
    if (value) {
        return value;
    }

    const LSComponentSettingValue* defaults = setting_find(component->ops->settings, array_key);
    return setting_find(defaults->array_val.values[0], key);
}

/**
 * @brief Call the component's delete method when the components list is being destroyed.
 *
 * @param data The component to cleanup.
 */
void ls_component_destroy(gpointer data)
{
    LSComponent* component = data;
    if (component && component->ops && component->ops->delete) {
        LSComponentSetting* settings = component->settings;
        component->ops->delete(component);
        settings_clear(settings);
    }
}
