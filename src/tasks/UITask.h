#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Entry point for the UI FreeRTOS task (runs on Core 1).
// Owns the display and encoder inputs.
void uiTaskStart(QueueHandle_t eventQueue, QueueHandle_t cmdQueue);
