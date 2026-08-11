#pragma once
#include <stdint.h>
#include <stddef.h>
#include <interfaces/evdev.h>

void input_core_report_event(evdev_t *source_device, input_event *event);