/*
 * mmio_adapter.h
 *
 * Copyright (c) 2026 Prashun Jha. All rights reserved.
 *
 * @author Prashun Jha
 *
 * Legacy pin-level MMIO adapter (pre-TLM cosim path).
 *
 * MmioAdapter drives the same read_en/write_en/address/data_in/data_out pins
 * as TlmPinBridge, but is invoked directly from CosimServer::server_thread
 * instead of through TLM sockets. The current cosim platform uses the TLM
 * chain (CosimServer -> TlmAddressMap -> TlmPinBridge); this header is
 * retained for reference and simpler bring-up experiments.
 *
 * bus_read()/bus_write() must run inside an SC_THREAD because they call
 * wait(). tick_clocks() advances simulated time by count bus cycles.
 */

#ifndef MMIO_ADAPTER_H
#define MMIO_ADAPTER_H

#include <systemc.h>
#include "peripheral_if.h"

SC_MODULE(MmioAdapter) {
    sc_out<bool>          read_en;
    sc_out<bool>          write_en;
    sc_out<sc_uint<32>>   address;
    sc_out<sc_uint<32>>   data_in;
    sc_in<sc_uint<32>>    data_out;

    sc_time clock_period;

    SC_CTOR(MmioAdapter) : clock_period(10, SC_NS) {}

    /*
     * Perform one read transaction at byte offset. Strobes read_en, samples
     * data_out, then waits clock_period so clocked models can advance.
     */
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
        wait(clock_period);
        return value;
    }

    /*
     * Perform one write transaction at byte offset with the given value.
     * Strobes write_en and waits clock_period for peripheral settling.
     */
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

    /* Advance SystemC time by count * clock_period (explicit clock ticks). */
    void tick_clocks(unsigned count)
    {
        for (unsigned i = 0; i < count; ++i) {
            wait(clock_period);
        }
    }
};

/* Wire MmioAdapter ports to a PeripheralBusSignals signal bundle. */
inline void bind_adapter(MmioAdapter &ad, PeripheralBusSignals &bus)
{
    ad.read_en(bus.read_en);
    ad.write_en(bus.write_en);
    ad.address(bus.address);
    ad.data_in(bus.data_in);
    ad.data_out(bus.data_out);
}

#endif /* MMIO_ADAPTER_H */
