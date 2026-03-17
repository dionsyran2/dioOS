#include <drivers/drivers.h>
#include <drivers/ps2/ps2.h>
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
            if (drv->pci_device_hdr){
                kprintf("\e[0;31m[DRIVERS]\e[0m Failed to start driver for device %.4x:%.4x\n", 
                    drv->pci_device_hdr->vendor_id, drv->pci_device_hdr->device_id);
            }

            delete drv;
        }
    }

    ps2::initialize_ps2();
}

void stop_drivers(){

}


// Drivers

base_driver_t::base_driver_t(pci::pci_device_header* hdr){
    this->pci_device_hdr = hdr;
    return;
}

base_driver_t::~base_driver_t(){
    return;
}