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