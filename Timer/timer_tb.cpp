#ifndef TIMER_TB_H
#define TIMER_TB_H

#include <systemc>
#include <iostream>
#include "timer.h"

using namespace std;

SC_MODULE(Timer_tb)
{
    //-----------------------------------
    // Signals
    //-----------------------------------

    sc_clock clk;

    sc_signal<bool> reset;
    sc_signal<bool> read_en;
    sc_signal<bool> write_en;

    sc_signal<sc_uint<32>> addr;
    sc_signal<sc_uint<32>> data_in;
    sc_signal<sc_uint<32>> data_out;

    sc_signal<bool> intr1;
    sc_signal<bool> intr2;

    //-----------------------------------
    // DUT
    //-----------------------------------

    Timer dut{"Timer"};

    SC_CTOR(Timer_tb)
    : clk("clk", 10, SC_NS, 0.2, 10, SC_NS, false)
    {
        dut.clock(clk);
        dut.reset(reset);
        dut.read_en(read_en);
        dut.write_en(write_en);
        dut.address(addr);
        dut.data_in(data_in);
        dut.data_out(data_out);
        dut.intr1(intr1);
        dut.intr2(intr2);

        SC_THREAD(run);

        SC_METHOD(monitor);
        sensitive << intr1 << intr2;
        dont_initialize();
    }

    //-------------------------------------------------
    // Helper functions
    //-------------------------------------------------

    void write_reg(uint32_t address,uint32_t value)
    {
        addr.write(address);
        data_in.write(value);

        write_en.write(true);
        wait(1,SC_NS);
        write_en.write(false);

        cout<<"[WRITE] Addr=0x"
            <<hex<<address
            <<" Data=0x"<<value
            <<" @"<<sc_time_stamp()
            <<endl;
    }

    uint32_t read_reg(uint32_t address)
    {
        addr.write(address);

        read_en.write(true);
        wait(1,SC_NS);

        uint32_t value=data_out.read();

        read_en.write(false);

        cout<<"[READ ] Addr=0x"
            <<hex<<address
            <<" Data=0x"<<value
            <<" @"<<sc_time_stamp()
            <<endl;

        return value;
    }

    //-------------------------------------------------
    // Interrupt monitor
    //-------------------------------------------------

    void monitor()
    {
        cout<<"[MONITOR] "
            <<sc_time_stamp()
            <<" intr1="
            <<intr1.read()
            <<" intr2="
            <<intr2.read()
            <<endl;
    }

    //-------------------------------------------------
    // Main Test
    //-------------------------------------------------

    void run()
    {
        cout<<"\n=====================================\n";
        cout<<" TIMER VERIFICATION STARTED\n";
        cout<<"=====================================\n";

        //-----------------------------------
        // Reset
        //-----------------------------------

        reset.write(true);
        wait(2,SC_NS);

        reset.write(false);

        cout<<"Reset Completed\n";

        //-----------------------------------
        // Test1 Register Reset
        //-----------------------------------

        cout<<"\nTEST1 : Register Reset\n";

        read_reg(0x0);
        read_reg(0x4);
        read_reg(0x8);
        read_reg(0xC);

        //-----------------------------------
        // Test2 Write/Read Control Register
        //-----------------------------------

        cout<<"\nTEST2 : Control Register\n";

        write_reg(0x0,1);

        read_reg(0x0);

        //-----------------------------------
        // Test3 Compare Register
        //-----------------------------------

        cout<<"\nTEST3 : Compare Register\n";

        write_reg(0x8,20);

        read_reg(0x8);

        //-----------------------------------
        // Test4 Timer Count
        //-----------------------------------

        cout<<"\nTEST4 : Timer Counting\n";

        wait(5,SC_NS);

        read_reg(0x4);

        //-----------------------------------
        // Test5 Compare Interrupt
        //-----------------------------------

        cout<<"\nTEST5 : Compare Interrupt\n";

        write_reg(0x8,10);

        write_reg(0x0,1);

        wait(10,SC_NS);

        read_reg(0xC);

        //-----------------------------------
        // Test6 Overflow
        //-----------------------------------

        cout<<"\nTEST6 : Overflow\n";

        write_reg(0x4,250);

        wait(10,SC_NS);

        read_reg(0x4);

        read_reg(0xC);

        //-----------------------------------
        // Test7 Disable Timer
        //-----------------------------------

        cout<<"\nTEST7 : Disable Timer\n";

        write_reg(0x0,0);

        uint32_t before=read_reg(0x4);

        wait(1,SC_NS);

        uint32_t after=read_reg(0x4);

        if(before==after)
            cout<<"PASS : Timer Stopped\n";
        else
            cout<<"FAIL : Timer Still Running\n";

        //-----------------------------------
        // Test8 Multiple Writes
        //-----------------------------------

        cout<<"\nTEST8 : Consecutive Register Writes\n";

        for(int i=0;i<5;i++)
        {
            write_reg(0x8,i*10);
            read_reg(0x8);
        }

        //-----------------------------------
        // Finish
        //-----------------------------------

        cout<<"\n=====================================\n";
        cout<<" ALL TESTS COMPLETED\n";
        cout<<"=====================================\n";

        sc_stop();
    }

};
#endif  // TIMER_TB_H

int sc_main(int argc, char* argv[])
{
    Timer_tb tb("timer_tb");

    std::cout << "=====================================\n";
    std::cout << "Starting Timer Verification\n";
    std::cout << "=====================================\n";

    sc_start();

    std::cout << "\nSimulation completed at "
              << sc_time_stamp()
              << std::endl;

    return 0;
}
