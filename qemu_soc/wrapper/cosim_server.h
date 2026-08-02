#ifndef COSIM_SERVER_H
#define COSIM_SERVER_H

#include <cstdint>
#include <string>

#include <systemc.h>
#include "mmio_adapter.h"

/*
 * Unix-socket front-end for QEMU remote-mmio.
 * Runs entirely in a SystemC thread (no host-thread SystemC calls).
 */
SC_MODULE(CosimServer) {
    MmioAdapter *adapter;

    SC_HAS_PROCESS(CosimServer);
    CosimServer(sc_module_name name, std::string socket_path);

    ~CosimServer();

    /* Call before sc_start(). Creates the listening socket. */
    void start_listening();

private:
    std::string socket_path_;
    int listen_fd_;
    int client_fd_;
    bool quit_;

    void server_thread();
    bool send_all(int fd, const void *buf, size_t len);
    bool recv_all(int fd, void *buf, size_t len);
    int wait_readable(int fd, sc_time slice);
};

#endif /* COSIM_SERVER_H */
