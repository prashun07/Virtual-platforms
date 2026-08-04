#ifndef TLM_PIN_BRIDGE_H
#define TLM_PIN_BRIDGE_H

#include <cstring>

#include <systemc.h>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "peripheral_if.h"

/*
 * TLM-2.0 target that translates generic payload transactions into pin-level
 * bus cycles for legacy user models (e.g. Timer with read_en/write_en ports).
 */
SC_MODULE(TlmPinBridge) {
    tlm_utils::simple_target_socket<TlmPinBridge> socket;

    sc_out<bool>          read_en;
    sc_out<bool>          write_en;
    sc_out<sc_uint<32>>   address;
    sc_out<sc_uint<32>>   data_in;
    sc_in<sc_uint<32>>    data_out;

    sc_time clock_period;

    SC_CTOR(TlmPinBridge)
        : socket("socket")
        , clock_period(10, SC_NS)
    {
        socket.register_b_transport(this, &TlmPinBridge::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
    {
        const uint32_t offset = static_cast<uint32_t>(trans.get_address());

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            if (trans.get_data_length() != 4) {
                trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
                return;
            }
            uint32_t value = bus_read(offset);
            std::memcpy(trans.get_data_ptr(), &value, 4);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            if (trans.get_data_length() != 4) {
                trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
                return;
            }
            uint32_t value;
            std::memcpy(&value, trans.get_data_ptr(), 4);
            bus_write(offset, value);
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += clock_period;
    }

private:
    uint32_t bus_read(uint32_t offset)
    {
        address.write(offset);
        data_in.write(0);
        write_en.write(false);
        read_en.write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        const uint32_t value = data_out.read();
        read_en.write(false);
        wait(clock_period);
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
};

inline void bind_pin_bridge(TlmPinBridge &bridge, PeripheralBusSignals &bus)
{
    bridge.read_en(bus.read_en);
    bridge.write_en(bus.write_en);
    bridge.address(bus.address);
    bridge.data_in(bus.data_in);
    bridge.data_out(bus.data_out);
}

#endif /* TLM_PIN_BRIDGE_H */
