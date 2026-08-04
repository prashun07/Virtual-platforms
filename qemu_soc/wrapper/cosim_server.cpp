#include "cosim_server.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../protocol/cosim_protocol.h"

CosimServer::CosimServer(sc_module_name name, std::string socket_path)
    : sc_module(name)
    , socket_path_(std::move(socket_path))
    , listen_fd_(-1)
    , client_fd_(-1)
    , quit_(false)
{
    SC_THREAD(server_thread);
}

CosimServer::~CosimServer()
{
    if (client_fd_ >= 0) {
        ::close(client_fd_);
        client_fd_ = -1;
    }
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    unlink(socket_path_.c_str());
}

void CosimServer::start_listening()
{
    unlink(socket_path_.c_str());

    listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("cosim: socket() failed");
    }

    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path)) {
        throw std::runtime_error("cosim: socket path too long");
    }
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s",
                  socket_path_.c_str());

    if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error(std::string("cosim: bind failed: ") +
                                 std::strerror(errno));
    }
    if (listen(listen_fd_, 1) < 0) {
        throw std::runtime_error("cosim: listen failed");
    }

    std::cout << "[cosim] listening on " << socket_path_ << std::endl;
}

bool CosimServer::send_all(int fd, const void *buf, size_t len)
{
    const auto *p = static_cast<const uint8_t *>(buf);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, p + sent, len - sent, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            wait(100, SC_US);
            continue;
        }
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool CosimServer::recv_all(int fd, void *buf, size_t len)
{
    auto *p = static_cast<uint8_t *>(buf);
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::recv(fd, p + got, len - got, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            wait(100, SC_US);
            continue;
        }
        if (n <= 0) {
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

int CosimServer::wait_readable(int fd, sc_time slice)
{
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    int rc = ::poll(&pfd, 1, 0);
    if (rc > 0) {
        return 1;
    }
    wait(slice);
    /* While idle, keep the user model clock advancing. */
    return 0;
}

void CosimServer::server_thread()
{
    const sc_time poll_slice(100, SC_US);

    while (!quit_) {
        if (client_fd_ < 0) {
            int fd = ::accept(listen_fd_, nullptr, nullptr);
            if (fd < 0) {
                wait_readable(listen_fd_, poll_slice);
                continue;
            }
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            client_fd_ = fd;
            std::cout << "[cosim] QEMU connected @" << sc_time_stamp()
                      << std::endl;
        }

        if (wait_readable(client_fd_, poll_slice) == 0) {
            continue;
        }

        CosimRequest req{};
        if (!recv_all(client_fd_, &req, sizeof(req))) {
            std::cout << "[cosim] QEMU disconnected" << std::endl;
            ::close(client_fd_);
            client_fd_ = -1;
            continue;
        }

        CosimResponse resp{};
        resp.magic = COSIM_MAGIC;
        resp.version = COSIM_VERSION;
        resp.status = COSIM_OK;
        resp.data = 0;

        if (req.magic != COSIM_MAGIC || req.version != COSIM_VERSION) {
            resp.status = COSIM_ERROR;
        } else {
            switch (req.op) {
            case COSIM_OP_READ:
                if (!tlm_read(req.addr, resp.data)) {
                    resp.status = COSIM_ERROR;
                }
                break;
            case COSIM_OP_WRITE:
                if (!tlm_write(req.addr, req.data)) {
                    resp.status = COSIM_ERROR;
                }
                break;
            case COSIM_OP_TICK:
                tlm_tick(req.data ? req.data : 1);
                break;
            case COSIM_OP_QUIT:
                quit_ = true;
                break;
            default:
                resp.status = COSIM_ERROR;
                break;
            }
        }

        if (!send_all(client_fd_, &resp, sizeof(resp))) {
            ::close(client_fd_);
            client_fd_ = -1;
        }

        if (quit_) {
            sc_stop();
        }
    }
}

bool CosimServer::tlm_read(uint32_t addr, uint32_t &data)
{
    tlm::tlm_generic_payload trans;
    unsigned char buf[4] = {};
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(buf);
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator->b_transport(trans, delay);
    if (trans.is_response_error()) {
        return false;
    }

    std::memcpy(&data, buf, 4);
    return true;
}

bool CosimServer::tlm_write(uint32_t addr, uint32_t data)
{
    tlm::tlm_generic_payload trans;
    unsigned char buf[4];
    sc_time delay = SC_ZERO_TIME;

    std::memcpy(buf, &data, 4);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(buf);
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator->b_transport(trans, delay);
    return !trans.is_response_error();
}

void CosimServer::tlm_tick(unsigned count)
{
    sc_time delay = SC_ZERO_TIME;
    for (unsigned i = 0; i < count; ++i) {
        wait(10, SC_NS);
    }
    (void)delay;
}
