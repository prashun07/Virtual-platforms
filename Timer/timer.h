#ifndef TIMER_H
#define TIMER_H

#include <systemc.h>
#include <iomanip>
#include "register.h"

static constexpr uint32_t ENABLE_BIT = 0;
static constexpr uint32_t CMP_BIT = 1;
static constexpr uint32_t OV_BIT = 2;
static constexpr uint32_t INTR_OV_BIT = 2;
static constexpr uint32_t INTR_CMP_BIT = 1;



SC_MODULE(Timer) {
    // Ports
    sc_in<bool>     clock;
    sc_in<bool>        reset;
    sc_in<bool>        read_en;
    sc_in<bool>        write_en;
    sc_in<sc_uint<32>> data_in;
    sc_in<sc_uint<32>>address;
    sc_out<sc_uint<32>>data_out;
    sc_out<bool>       intr1;
    sc_out<bool>       intr2;


    //Register
    Register32 timer_cntrl;
    Register32 timer_intr;
    // Internal state
    uint32_t timer_val;
    uint32_t timer_cmp;

    SC_CTOR(Timer) : clock("clock") {
        SC_THREAD(timer_thread);
        sensitive << clock.pos();
        SC_THREAD(bus_thread);
        sensitive << read_en<<write_en;
        SC_METHOD(reset_method)
        sensitive << reset;
    }

    void reset_method()
    {
        if(reset.read())
        {
            timer_cntrl.reset();
            timer_val=0x0;
            timer_cmp=0x0;
            timer_intr.reset();
        }
    }

    void timer_thread()
    {
        while(true)
        {
            std::cout<<"Timer thread running..."<<endl;
            wait();
            uint32_t timer_cntrl_val = timer_cntrl.read();
            if((timer_cntrl_val & (1<<ENABLE_BIT)))
            {
                timer_val++;
                uint32_t timer_intr_val= timer_intr.read();
                if( timer_cntrl_val & (1 << CMP_BIT) && timer_val == timer_cmp)
                {
                    timer_intr_val |= (1 << INTR_CMP_BIT);
                }
                if(timer_cntrl_val & (1 << OV_BIT) && timer_val == 0xFF )
                {
                    wait(SC_ZERO_TIME); //wait for one delta cycle
                    timer_val=0x0;
                    timer_intr_val |= (1 << INTR_OV_BIT);
                }
                timer_intr.write(timer_intr_val);
            }
        }
    }
    void bus_thread()
    {
        while(true)
        {
            wait();
            uint32_t offset = address.read();
            std::cout<<"Address offset received is: "<<offset<<endl;
            if(read_en.read())
            {
                switch (offset)
                {
                    case 0x0: data_out.write(timer_cntrl.read()); break;
                    case 0x4: data_out.write(timer_val); break;
                    case 0x8: data_out.write(timer_cmp);break;
                    case 0xC: data_out.write(timer_intr.read());break;
                }
            }
            else if(write_en.read())
            {
                switch (offset)
                {
                    case 0x0: timer_cntrl.write(data_in.read());break;
                    case 0x4: timer_val=data_in.read();break;
                    case 0x8: timer_cmp=data_in.read();break;
                    case 0xC: timer_intr.write(data_in.read());break;
                }
            }   
        }
    }
};

#endif