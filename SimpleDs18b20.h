/*
The MIT License (MIT)

Copyright (c) 2013 Mathias Munk Hansen

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*SimpleDs18b20.h - Library for 1 pin connect 1 DS18B20.
  Created by Billy Cheng,November 20, 2017.
  Released into the public domain
*/

#ifndef SimpleDs18b20_h
#define SimpleDs18b20_h

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

// Error Codes異常值
#define DEVICE_DISCONNECTED_C -127  //當DS18B20異常時，會顯現-127的數值

class SimpleDs18b20 {
public:
	SimpleDs18b20(uint8_t DQ_PIN);

	float GetTemperature(void);

private:
	float CaculateTemperature(void);

	uint8_t isFoundDataWarehouse(void);

	void ShelveData(void);

	uint8_t CheckSratchpadCRC(void);

	uint8_t ReCalculateCRC(uint8_t old_CRC,uint8_t input_byte);

	uint8_t ReadByte(void);

	uint8_t ReadSlot(void);

	void SendCommand(uint8_t instruction);

	void WriteSolt(uint8_t order_bit);

	uint8_t isConnected(void);

	uint8_t TestConnect(void);

	void TxReset(void);

	uint8_t RxResult(void);

	void ThroughRx(void);

	uint8_t _g_dq_pin;

	uint8_t scratchpad[9];

};
#endif