
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern TaskHandle_t displayTaskHandle;

void update_display_widgets();
bool getDisplayBacklight();

constexpr uint32_t DISPLAY_NOTIFY_TOUCH = 0x01;
constexpr uint32_t DISPLAY_UPDATE_WIDGETS = 0x02;

