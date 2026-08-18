#ifndef TIMER_REGISTER_H
#define TIMER_REGISTER_H

#include "register.h"

/* Typed register helpers — bit positions match timer.h / timer_regs.h */

class timer_cntrl_reg : public Register32 {
public:
    using Register32::Register32;

    bool is_enable() const { return value & (1u << 0); }
    bool is_cmp_enabled() const { return value & (1u << 1); }
    bool is_overflow_enabled() const { return value & (1u << 2); }

    void set_enable(bool flag)
    {
        if (flag) {
            value |= (1u << 0);
        } else {
            value &= ~(1u << 0);
        }
    }

    void set_compare(bool flag)
    {
        if (flag) {
            value |= (1u << 1);
        } else {
            value &= ~(1u << 1);
        }
    }

    void set_overflow(bool flag)
    {
        if (flag) {
            value |= (1u << 2);
        } else {
            value &= ~(1u << 2);
        }
    }
};

class timer_intr_reg : public Register32 {
public:
    using Register32::Register32;

    static constexpr uint32_t CMP_MASK = (1u << 1);
    static constexpr uint32_t OV_MASK  = (1u << 2);

    void set_cmp_pending(bool flag)
    {
        if (flag) {
            value |= CMP_MASK;
        } else {
            value &= ~CMP_MASK;
        }
    }

    void set_ov_pending(bool flag)
    {
        if (flag) {
            value |= OV_MASK;
        } else {
            value &= ~OV_MASK;
        }
    }

    bool cmp_pending() const { return (value & CMP_MASK) != 0; }
    bool ov_pending() const { return (value & OV_MASK) != 0; }
};

#endif /* TIMER_REGISTER_H */
