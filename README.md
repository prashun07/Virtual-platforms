# ⏱️ SystemC Memory-Mapped Timer Peripheral Model

A reusable **SystemC/C++ timer peripheral** that models the behavior of a programmable hardware timer commonly found in modern SoCs. The project demonstrates how embedded peripherals are designed, modeled, and integrated into a virtual platform for pre-silicon software development and architectural exploration.

Rather than implementing just an 8-bit counter, this project models the **complete lifecycle of a hardware peripheral** including register abstraction, memory-mapped I/O, interrupt generation, reset behavior, clock-driven execution, and software interaction.

---

# Why This Project?

Almost every modern SoC contains multiple hardware timers.

They are used by:

* Operating Systems (Linux, RTOS)
* Bootloaders
* Task Scheduling
* Delay Generation
* Timeout Detection
* Watchdog Timers
* Communication Protocols
* PWM Controllers
* Input Capture Modules
* Real-Time Applications

Before RTL becomes available, software teams still need to develop firmware, drivers, bootloaders, and operating systems. Virtual platform models built using **SystemC/TLM** enable software development months before silicon is fabricated.

This project demonstrates the development of one such reusable peripheral model.

---

# Real World Use Cases

A programmable timer is one of the most fundamental peripherals inside an SoC.

Typical applications include:

### Operating System Tick Timer

Generates periodic interrupts used by Linux or an RTOS scheduler.

```
CPU
 │
 │ 1 ms Tick
 ▼
Timer
 │
 ▼
Interrupt
 │
 ▼
Scheduler
```

---

### Timeout Detection

Used for communication protocols.

Examples:

* UART timeout
* Ethernet timeout
* SPI timeout
* I2C timeout

---

### Delay Generation

Instead of software busy loops:

```
Configure Timer

↓

Start Timer

↓

Wait for Interrupt

↓

Continue Execution
```

---

### Performance Measurement

Measure execution latency.

Example:

```
Start Timer

↓

Execute Function

↓

Read Timer

↓

Execution Cycles
```

---

### Event Scheduling

Many peripherals trigger future events using programmable compare values.

Examples:

* DMA
* PWM
* Audio
* Display Controllers

---

# Features

✔ Memory-mapped register interface

✔ Configurable timer enable

✔ Compare match functionality

✔ Overflow detection

✔ Interrupt generation

✔ Register abstraction

✔ Clock-driven execution

✔ Reset handling

✔ Read/Write register access

✔ Modular SystemC implementation

✔ Easily extendable architecture

---

# Timer in a Virtual SoC

```text
                    +----------------------+
                    |        CPU           |
                    +----------+-----------+
                               |
                               |
                     Memory-Mapped Bus
                               |
        ------------------------------------------------
        |         |         |         |                |
      UART      Timer      GPIO      SPI            Ethernet
        |         |         |         |                |
        ------------------------------------------------
                               |
                     Interrupt Controller
                               |
                              CPU
```

In a virtual platform, software running on a virtual CPU accesses peripherals exactly as it would on real silicon.

The timer behaves like an actual hardware IP, allowing firmware developers to validate software before RTL or FPGA prototypes are available.

---

# Architecture of the Timer Model

```text
                   Timer Peripheral
        +------------------------------------+
        |                                    |
        |  Bus Interface                     |
        |                                    |
        |  Address Decoder                   |
        |                                    |
        |  Register Bank                     |
        |   ├── Control Register             |
        |   ├── Timer Value Register         |
        |   ├── Compare Register             |
        |   └── Interrupt Register           |
        |                                    |
        |  Timer Engine                      |
        |                                    |
        |  Compare Logic                     |
        |                                    |
        |  Overflow Detection                |
        |                                    |
        |  Interrupt Generation              |
        +------------------------------------+
```

Each block is designed as an independent functional unit, making the design easier to extend and maintain.

---

# Software Interaction Flow

```text
Software

↓

Write Control Register

↓

Enable Timer

↓

Counter Starts Incrementing

↓

Compare Match / Overflow

↓

Interrupt Generated

↓

Software Reads Status Register

↓

Interrupt Cleared
```

This closely resembles the behavior of timers found in ARM-based SoCs.

---

# Register Map

| Offset | Register             | Description                                 |
| ------ | -------------------- | ------------------------------------------- |
| 0x00   | Control Register     | Enables timer and configures timer behavior |
| 0x04   | Timer Value Register | Current timer count                         |
| 0x08   | Compare Register     | Compare threshold                           |
| 0x0C   | Interrupt Register   | Compare and overflow interrupt status       |

---

# Register Description

## Control Register

Responsible for configuring timer operation.

Example fields:

| Bit | Description               |
| --- | ------------------------- |
| 0   | Timer Enable              |
| 1   | Compare Enable            |
| 2   | Overflow Interrupt Enable |

---

## Timer Value Register

Stores the current counter value.

The counter increments every clock cycle while enabled.

---

## Compare Register

Defines the value at which the timer generates a compare interrupt.

---

## Interrupt Register

Stores interrupt status.

Current implementation supports:

* Compare Interrupt
* Overflow Interrupt

---

# Internal Execution Flow

```text
Clock Edge

↓

Check Timer Enable

↓

Increment Counter

↓

Compare Counter

↓

Generate Compare Interrupt

↓

Check Overflow

↓

Generate Overflow Interrupt
```

---

# Memory-Mapped Communication

Software interacts with the timer using memory-mapped I/O.

```
CPU Write

Address = BASE + 0x00

↓

Address Decoder

↓

Control Register

↓

Timer Enabled
```

Similarly,

```
CPU Read

Address = BASE + 0x04

↓

Timer Register

↓

Current Counter Value
```

This mirrors how embedded software communicates with peripherals in real hardware.

---

# Design Philosophy

The model is intentionally separated into multiple logical components.

* Bus interface
* Register abstraction
* Timer engine
* Interrupt logic
* Reset logic

This separation allows each component to evolve independently while keeping interfaces clean and maintainable.

---

# Current Implementation

The current implementation models:

* Cycle-driven timer behavior
* Memory-mapped register accesses
* Register abstraction
* Compare logic
* Overflow detection
* Interrupt status generation
* Reset handling
* Software configurable registers

Although simplified, the design follows the same architectural principles used in commercial virtual platform development.

---

# Current Limitations

The objective of this project is to demonstrate peripheral modeling concepts rather than implement a production-ready timer IP.

Current limitations include:

* Simple custom bus interface instead of APB/AHB/AXI-Lite
* Signal-level communication only
* Single timer instance
* Fixed timer width
* No prescaler support
* No auto-reload mode
* No capture/compare channels
* No PWM generation
* No interrupt masking
* No interrupt controller integration
* No DMA triggering
* No TLM-2.0 sockets
* No timing annotation
* Limited error handling
* Simplified register permissions

These limitations were intentionally kept to focus on the core concepts of peripheral modeling.

---

# Future Roadmap

This project is designed to evolve into a more complete virtual peripheral.

Planned enhancements include:

## Bus Interfaces

* APB Slave
* AHB-Lite Slave
* AXI-Lite Slave

---

## TLM-2.0 Support

* Target Socket
* Generic Payload
* DMI Support
* Timing Annotation
* Loosely Timed Modeling
* Approximately Timed Modeling

---

## Advanced Timer Features

* 16/32/64-bit Timer
* Multiple Timer Channels
* Auto Reload
* Prescaler
* Input Capture
* Output Compare
* PWM Generator
* Watchdog Mode
* One-Shot Mode
* Periodic Timer Mode

---

## Interrupt Enhancements

* Interrupt Mask Register
* Interrupt Clear Register
* Pending Register
* Interrupt Priorities
* Interrupt Controller Integration

---

## Virtual Platform Integration

* ARM Virtual Platform
* QEMU Integration
* Linux Driver Validation
* Bare-metal Firmware Development
* Device Tree Support
* Bootloader Validation

---

## Performance Improvements

* Parameterized Register Framework
* Generic Peripheral Base Class
* Configurable Register Permissions
* Multiple Peripheral Instances
* Better Debug Logging
* Configuration through JSON/YAML

---

# Interview Discussion Points

This project provides an excellent platform to discuss several important topics commonly asked in SystemC, Embedded Software, and Virtual Platform interviews.

Possible discussion topics include:

* Memory-Mapped I/O
* Peripheral Modeling
* Register Abstraction
* Clock-Driven Design
* Interrupt Generation
* Compare Logic
* Overflow Detection
* Hardware/Software Interaction
* Virtual Platform Architecture
* SoC Peripheral Integration
* Address Decoding
* Embedded Firmware Interaction
* Pre-Silicon Software Development
* Event-Driven Simulation
* Modular SystemC Design
* Reusable IP Modeling

Each of these topics naturally extends into deeper discussions about SystemC, TLM, embedded software, and SoC architecture, making this project a strong interview talking point for roles involving virtual platforms, architectural modeling, and embedded systems development.
