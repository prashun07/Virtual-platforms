#ifndef COSIM_SERVER_H
#define COSIM_SERVER_H

#include <cstdint>
#include <cstring>
#include <string>

#include <systemc.h>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

/*
 * Unix-socket front-end for QEMU remote-mmio.
 * Converts socket READ/WRITE into TLM-2.0 transactions on initiator socket.
 */
SC_MODULE(CosimServer) {
    tlm_utils::simple_initiator_socket<CosimServer> initiator;

    SC_HAS_PROCESS(CosimServer);
    CosimServer(sc_module_name name, std::string socket_path);

    ~CosimServer();

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

    bool tlm_read(uint32_t addr, uint32_t &data);
    bool tlm_write(uint32_t addr, uint32_t data);
    void tlm_tick(unsigned count);
};

#endif /* COSIM_SERVER_H */
