#include "Diagnostics.h"
void Diagnostics::begin(bool,const String&){}
void Diagnostics::log(const char* level,const String& message){Serial.printf("[%s] %s\n",level,message.c_str());}
