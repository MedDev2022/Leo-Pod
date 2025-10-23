/*
 * Utils.h
 *
 *  Created on: 9 Dec 2023
 *      Author: Eyal
 */

#ifndef SRC_UTILS_UTILS_H_
#define SRC_UTILS_UTILS_H_

//#include "Arduino.h"
#include <cstdarg> 
#include <iostream> 

#include <sstream>
#include <stdarg.h> // Include for va_list, va_start, va_arg, va_end
using namespace std;
//#include "../system.h"

union FloatBytes {
	float f;
	uint8_t bytes[4];
};

class Utils {
public:
	Utils();
	virtual ~Utils();

	uint8_t crc8( const uint8_t aData[], uint32_t ulOffset, uint32_t ulLength, uint8_t crc = 0);

	static float bytes2Float(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0);
	static unsigned char getHighByte(int input);
	static unsigned char getLowByte(int input);
	static char* to_hex_string(const unsigned char *array, size_t length);

	static void printSerial(string msg, const unsigned char* array, size_t length);

	

	static void DebugHexMsg(char* msg, size_t size);
	static void DebugHexMsg(char* msg, size_t size, bool isNewLine);
	static void DebugMsg(char* msg);
	static void DebugMsg(char* msg, bool isNewLine);

	static string ConcatString( int count, ...);
	static bool cmpStr( char* str1,  char* str2, int size);

	static bool isEqual(float f1, float f2);


//#ifdef PC_ENV
//	static void Utils::print(void* _, char * str);
//#else
//static void print(Serial_ serial, char* str);
//#endif

	static void print(char* str)
	{

		//Serial.print(str);
	}

	static unsigned char calcCRC(unsigned char* data, unsigned int size);


};

#endif /* SRC_UTILS_UTILS_H_ */
