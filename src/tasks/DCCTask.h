#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Entry point for the DCC FreeRTOS task (runs on Core 0).
// Owns the DCCEXProtocol instance and serial connection.
void dccTaskStart(QueueHandle_t eventQueue, QueueHandle_t cmdQueue);
