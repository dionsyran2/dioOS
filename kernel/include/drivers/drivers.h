#pragma once
#include <stdint.h>
#include <stddef.h>
#include <pci.h>
/* Driver Subsystem */

#define DRIVER_SECTION_NAME ".drivers"
#define DEFINE_DRIVER(name) \
    driver_class_t name __attribute__((section(DRIVER_SECTION_NAME))) \
    __attribute__((aligned(8)))


typedef struct driver_class {
    const char* name;
    int type = 0;
    bool (*supports_device)(pci::pci_device_header* dev);
    struct base_driver_t* (*create_instance)(pci::pci_device_header* dev);
} driver_class_t;

class base_driver_t{
    public:
    base_driver_t(pci::pci_device_header* hdr);
    ~base_driver_t();
    
    virtual bool init_device() = 0;
    virtual bool start_device() = 0;
    virtual bool shutdown_device() = 0;

    pci::pci_device_header* pci_device_hdr;
    base_driver_t* next;
};


extern driver_class_t __start_drivers[];
extern driver_class_t __stop_drivers[];

void add_driver_to_list(base_driver_t* drv);

void start_drivers();
void stop_drivers();
void start_deviceless_drivers();

/* DEVICE-LESS DRIVERS (Like localhost, anything that needs to run without an actual device backing it up) */
typedef struct deviceless_driver_class {
    const char* name;
    void (*initialize)();
} deviceless_driver_class;

#define DEVICELESS_DRIVER_SECTION_NAME ".deviceless-drivers"
#define DEFINE_DEVICELESS_DRIVER(name) \
    deviceless_driver_class name __attribute__((section(DEVICELESS_DRIVER_SECTION_NAME))) \
    __attribute__((aligned(8)))


extern deviceless_driver_class __start_deviceless_drivers[];
extern deviceless_driver_class __stop_deviceless_drivers[];