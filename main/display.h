
#pragma once

extern TaskHandle_t displayTaskHandle;

void update_display_widgets();

constexpr uint32_t DISPLAY_NOTIFY_TOUCH = 0x01;
constexpr uint32_t DISPLAY_UPDATE_WIDGETS = 0x02;

