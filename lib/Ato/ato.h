#ifndef ATO_H_
#define ATO_H_

#include "Arduino.h"

class Ato{
    public:
        int ato();
    private:
        bool firstTime = true;
        unsigned long startTime = 0;
        

};

#endif