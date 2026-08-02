/*
 * Expected pin-level interface for user SystemC peripherals.
 *
 * Your model (e.g. Timer) should expose these ports. The cosim adapter
 * drives them from QEMU MMIO transactions — it does not implement the model.
 *
 *   sc_in<bool>          clock
 *   sc_in<bool>          reset
 *   sc_in<bool>          read_en
 *   sc_in<bool>          write_en
 *   sc_in<sc_uint<32>>   data_in
 *   sc_in<sc_uint<32>>   address      // byte offset within peripheral
 *   sc_out<sc_uint<32>>  data_out
 *   sc_out<bool>         intr1        // optional
 *   sc_out<bool>         intr2        // optional
 */

#ifndef PERIPHERAL_IF_H
#define PERIPHERAL_IF_H

#include <systemc.h>

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
 * Bind a user module that matches the port names above.
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
