/*
 * Utils.cpp
 *
 *  Created on: 9 Dec 2023
 *      Author: Eyal
 */

#include "Utils.h"

//#include "SerialPort.h"
//#include "../typedef.h"
#include <cstdarg>

#include <iostream>
#include <sstream>
#include <stdarg.h> // Include for va_list, va_start, va_arg, va_end
#include <stdio.h>




Utils::Utils() {
	// TODO Auto-generated constructor stub

}

Utils::~Utils() {
	// TODO Auto-generated destructor stub
}


float Utils::bytes2Float(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0)
{
    FloatBytes fb;
    fb.bytes[3] = b3;  // 0x40;
    fb.bytes[2] = b2;  // 0x49;
    fb.bytes[1] = b1;  // 0x0F;
    fb.bytes[0] = b0;  // 0x00;
    float value1 = fb.f;  // value will be 3.14159
    return value1;
}
unsigned char Utils::getHighByte(int input) {
    return (input >> 8) & 0xFF;
}

unsigned char Utils::getLowByte(int input) {
    return input & 0xFF;
}

/* Returns a string representation of _length_ bytes beginning at
   _array_.  The caller has responsibility to release the string,
   using _free()_.  If the string could not be allocated, returns a
   null pointer instead. */
char* Utils::to_hex_string(const unsigned char *array, size_t length)
{
    char *outstr = (char *) malloc(2*length + 1);
    if (!outstr) return outstr;

    char *p = outstr;
    for (size_t i = 0;  i < length;  ++i) {
        //p += sprintf(p, "%02hhx", array[i]);
    	p += sprintf(p, "%02x", array[i]);
    }

    return outstr;
}
/*
void Utils::printSerial(String msg, const unsigned char* array, size_t length) {
    char* outstr = Utils::to_hex_string(array, length * sizeof(unsigned char));
    _UART1_.print("processLrfStatusMsg: ");
    _UART1_.println(outstr);
}
MK*/





void Utils::DebugMsg(char* msg)
{
    Utils::DebugMsg(msg, true);
}





string Utils::ConcatString(int count, ...)
{
    // va_list found in <cstdarg> and 
    // list is its type, used to 
    // iterate on ellipsis 
    va_list list;

    // Initialize position of va_list 
    va_start(list, count);

    string str = "";

    // Iterate through every argument 
    for (int i = 0; i < count; i++) {
       str = str + static_cast<string>(va_arg(list, const char*));
    }

    // Ends the use of va_list 
    va_end(list);

    // Return the average 
    return str;
}

bool Utils::isEqual(float f1, float f2) {
    return (abs(f1 - f2) < 0.001);
}

unsigned char Utils::calcCRC(unsigned char* data, unsigned int size)
{
		unsigned char byXorSum;
		byXorSum = 0;
		for (uint8_t i = 0; i < size; ++i)
		{
			byXorSum = byXorSum ^ data[i];
		}
		return byXorSum;
}

bool Utils::cmpStr( char* str1,  char* str2, int size){

	int i =0;
	while (i < size && str1[i]==str2[i]){
		i++;
	}
	if (i==size)
		return true;
	else
		return false;

}

//#ifdef PC_ENV
//static void Utils::print(void* _, char * str)
//{
//	cout << "LRF_STATUS-Start" << endl;
//}
//#else
//static void Utils::print(Serial_ serial, char* str)
//{
//	serial.print(str);
//}
//#endif



