/*
 * cosim_server.h
 *
 * Copyright (c) 2026 Prashun Jha. All rights reserved.
 *
 * @author Prashun Jha
 *
 * SystemC module declaration for the QEMU cosimulation socket server.
 *
 * CosimServer exposes a TLM-2.0 initiator socket that downstream platform
 * code connects to TlmAddressMap (or other bus fabric). Guest MMIO from
 * QEMU arrives as SCM1 messages; the implementation in cosim_server.cpp
 * translates them into b_transport() calls on initiator.
 */

#ifndef COSIM_SERVER_H
#define COSIM_SERVER_H

#include <cstdint>
#include <cstring>
#include <string>

#include <systemc.h>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

/* Socket front-end: QEMU remote-mmio <-> TLM initiator. */
SC_MODULE(CosimServer) {
    /* Downstream connection to TlmAddressMap::cpu_socket. */
    tlm_utils::simple_initiator_socket<CosimServer> initiator;

    SC_HAS_PROCESS(CosimServer);

    /* Store socket_path; register server_thread. Call start_listening() before sc_start(). */
    CosimServer(sc_module_name name, std::string socket_path);

    /* Close fds and unlink the Unix socket file. */
    ~CosimServer();

    /* Bind and listen on socket_path (non-blocking listen fd). */
    void start_listening();

private:
    std::string socket_path_;
    int listen_fd_;
    int client_fd_;
    bool quit_;

    /* Accept QEMU, read CosimRequest, dispatch, send CosimResponse. */
    void server_thread();

    /* Reliable send/recv helpers for non-blocking socket fds. */
    bool send_all(int fd, const void *buf, size_t len);
    bool recv_all(int fd, void *buf, size_t len);

    /* Poll fd; wait(slice) and advance sim time when idle. */
    int wait_readable(int fd, sc_time slice);

    /* 32-bit TLM access helpers used by server_thread. */
    bool tlm_read(uint32_t addr, uint32_t &data);
    bool tlm_write(uint32_t addr, uint32_t data);
    void tlm_tick(unsigned count);
};

#endif /* COSIM_SERVER_H */
