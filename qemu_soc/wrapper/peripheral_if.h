/*
 * peripheral_if.h
 *
 * Copyright (c) 2026 Prashun Jha. All rights reserved.
 *
 * @author Prashun Jha
 *
 * Pin-level bus contract for user SystemC peripheral models.
 *
 * Defines PeripheralBusSignals (sc_signal bundle) and bind_peripheral(), a
 * template helper that connects a user module's ports to those signals and
 * to the platform clock. Cosim adapters (TlmPinBridge, MmioAdapter) drive
 * the bus side; the user model (e.g. Timer) implements register logic on
 * the peripheral side.
 *
 * Expected user module ports:
 *   clock, reset, read_en, write_en, data_in, address, data_out
 *   intr1, intr2 (optional interrupt outputs)
 */

#ifndef PERIPHERAL_IF_H
#define PERIPHERAL_IF_H

#include <systemc.h>

/*
 * Shared signal bundle between cosim adapter and user peripheral.
 * Platform code instantiates one bundle per peripheral instance.
 */
struct PeripheralBusSignals {
    sc_signal<bool>          reset;
    sc_signal<bool>          read_en;
    sc_signal<bool>          write_en;
    sc_signal<sc_uint<32>>   data_in;
    sc_signal<sc_uint<32>>   address;
    sc_signal<sc_uint<32>>   data_out;
    sc_signal<bool>          intr1;
    sc_signal<bool>          intr2;
};

/*
 * Bind a user module that exposes the standard peripheral port names.
 * Connects clock plus all bus and interrupt signals to the shared bundle.
 *
 * Example:
 *   PeripheralBusSignals bus;
 *   Timer dut("Timer");
 *   bind_peripheral(dut, clk, bus);
 */
template <typename Mod>
void bind_peripheral(Mod &m, sc_clock &clk, PeripheralBusSignals &bus)
{
    m.clock(clk);
    m.reset(bus.reset);
    m.read_en(bus.read_en);
    m.write_en(bus.write_en);
    m.data_in(bus.data_in);
    m.address(bus.address);
    m.data_out(bus.data_out);
    m.intr1(bus.intr1);
    m.intr2(bus.intr2);
}

#endif /* PERIPHERAL_IF_H */
