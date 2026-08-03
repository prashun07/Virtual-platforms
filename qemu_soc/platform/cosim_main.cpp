/*
 * Cosimulation platform: wires the USER SystemC model to the QEMU bridge.
 *
 * To simulate a different peripheral later, replace the Timer include/instance
 * and keep the same bind_peripheral() / MmioAdapter / CosimServer wiring.
 */

#include <cstdlib>
#include <iostream>
#include <string>

#include <systemc.h>

#include "peripheral_if.h"
#include "mmio_adapter.h"
#include "cosim_server.h"

/* User model — do not modify; only bind through the wrapper interface. */
#include "../../Timer/timer.h"

int sc_main(int argc, char *argv[])
{
    const char *sock_env = std::getenv("SYSTEMC_COSIM_SOCKET");
    std::string socket_path =
        sock_env && sock_env[0] ? sock_env : "/tmp/systemc_cosim.sock";

    if (argc > 1) {
        socket_path = argv[1];
    }

    sc_clock clk{"clk", 10, SC_NS,0.5,10,SC_NS,false};

    PeripheralBusSignals bus;
    Timer dut{"Timer"};
    bind_peripheral(dut, clk, bus);

    MmioAdapter adapter{"adapter"};
    bind_adapter(adapter, bus);

    CosimServer server{"cosim", socket_path};
    server.adapter = &adapter;
    server.start_listening();

    /* Release reset after a couple of clocks. */
    bus.reset.write(true);
    sc_start(20, SC_NS);
    bus.reset.write(false);

    std::cout << "[platform] SystemC model ready; waiting for QEMU on "
              << socket_path << std::endl;

    sc_start();

    std::cout << "[platform] cosim finished at " << sc_time_stamp() << std::endl;

    return 0;
}
