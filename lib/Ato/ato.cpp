#include "ato.h"

int Ato::ato(){
    int retVal = 0
    int pin = digitalRead(Normal_LEVEL_PIN);
    if(pin == 1){
        digitalWrite(RO_MOTOR_PIN,HIGH);
    }else{
        digitalWrite(RO_MOTOR_PIN,LOW);
    }
    return retVal;
}