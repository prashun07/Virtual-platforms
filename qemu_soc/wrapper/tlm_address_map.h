#ifndef TLM_ADDRESS_MAP_H
#define TLM_ADDRESS_MAP_H

#include <cstdint>

#include <systemc.h>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

/*
 * TLM-2.0 address decoder with one downstream port (scalable to N ports later).
 *
 * CPU-side target receives absolute addresses from CosimServer initiator,
 * subtracts region base, forwards via initiator socket to peripheral target.
 */
SC_MODULE(TlmAddressMap) {
    tlm_utils::simple_target_socket<TlmAddressMap> cpu_socket;
    tlm_utils::simple_initiator_socket<TlmAddressMap> device_socket;

    SC_CTOR(TlmAddressMap)
        : cpu_socket("cpu_socket")
        , device_socket("device_socket")
        , region_base_(0)
        , region_size_(0)
    {
        cpu_socket.register_b_transport(this, &TlmAddressMap::b_transport);
    }

    void map_region(uint64_t base, uint64_t size)
    {
        region_base_ = base;
        region_size_ = size;
    }

private:
    uint64_t region_base_;
    uint64_t region_size_;

    void b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
    {
        const uint64_t addr = trans.get_address();

        if (region_size_ == 0 ||
            addr < region_base_ ||
            addr >= region_base_ + region_size_) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        trans.set_address(addr - region_base_);
        device_socket->b_transport(trans, delay);
        trans.set_address(addr);
    }
};

#endif /* TLM_ADDRESS_MAP_H */
