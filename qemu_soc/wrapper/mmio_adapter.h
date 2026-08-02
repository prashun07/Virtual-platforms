#ifndef MMIO_ADAPTER_H
#define MMIO_ADAPTER_H

#include <systemc.h>
#include "peripheral_if.h"

/*
 * Drives a user peripheral's bus pins for one MMIO access, then returns.
 * No model logic — only pin wiggles + wait for SystemC settling.
 * Must be called from an SC_THREAD (e.g. CosimServer::server_thread).
 */
SC_MODULE(MmioAdapter) {
    sc_out<bool>          read_en;
    sc_out<bool>          write_en;
    sc_out<sc_uint<32>>   address;
    sc_out<sc_uint<32>>   data_in;
    sc_in<sc_uint<32>>    data_out;

    sc_time clock_period;

    SC_CTOR(MmioAdapter) : clock_period(10, SC_NS) {}

    uint32_t bus_read(uint32_t offset)
    {
        address.write(offset);
        data_in.write(0);
        write_en.write(false);
        read_en.write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        uint32_t value = data_out.read();
        read_en.write(false);
        wait(clock_period); /* allow user model clock to advance */
        return value;
    }

    void bus_write(uint32_t offset, uint32_t value)
    {
        address.write(offset);
        data_in.write(value);
        read_en.write(false);
        write_en.write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        write_en.write(false);
        wait(clock_period);
    }

    void tick_clocks(unsigned count)
    {
        for (unsigned i = 0; i < count; ++i) {
            wait(clock_period);
        }
    }
};

inline void bind_adapter(MmioAdapter &ad, PeripheralBusSignals &bus)
{
    ad.read_en(bus.read_en);
    ad.write_en(bus.write_en);
    ad.address(bus.address);
    ad.data_in(bus.data_in);
    ad.data_out(bus.data_out);
}

#endif /* MMIO_ADAPTER_H */
