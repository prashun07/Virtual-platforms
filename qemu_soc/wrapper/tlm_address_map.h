/*
 * tlm_address_map.h
 *
 * Copyright (c) 2026 Prashun Jha. All rights reserved.
 *
 * @author Prashun Jha
 *
 * TLM-2.0 address decoder for the cosim peripheral window.
 *
 * CosimServer issues transactions with byte offsets within the PL region
 * (QEMU sends window-relative addresses). map_region() defines the valid
 * range; b_transport() checks bounds, subtracts region_base_, forwards to
 * device_socket, then restores the original address for the caller.
 *
 * Typical wiring:
 *   CosimServer.initiator -> cpu_socket
 *   device_socket -> TlmPinBridge.socket (or native TLM peripheral)
 */

#ifndef TLM_ADDRESS_MAP_H
#define TLM_ADDRESS_MAP_H

#include <cstdint>

#include <systemc.h>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

SC_MODULE(TlmAddressMap) {
    /* Upstream: CosimServer initiator. */
    tlm_utils::simple_target_socket<TlmAddressMap> cpu_socket;

    /* Downstream: peripheral bridge or TLM target. */
    tlm_utils::simple_initiator_socket<TlmAddressMap> device_socket;

    SC_CTOR(TlmAddressMap)
        : cpu_socket("cpu_socket")
        , device_socket("device_socket")
        , region_base_(0)
        , region_size_(0)
    {
        cpu_socket.register_b_transport(this, &TlmAddressMap::b_transport);
    }

    /*
     * Configure the decoded region. For QEMU cosim use base=0 and size equal
     * to the PL window because CosimServer already passes window offsets.
     */
    void map_region(uint64_t base, uint64_t size)
    {
        region_base_ = base;
        region_size_ = size;
    }

private:
    uint64_t region_base_;
    uint64_t region_size_;

    /*
     * TLM target callback: bounds-check, translate address, forward to
     * device_socket, restore original address. Sets TLM_ADDRESS_ERROR_RESPONSE
     * when the transaction falls outside the mapped region.
     */
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
