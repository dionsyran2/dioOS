#include <drivers/drivers.h>
#include <kstdio.h>

base_driver_t* driver_list = nullptr;

void add_driver_to_list(base_driver_t* drv){
    drv->next = nullptr;

    if (driver_list == nullptr){
        driver_list = drv;
        return;
    }

    base_driver_t* c = driver_list;
    while(c->next) c = c->next;

    c->next = drv;
}

void start_drivers(){
    for (base_driver_t* drv = driver_list; drv != nullptr; drv = drv->next){
        if (!drv->start_device()){
            if (drv->device->header){
                kprintf("\e[0;31m[DRIVERS]\e[0m Failed to start driver for device %.4x:%.4x\n", 
                    drv->device->header->vendor_id, drv->device->header->device_id);
            }

            delete drv;
        }
    }
}

void stop_drivers(){

}

void start_deviceless_drivers(){
    for (deviceless_driver_class* driver = __start_deviceless_drivers; driver < __stop_deviceless_drivers; driver++){
        driver->initialize();
    }
}


// Drivers

base_driver_t::base_driver_t(pci_device_t* dev){
    this->device = dev;
    return;
}

base_driver_t::~base_driver_t(){
    return;
}