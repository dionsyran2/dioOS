#include <drivers/serial/serial.h>
#include <drivers/filesystems/devfs/devfs.h>
#include <scheduling/apic/ioapic.h>
#include <interrupts/interrupts.h>
#include <IO.h>
#include <cstr.h>
#include <kerrno.h>
#include <drivers/drivers.h>

void InitSerial();

DEFINE_DEVICELESS_DRIVER(serial_driver) = {
    .name = "serial_driver",
    .initialize = InitSerial
};


uint16_t serial_port_addresses[] = {
    0x3F8,
    0x2F8,
    0x3E8,
    0x2E8
};

uint8_t serial_port_irq_line[] = {
    4,
    3,
    4,
    3
};


bool is_com_valid(uint16_t address){
    // Get the scratch register offset
    uint16_t scratch = SERIAL_SCRATCH_REGISTER(address);

    // Write the test byte to the scratch register
    uint8_t test_byte = 0xAE;
    outb(scratch, test_byte);

    // Wait a bit
    io_wait();

    // Read it back
    uint8_t res = inb(scratch);

    // Compare it
    if (test_byte == res) return true;

    // If they are not equals, we read some floating pins or something from another device
    // In either case its not a serial port

    return false;
}


void InitSerial(){
    // Loop through every possible address
    for (int i = 0; i < (sizeof(serial_port_addresses) / sizeof(uint16_t)); i++){
        // Check if its valid
        if (!is_com_valid(serial_port_addresses[i])) continue;

        // Calculate a path
        char buffer[24];
        stringf(buffer, sizeof(buffer), "/ttyS%d", i);

        // Create the serial port object
        serial_port *com = new serial_port(serial_port_addresses[i], serial_port_irq_line[i]);

        // Register it
        line_discipline::register_vt(com, buffer);
    }
}

void serial_irq_handler(void *ctx){
    ((serial_port*)ctx)->handle_interrupt();
};

serial_port::serial_port(uint16_t port, uint8_t irq){
    this->address = port;

    // SHUT EVERYTHING DOWN FIRST
    outb(port + 1, 0x00); // IER: Disable all UART interrupts
    outb(port + 4, 0x00); // MCR: Disable OUT2 (Physically disconnects the IRQ line)

    // Configure Baud Rate & Line Settings
    outb(port + 3, 0x80); // Enable DLAB
    outb(port + 0, 0x03); // Divisor Low  (38400)
    outb(port + 1, 0x00); // Divisor High
    outb(port + 3, 0x03); // Disable DLAB, 8N1
    
    // Configure FIFO (1-byte threshold)
    outb(port + 2, 0x07); 

    add_dynamic_isr(COM_PORT_INT_VECTOR, serial_irq_handler, this);
    set_apic_irq(irq, COM_PORT_INT_VECTOR, false); 

    // Drain the uart completely to ensure its internal state is LOW
    inb(port + 2); // IIR
    inb(port + 5); // LSR
    inb(port + 6); // MSR
    inb(port + 0); // RBR (Discard ghost bytes)

    // CONNECT THE WIRE to the IOAPIC
    outb(port + 4, 0x0B); // Set OUT2, RTS, DTR

    // ARM THE UART to fire interrupts on new data!
    outb(port + 1, 0x01); // Enable RBR Interrupts

    ws.ws_row = 25;
    ws.ws_col = 80;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
}

void serial_port::handle_interrupt(){
    // Read Line Status Register to see if data is waiting
    while ((inb(this->address + 5) & 0x01)) {
        char c = inb(this->address + 0);

        if (this->input_handler){
            this->input_handler(c, this->input_handler_ctx);
        }
    }
}


void serial_port::write(const char *data, size_t size, bool onlcr){
    for (size_t i = 0; i < size; i++) {
        if (onlcr && data[i] == '\n') {
            while ((inb(this->address + 5) & 0x20) == 0);
            outb(this->address + 0, '\r');
        }

        while ((inb(this->address + 5) & 0x20) == 0);
        
        outb(this->address + 0, data[i]);
    }
}

int serial_port::ioctl(int op, char *argp){
    return -EOPNOTSUPP;
}

void serial_port::apply_termios(termios *state){
    // Extract baud rate speed from termios
    uint32_t baud = 9600; // default
    if (state->c_cflag & B115200) baud = 115200;
    else if (state->c_cflag & B38400) baud = 38400;
    else if (state->c_cflag & B9600) baud = 9600;

    uint16_t divisor = 1843200 / (16 * baud);

    // Enable DLAB (Divisor Latch Access Bit) to write baud rate
    outb(this->address + 3, inb(this->address + 3) | 0x80);

    // Write divisor (Low byte, then High byte)
    outb(this->address + 0, (uint8_t)(divisor & 0xFF));
    outb(this->address + 1, (uint8_t)((divisor >> 8) & 0xFF));

    // Disable DLAB and set line control: 8 data bits, no parity, 1 stop bit (8N1)
    outb(this->address + 3, 0x03);
}


/* FOR DEBUGGING... TO BE REMOVED */
// @brief Writes one byte to the serial port
void serialWrite(uint16_t port, char c) {
    while ((inb(port + 5) & 0x20) == 0);
    outb(port, c);
}

// @brief Prints a string on the serial port
void serialPrint(uint16_t port, const char* str) {
    while (*str) {
        serialWrite(port, *str++);
    }
}

#include <printf.h>

void serialfva(const char* str, va_list args){
    if (str == nullptr) return;
    
    const uint64_t buffer_size = 2048;
    char buffer[buffer_size] = { 0 };
    int written = vsnprintf_(buffer, buffer_size, str, args);
    serialPrint(0x3F8, buffer);
}

#include <scheduling/spinlock/spinlock.h>
spinlock_t seriallock;
// @brief Prints a formatted string to the serial port
void serialf(const char* str, ...){
    uint64_t rflags = spin_lock(&seriallock);
    
    va_list args;
    va_start(args, str);
    serialfva(str, args);
    va_end(args);

    spin_unlock(&seriallock, rflags);
}