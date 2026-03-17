/* dioOS Specific. Embed Controller driver!!! */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <acpi.h>
#include <IO.h>

#define EC_DEV_NAME "PNP0C09"

#define EC_CMD_PORT 0x66
#define EC_STS_PORT 0x66
#define EC_DATA_PORT 0x62

#define EC_STS_OBF (1 << 0)
#define EC_STS_IBF (1 << 1)
#define EC_STS_ECI_EVT (1 << 5)

#define EC_CMD_QR 0x84

ACPI_STATUS acpi_intialize_embedded_controller();
void embedded_controler_enable_gpes();