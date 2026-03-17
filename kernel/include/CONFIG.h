#pragma once

#define KERNEL_VERSION_STRING "5.0.1.0"

//#define NVME_LOGGING
#define BUILTIN_DEBUGGER
//#define ACPICA_FULL_LOGGING
#define EXPOSE_PCI_DEVICES_IN_VFS


// It will print every device it finds on the pci bus during enumeration
//#define LOG_PCI_DEVICE_ENUMERATION