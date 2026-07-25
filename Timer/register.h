#ifndef  REGISTER_H
#define REGISTER_H
#include <iostream>

class Register32{
    public:
        Register32()
        {
            value=0x0;
        }
        void write(uint32_t reg_val)
        {
            value = reg_val;
        }
        uint32_t read()
        {
            return value;
        }
        void reset()
        {
            value = 0x0;
        }
    private:
        uint32_t value;
};
#endif   REGISTER_H