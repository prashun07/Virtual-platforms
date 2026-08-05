# Timer Model — Senior SystemC / C++ Interview Q&A

Interview-style questions and answers grounded in this `Timer` peripheral and its QEMU + SystemC cosimulation integration. Use these to explain design trade-offs, not just API trivia.

---

## 1. Architecture & partitioning

### Q: How is the Timer module structured, and why split bus logic from the counter?

**A:** The model has four logical pieces:

| Process | Type | Role |
|---------|------|------|
| `timer_thread` | `SC_THREAD` | Clocked counter, compare/overflow detection |
| `bus_read_method` | `SC_METHOD` | MMIO read on `read_en` posedge |
| `bus_write_method` | `SC_METHOD` | MMIO write on `write_en` posedge |
| `irq_output_method` | `SC_METHOD` | Drives `intr1`/`intr2` from `INTR` register |
| `reset_method` | `SC_METHOD` | Async reset of state |

Splitting bus access from the timer FSM mirrors real hardware: the bus interface is combinational/registered slave logic; the counter runs on `clock`. Software sees a memory-mapped peripheral; internally, two clock domains of *behavior* exist (bus events vs timer clock), even if both share one `sc_clock` in this lab model.

---

### Q: Why `SC_THREAD` for the counter but `SC_METHOD` for the bus?

**A:**

- **`SC_THREAD`** can `wait()` on clock edges in a `while(true)` loop — natural for sequential hardware (increment each cycle).
- **`SC_METHOD`** runs to completion in zero simulated time (no `wait()` inside). It fits combinational or single-cycle bus decode: sample strobes, update registers, return.

Using `SC_METHOD` for the bus avoids spawning a thread that blocks on every strobe and keeps bus turnaround predictable (one evaluation per `read_en`/`write_en` rising edge).

---

### Q: What is the register map, and how does it relate to firmware?

**A:**

| Offset | Name | Access | Purpose |
|--------|------|--------|---------|
| `0x00` | CTRL | R/W | bit0 ENABLE, bit1 CMP_EN, bit2 OV_EN |
| `0x04` | VALUE | R/W | Current counter |
| `0x08` | CMP | R/W | Compare threshold |
| `0x0C` | INTR | R/W | bit1 compare pending, bit2 overflow pending |

Firmware (`qemu_soc/firmware/timer_regs.h`) uses the same layout. Absolute address depends on the SoC map (e.g. `0x40000000` on Cortex-M3, `0xF0000000` on Cortex-A9 PS).

---

## 2. SystemC scheduling & sensitivity

### Q: What does `sensitive << read_en.pos()` mean, and why not `sensitive << read_en`?

**A:** `read_en.pos()` fires the method only on a **0→1 transition** (posedge). That models a bus protocol where the master asserts `read_en` for one cycle to start a read.

Level sensitivity (`sensitive << read_en`) would re-run the method on **every** edge (assert and de-assert), often sampling `read_en` low on the falling edge and doing nothing useful — or worse, missing the transaction if the method runs after the strobe de-asserted.

The testbench must align strobes with the clock (posedge assert, next posedge de-assert) — same expectation as `TlmPinBridge` in cosim.

---

### Q: What is `dont_initialize()` on `SC_METHOD`, and why use it?

**A:** By default, every `SC_METHOD` runs once at **simulation time 0** before `sc_start()` advances time. `dont_initialize()` skips that initial run.

We use it on bus and IRQ methods so they do not drive outputs or interpret stale pin values before reset and first real bus cycles. `reset_method` is *not* `dont_initialize()` so reset can initialize state at time 0 if `reset` is asserted.

---

### Q: Explain delta cycles vs timed cycles in this model.

**A:**

- **Timed cycle:** `timer_thread` waits on `clock.pos()` — advances simulation time by the clock period (e.g. 10 ns).
- **Delta cycle:** `SC_METHOD` runs without advancing time; multiple methods can run in the same simulation time slot in delta cycles.

`irq_changed_ev.notify()` schedules `irq_output_method` in a later delta at the **same** simulation time when `INTR` changes — IRQ pins update without waiting for the next clock edge. `sensitive << clock.pos()` on the same method also refreshes level-sensitive IRQ outputs each clock edge.

---

## 3. Interrupt design

### Q: How do interrupts work in this Timer?

**A:**

1. **Status register (`INTR`):** Hardware sets sticky bits when events occur (`post_interrupt()` ORs into `timer_intr`).
2. **IRQ pins:** `intr1` = compare pending (bit 1), `intr2` = overflow pending (bit 2). Level-sensitive outputs.
3. **Software clear:** Firmware writes `INTR` with bits cleared: `TIMER_REG_INTR &= ~TIMER_INTR_CMP` — not W1C, but a full register write with updated value.
4. **Enable gating:** Compare/overflow only post if `CMP_EN` / `OV_EN` are set in `CTRL`.

NVIC wiring in `systemc-soc`: `intr1` → IRQ0, `intr2` → IRQ1 (see `firmware/startup.s`).

---

### Q: Why is `irq_output_method` the only driver of `intr1`/`intr2`?

**A:** In SystemC, an `sc_out` connected to an `sc_signal` allows **only one process** to write it per module. If `reset_method`, `bus_write_method`, and `timer_thread` all called `intr1.write()`, you get:

```text
Error (E115): sc_signal<T> cannot have more than one driver
```

Centralizing IRQ pin updates in one `SC_METHOD` is the standard fix. Internal state (`timer_intr`) can be updated from many places; outputs are driven from one place.

---

### Q: Why `irq_changed_ev.notify()` only when `INTR` changes, not every delta?

**A:** `commit_intr()` compares old and new values:

```cpp
if (timer_intr.read() == value) return;
timer_intr.write(value);
irq_changed_ev.notify();
```

This avoids pointless re-evaluation when software writes the same value or hardware re-sets an already-set bit. Combined with `sensitive << clock.pos()`, IRQ outputs update on **meaningful status changes** or **clock edges** — not on arbitrary `notify(SC_ZERO_TIME)` spam every delta.

---

### Q: W1C vs read-to-clear vs write-mask-clear — which does this model use?

**A:** **Write-mask-clear (software RMW):** Driver reads `INTR`, clears bits in software, writes back. Simple for firmware; requires the model to accept a full write to `REG_INTR`.

Production IPs often use **W1C** (write 1 to clear). If you migrated to W1C, `bus_write_method` for `REG_INTR` would become:

```cpp
timer_intr.write(timer_intr.read() & ~wdata);  // W1C
```

Document the chosen semantics in the programmer's model — driver writers depend on it.

---

## 4. Concurrency & correctness

### Q: Do you need a mutex between `timer_thread` and the bus methods?

**A:** For this **untimed, single-clock lab model** with cooperative SystemC scheduling: **usually no.** Only one process runs at a time until it `wait()`s or returns. Bus methods complete in zero time; the timer thread runs on clock edges.

You would add synchronization (`sc_mutex`, lock-free queues, or moving all state into clocked `SC_CTHREAD`) if you:

- Model explicit bus wait states overlapping counter logic
- Share state with another simulator thread (e.g. SystemC + pthread cosim server) without delta alignment
- Target formal verification of absence of races

Over-using mutexes in simple SystemC peripherals adds complexity without fixing a real bug in cooperative simulation.

---

### Q: What happens if firmware polls `INTR` in a tight loop during cosim?

**A:** Each poll is an MMIO read → QEMU socket → `CosimServer` → TLM → `TlmPinBridge` → bus read. The timer thread still advances on `clock.pos()` when simulation time moves. `CosimServer` advances time while waiting on the socket (`wait(100, SC_US)`), so the counter keeps ticking during idle polls — important for compare/overflow tests.

---

### Q: Compare match uses `timer_val == timer_cmp` after increment. Any pitfalls?

**A:**

- **Exact equality:** If software writes `CMP` below current `VALUE` while enabled, compare may never fire until wrap/reset.
- **One-shot per match:** Re-arming requires count to leave and re-enter equality (or clear status and change CMP).
- **Overflow at `0xFF`:** Not full 32-bit wrap — document `TIMER_OVF_COUNT` for software.

Production timers add capture, auto-reload, and prescalers to avoid these footguns.

---

## 5. C++ & modeling style

### Q: Why `static constexpr` for register offsets and bit masks?

**A:** Compile-time constants:

- No runtime cost
- Shared across firmware headers and SystemC if duplicated deliberately
- Type-safe bit masks (`1u << INTR_CMP_BIT`) avoid magic numbers

Senior follow-up: prefer a single generated header (RDL/SystemRDL, IP-XACT) for RTL, firmware, and SystemC — this project manually keeps `timer_regs.h` and `timer.h` in sync.

---

### Q: What is `Register32`, and would you use it in production?

**A:** A minimal C++ class holding a `uint32_t` with `read()`/`write()`/`reset()`. It is **not** a SystemC signal — just module-internal state.

`timer_register.h` adds typed helpers (`timer_cntrl_reg`, `timer_intr_reg`) for bit operations. Production models might use:

- `sc_bv<32>` or `sc_uint<32>` for visibility in waveforms
- Separate bitfields with read/write side effects (clear-on-read, lock bits)
- Generated register classes from a spec

---

### Q: `timer.h` is header-only `SC_MODULE`. Pros and cons?

**A:**

| Pros | Cons |
|------|------|
| Easy to include in testbench and cosim | Long compile times if large |
| All logic visible in one place | Harder to unit-test pieces in isolation |
| Fine for small IPs | Temptation to grow without `.cpp` separation |

For a portfolio peripheral, header-only is acceptable. For industry, split `timer_bus.cpp` / `timer_core.cpp` or use SystemC CCI/configuration.

---

## 6. TLM, pins, and QEMU cosim

### Q: How does software on QEMU reach this Timer?

**A:**

```text
Firmware (Cortex-M3/A9)
    → MMIO load/store
    → QEMU remote-mmio (system mode, not user mode)
    → Unix socket (SCM1 protocol)
    → CosimServer (TLM initiator)
    → TlmAddressMap (offset decode)
    → TlmPinBridge (TLM → read_en/write_en pins)
    → Timer::bus_*_method
```

The Timer model does not know about QEMU. It only sees pin-level bus strobes — good separation of concerns.

---

### Q: Why TLM sockets instead of direct `MmioAdapter` pin wiggling from the server?

**A:** TLM-2.0 is the standard for virtual platforms:

- Scales to multiple initiators/targets, address maps, bridges
- Native TLM peripherals plug in without pin translation
- `TlmPinBridge` is a **legacy adapter** for existing pin-level models like this Timer

Senior answer: new IP should be native `tlm_target_socket`; keep pin bridge only for imported RTL-style models.

---

### Q: QEMU runs in system mode here — why does that matter?

**A:** `qemu-system-arm` emulates a full machine (CPU, memory map, devices). MMIO to `0x40000000` hits `remote-mmio` and forwards to SystemC.

**User-mode** QEMU (`qemu-arm`) runs Linux userspace binaries on the host — no custom memory map, no `remote-mmio`. Cosim requires **system mode** (`arm-softmmu`).

---

## 7. Verification & testbench

### Q: How is the Timer verified?

**A:**

1. **`timer_tb.cpp`:** SystemC testbench — register R/W, counting, compare/overflow `INTR` bits, `intr1`/`intr2` assert/clear, disable behavior.
2. **`qemu_soc/firmware`:** Baremetal on virtual SoC — end-to-end MMIO through QEMU socket.
3. **Clock-aligned strobes:** `read_reg`/`write_reg` wait on `clk.posedge_event()` so posedge-sensitive bus methods see valid strobes.

---

### Q: What coverage gaps would you call out in an interview?

**A:**

- No back-pressure or bus error response on bad addresses
- No formal protocol checker on `read_en`/`write_en` mutual exclusion
- IRQ to QEMU NVIC not fully wired (firmware polls; async IRQ path reserved)
- No power/clock-gating, no metastability synchronizers
- Overflow width fixed at 8 bits (`0xFF`) — not documented in a programmer's guide PDF
- No UVM/SCV constrained-random register tests

---

## 8. “What would you improve?” (classic senior question)

**A:** A strong answer is prioritized:

1. **Programmer's model doc** — W1C vs RW, reset values, clock frequency assumption.
2. **Native TLM target** — drop pin bridge for this block; annotate `b_transport` delay.
3. **IRQ back-channel** — SystemC `intr1`/`intr2` → QEMU `remote-mmio` IRQ lines → NVIC/GIC.
4. **SC_CTHREAD** for the counter (`reset_signal_is`) — clearer clock domain than `SC_THREAD` + `wait(clock)`.
5. **Generated registers** from single source (RDL) for RTL/firmware/SystemC alignment.
6. **APB/AXI-Lite** bus interface instead of custom strobes — industry standard.

---

## 9. Quick-fire C++ / SystemC

| Question | Short answer |
|----------|----------------|
| `SC_MODULE` vs `sc_module` | Macro wraps constructor registration; same idea |
| Can `SC_METHOD` call `wait()`? | No — becomes `SC_THREAD` behavior / error |
| `sc_start()` vs `sc_pause()` | Run until stop/pause; cosim server loops with timed `wait` |
| `sc_event::notify()` vs `notify(SC_ZERO_TIME)` | Immediate delta vs explicit zero-time offset |
| Why `#include <systemc.h>` | Accellera installs often use `.h`; older code used `<systemc>` |
| `sc_out` vs `sc_signal` | Port vs channel; module drives `sc_out`, bound to signal |
| Multiple drivers on `sc_signal` | Illegal unless `sc_buffer` / resolved port / single writer |

---

## 10. Sample “tell me about a bug you fixed” story

**A (cosim semihosting):** Firmware used `bkpt 0xAB` for semihosting but did not mark `r0`/`r1` as clobbered in inline asm. After `SYS_WRITE0`, QEMU left `r0 = 0xdeadbeef`; the next semihosting call used garbage as the operation number → `Unsupported SemiHosting SWI 0xdeadbeef`. Fix: explicit `mov` into `r0`/`r1` and clobber list.

**A (IRQ pins):** `intr1`/`intr2` were declared but never driven — software polling `INTR` worked, but IRQ outputs and NVIC path were dead. Fix: central `irq_output_method` + `commit_intr()` on status changes.

**A (bus strobes):** Level-sensitive `SC_THREAD` bus missed writes when the testbench de-asserted strobes before the thread ran. Fix: posedge `SC_METHOD` + clock-aligned testbench strobes.

---

## Related files

| File | Role |
|------|------|
| `timer.h` | DUT — peripheral model |
| `register.h` | Generic 32-bit register storage |
| `timer_register.h` | Typed register helpers (optional) |
| `timer_tb.cpp` | SystemC testbench |
| `qemu_soc/wrapper/tlm_pin_bridge.h` | TLM → pin adapter |
| `qemu_soc/firmware/timer_regs.h` | Firmware register definitions |

---

*Use this document to practice explaining **why** the model is shaped this way — interviewers at senior level care more about scheduling, interface contracts, and integration trade-offs than memorizing `SC_METHOD` syntax.*
