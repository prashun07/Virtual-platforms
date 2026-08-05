#ifndef TIMER_H
#define TIMER_H

#include <systemc.h>
#include "register.h"

/* Memory map (byte offsets) — keep in sync with qemu_soc/firmware/timer_regs.h */
static constexpr uint32_t REG_CTRL  = 0x00;
static constexpr uint32_t REG_VALUE = 0x04;
static constexpr uint32_t REG_CMP   = 0x08;
static constexpr uint32_t REG_INTR   = 0x0C;

/* Control register (REG_CTRL) */
static constexpr uint32_t ENABLE_BIT = 0;
static constexpr uint32_t CMP_BIT    = 1;
static constexpr uint32_t OV_BIT     = 2;

/* Interrupt status register (REG_INTR) — sticky until software clears */
static constexpr uint32_t INTR_CMP_BIT = 1;
static constexpr uint32_t INTR_OV_BIT  = 2;

static constexpr uint32_t INTR_CMP_MASK = (1u << INTR_CMP_BIT);
static constexpr uint32_t INTR_OV_MASK  = (1u << INTR_OV_BIT);

/* Counter wraps and posts overflow when it reaches this value */
static constexpr uint32_t TIMER_OVF_COUNT = 0xFFu;

/*
 * Level-sensitive IRQ outputs (SoC integration):
 *   intr1 -> compare-match pending (NVIC IRQ0 in systemc-soc firmware)
 *   intr2 -> overflow pending       (NVIC IRQ1)
 *
 * INTR register: hardware sets bits on events; software clears by writing
 * the register with those bits cleared (see firmware main.c).
 */
SC_MODULE(Timer) {
    sc_in<bool>          clock;
    sc_in<bool>          reset;
    sc_in<bool>          read_en;
    sc_in<bool>          write_en;
    sc_in<sc_uint<32>>   data_in;
    sc_in<sc_uint<32>>   address;
    sc_out<sc_uint<32>>  data_out;
    sc_out<bool>         intr1;
    sc_out<bool>         intr2;

    Register32 timer_cntrl;
    Register32 timer_intr;

    uint32_t timer_val;
    uint32_t timer_cmp;

    sc_mutex state_mtx;
    sc_event irq_update_ev;

    SC_CTOR(Timer) : clock("clock") {
        SC_THREAD(timer_thread);
        sensitive << clock.pos();

        SC_METHOD(bus_read_method);
        sensitive << read_en.pos();
        dont_initialize();

        SC_METHOD(bus_write_method);
        sensitive << write_en.pos();
        dont_initialize();

        SC_METHOD(reset_method);
        sensitive << reset;

        SC_METHOD(drive_irq_outputs);
        sensitive << irq_update_ev;
        dont_initialize();
    }

    void request_irq_update()
    {
        irq_update_ev.notify(SC_ZERO_TIME);
    }

    void drive_irq_outputs()
    {
        state_mtx.lock();
        const uint32_t intr = timer_intr.read();
        state_mtx.unlock();

        intr1.write((intr & INTR_CMP_MASK) != 0);
        intr2.write((intr & INTR_OV_MASK) != 0);
    }

    /* Caller must hold state_mtx. */
    void post_interrupt(uint32_t mask)
    {
        timer_intr.write(timer_intr.read() | mask);
    }

    void reset_method()
    {
        if (!reset.read()) {
            return;
        }

        state_mtx.lock();
        timer_cntrl.reset();
        timer_val = 0;
        timer_cmp = 0;
        timer_intr.reset();
        state_mtx.unlock();

        request_irq_update();
    }

    void bus_read_method()
    {
        if (!read_en.read()) {
            return;
        }

        const uint32_t offset = address.read();
        uint32_t rdata = 0;

        state_mtx.lock();
        switch (offset) {
        case REG_CTRL:
            rdata = timer_cntrl.read();
            break;
        case REG_VALUE:
            rdata = timer_val;
            break;
        case REG_CMP:
            rdata = timer_cmp;
            break;
        case REG_INTR:
            rdata = timer_intr.read();
            break;
        default:
            break;
        }
        state_mtx.unlock();

        data_out.write(rdata);
    }

    void bus_write_method()
    {
        if (!write_en.read()) {
            return;
        }

        const uint32_t offset = address.read();
        const uint32_t wdata = data_in.read();
        bool irq_changed = false;

        state_mtx.lock();
        switch (offset) {
        case REG_CTRL:
            timer_cntrl.write(wdata);
            break;
        case REG_VALUE:
            timer_val = wdata;
            break;
        case REG_CMP:
            timer_cmp = wdata;
            break;
        case REG_INTR:
            timer_intr.write(wdata);
            irq_changed = true;
            break;
        default:
            break;
        }
        state_mtx.unlock();

        if (irq_changed) {
            request_irq_update();
        }
    }

    void timer_thread()
    {
        while (true) {
            wait();

            state_mtx.lock();

            const uint32_t ctrl = timer_cntrl.read();
            if (!(ctrl & (1u << ENABLE_BIT))) {
                state_mtx.unlock();
                continue;
            }

            timer_val++;

            uint32_t pending = 0;
            if ((ctrl & (1u << CMP_BIT)) && (timer_val == timer_cmp)) {
                pending |= INTR_CMP_MASK;
            }
            if ((ctrl & (1u << OV_BIT)) && (timer_val == TIMER_OVF_COUNT)) {
                timer_val = 0;
                pending |= INTR_OV_MASK;
            }

            if (pending != 0) {
                post_interrupt(pending);
            }

            state_mtx.unlock();

            if (pending != 0) {
                request_irq_update();
            }
        }
    }
};

#endif
