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

#include "SimpleDs18b20.h"

//ROM命令
#define Skip_ROM 			0xCC	//當線上只有1個DS18B20時，可省略ROM序號確認程序

//Function命令
#define Convert_T			0x44	//啟動溫度轉換命令
#define Read_Scratchpad		0xBE	//讀取暫存器值，有9個字元


SimpleDs18b20::SimpleDs18b20(uint8_t DQ_PIN) {
	_g_dq_pin = DQ_PIN;
	pinMode(_g_dq_pin, INPUT);
}
/**************************************************************************
 * [取得溫度值]
 * 如果資料讀取程序順利，將溫度值傳回，不然傳回999.99
 ************************************************************************/
float SimpleDs18b20::GetTemperature(void)
{
	//狀況1：連線失敗，回傳異常值
	if(!isConnected()) {return DEVICE_DISCONNECTED_C;}
	//狀況2：傳回值CRC檢測異常，回傳異常值
	if(!isFoundDataWarehouse()) {return DEVICE_DISCONNECTED_C;}	//讀取量測資料並建立基礎數據倉庫成功
	//一切正常，傳回溫度值
	return CaculateTemperature(); //計算溫度並傳回溫度值
}

/**************************************************************************
 * [溫度值轉換程序]
 * 1、正負值判定：當MSB>7，就是負值（將負值指標值設置為true）
 * 2、若為負值就進行補數運算：反相（~）後，再加1
 * 3、結合MSB及LSB成一個十六位元，並算出其整數
 * 4、乘上單位比例值0.0625
 * 5、若為負數時，加負數符號
 * 6、將溫度值傳回
 ************************************************************************/
float SimpleDs18b20::CaculateTemperature(void)
{
	//前兩個字元就是溫度的訊息
	uint8_t temp_LSB = scratchpad[0];	  //第一個讀到的是低位
	uint8_t temp_MSB = scratchpad[1];      //第二個讀到的是高位
	boolean  fp_minus = false;	  	    //温度正負標誌：預設為false，因為通常為零度以上
	if(temp_MSB > 0x7f)  //當temp_MSB>7代表此溫度為負數值時，測到的數值需要先反相再加 1
	{
		fp_minus = true;					//溫度正負指標，負數時fg_Minus=0
		temp_MSB = ~temp_MSB;			 	//將temp_MSB中的每一位元反相（0、1互換）
		temp_LSB = ~temp_LSB + 1;
		//將temp_LSB中的每一位元反相（0、1互換），要記得加一，才能正確的反應其值
	}
	//以十六位元空箱來結合MSB及LSB
	uint16_t raw_temp =
	    (((uint16_t)temp_MSB) << 8) |     	//將MSB先用空的十六位元左移8個位元，等於乘256
	    (((uint16_t)temp_LSB));

	float fp_temp = raw_temp * 0.0625;
	//將十六位元的整數值再乘於0.0625的單位值，既得溫度值

	if(fp_minus) {fp_temp = -fp_temp;}	//當fp_minus是1，代表是負數，將溫度加上負號
	return fp_temp;
}

/**************************************************************************
* [暫存器資料建倉作業]
* 1.重啟、skip ROM,啟動溫度轉換
* 2.等待轉換時間（使用讀忙之檢測法）
* 3.重啟、skip ROM,讀取暫存器命令
* 4.讀取暫存器資料並上架（放入變數陣列中）
* 5.並作CRC檢驗，當OK時，就傳回true
**************************************************************************/
uint8_t SimpleDs18b20::isFoundDataWarehouse(void)
{
	//step:01
	if(!isConnected()) {return 0;} //異常狀態回報
	SendCommand(Skip_ROM);		//主機下0xcc命令（1對1的省略ROM確認作業）
	SendCommand(Convert_T);		//0x44啟動溫度轉換

	//step:02
	//讀忙控制：當DS18B20於轉換溫度資料時，若主機對它作讀取slot的動作，
	//它將會持續於低電位(0)，直到它轉換完畢後，就能得到1
	while(! ReadSlot());

	//step:03
	if(!isConnected()) {return 0;} //異常狀態回報
	SendCommand(Skip_ROM);		      //主機下0xcc命令（1對1的省略ROM確認作業）
	SendCommand(Read_Scratchpad);	  //0xBE溫度暫存器中的訊息（共9個字元）

	//step:04
	ShelveData();			         //將暫存器資料放上資料架上，供其他程式使用

	//step:05
	return CheckSratchpadCRC();		 //檢查數據資料是否正確，以作為此次量測成功與否
}

void SimpleDs18b20::ShelveData(void)
{
	for(int i = 0; i < 9; ++i)
	{
		noInterrupts();
		scratchpad[i]=ReadByte();
		interrupts();
	}
}

/**************************************************************************
* [CRC確認作業]
* 1.將暫存器的資料0-7依CRC8 MAXIM之規則計算一次
* 2.計算值與暫存器之CRC值比較，若相同則傳回 1，否則傳回 0
**************************************************************************/
uint8_t SimpleDs18b20::CheckSratchpadCRC(void)
{
	uint8_t chip_CRC=scratchpad[8];
	uint8_t check_CRC =0;
	for(int i = 0; i < 8; ++i)
	{
		check_CRC=ReCalculateCRC(check_CRC,scratchpad[i]);
	}
	return ((check_CRC==chip_CRC) ? 1 : 0);
}
/**************************************************************************
* [CRC計算作業] CRC-Cyclic Redundancy Check
* 此函式為Dallas使用CRC8=X8+X5+X4+1的檢測法
* 使用暫存器之值，進行CRC8之計算，並回傳計算結果
**************************************************************************/
uint8_t SimpleDs18b20::ReCalculateCRC(uint8_t old_CRC,uint8_t input_byte)
{
	uint8_t target_bit, rightmost_bit, polynomial_bit;
	for(int i = 0; i < 8; ++i)
	{
		rightmost_bit = old_CRC & 0x01; //&的運算是A&0=0,A&1=A
		target_bit = (input_byte >> i) & 0x01;
		//	polynomial_bit = (target_bit ^ rightmost_bit) & 0x01; //需要再&0x01嗎？
		polynomial_bit = target_bit ^ rightmost_bit ;
		old_CRC = old_CRC>>1;

		//若polynomial_bit為0其XOR的運算不會改變原來CRC之值
		//故只處理polynomial_bit為1的情況
		if(polynomial_bit)
		{
			//CRC MAXIM =1+X4+X5-->10001100-->0x8c
			old_CRC=old_CRC^0x8c;
		}
	}
	return old_CRC;
}

uint8_t SimpleDs18b20::ReadByte(void)
{
	uint8_t byte_in=0;
	for(uint8_t i = 0; i < 8; i++)
	{
		//此時所測到的電位，就是此位元的資料
		noInterrupts();
		if(ReadSlot()) {
			//看看此時主機線的電位狀況若為高位，就是1
			bitSet(byte_in, i); 		  //將byte_in第i個位元值，設置為1
		}
		else {
			bitClear(byte_in, i); 		//將byte_in第i個位元值，設置為0
		}
		interrupts();
	}
	return (byte_in);
}

uint8_t SimpleDs18b20::ReadSlot(void)
{
	//one wire的設定是3,10,53
	//主廠建議：6/9/55（含Step是4/8/55）亦可
	//me:調整為1/2/10
	//讀時隙（Read Time Slot）Step01：確保與上一個讀時序有1us的間隔
	noInterrupts();
	delayMicroseconds(1);
	//讀時隙（Read Time Slot）Step02：啟始信號--拉低電位
	pinMode(_g_dq_pin, OUTPUT);	      //轉為輸出，可達到高電位
	digitalWrite(_g_dq_pin, LOW);	  //將電位拉低告訴DS18B20，主機已準備好了
	//讀時隙（Read Time Slot）Step03：保持低電位最少1us
	delayMicroseconds(1);

	//讀時隙（Read Time Slot）Step04：釋放線路電位
	delayMicroseconds(2);
	pinMode(_g_dq_pin, INPUT);		  //轉為輸入狀態，同時釋放線路
	//讀時隙（Read Time Slot）Step05：等待時間再取樣，
	delayMicroseconds(9);			  //加前面的延時，於2+9<=11us時取樣為保險值
	//讀時隙（Read Time Slot）Step06：讀取slot的電位值
	uint8_t fp=digitalRead(_g_dq_pin);
	//讀時隙（Read Time Slot）Step07：延時動作達到讀時序時段全長為60us
	interrupts();
	delayMicroseconds(48);
	return fp;
}

void SimpleDs18b20::SendCommand(uint8_t instruction)
{
	for(uint8_t i = 0; i < 8; i++) {
		noInterrupts();
		WriteSolt(bitRead(instruction, i));
		interrupts();
	}
}

void SimpleDs18b20::WriteSolt(uint8_t order_bit)
{
	if(order_bit) {						//當值為1時的處理，
		noInterrupts();
		pinMode(_g_dq_pin, OUTPUT);      //先將pin腳改為輸出狀態
		digitalWrite(_g_dq_pin, LOW);    //將電位拉低，等於通知DS18B20要do something
		delayMicroseconds(10); 		    //至少要等待1us，但於15us前轉為高電位
		pinMode(_g_dq_pin, INPUT);	    //將接收轉成INPUT狀態，轉為高電位
		interrupts();
		delayMicroseconds(60);		    //加前段的延時至少等待60us過此周期
	}
	else { 							   //當寫入值為'0'時，Tx拉低電位時段60~120us
		noInterrupts();
		pinMode(_g_dq_pin, OUTPUT);	   //先轉為輸出狀態
		digitalWrite(_g_dq_pin, LOW);   //將電位輸出低電位
		delayMicroseconds(65);         //靜靜的等待DS18B20來讀取資料
		pinMode(_g_dq_pin, INPUT);      //釋放電位控制轉回輸入狀態
		interrupts();
		delayMicroseconds(5);		   //等待上拉電阻將電位復位為HIGH
	}
}
/**************************************************************************
* [重啟作業]
* 1.檢測pin腳是否連接正常
* 2.檢測DS18B20是否能正常回應
* 以上狀況，若ok則傳回 1，異常就傳回 0
**************************************************************************/
uint8_t SimpleDs18b20::isConnected(void)
{
	//Step 01：接線狀態
	if(!TestConnect()) {return 0;}
	
	//Step 02:DS18B20狀態
	TxReset();					//Tx階段：主機發送詢問脈沖
	noInterrupts();
	uint8_t f=RxResult();		//Rx階段：DS18B20之應答脈沖的結果
	interrupts();
	ThroughRx();				//渡過reset之時段延遲
	return f;
}
uint8_t SimpleDs18b20::TestConnect(void)
{
	//防未正常接腳：arduino接到DQ_pin之線路異常
	uint8_t retries = 60;
	//先拉高電位（轉為讀取狀態）
	noInterrupts();
	pinMode(_g_dq_pin, INPUT);
	interrupts();
	//先觀察線路的狀態，時間約240us每隔4秒查一次
	//若是正常線路應處於高電位（idle state）
	while(!digitalRead(_g_dq_pin))
	{
		retries--;
		if(retries == 0) {return 0;}
		delayMicroseconds(4);
	}
	return 1;

}
void SimpleDs18b20::TxReset(void)
{
	uint16_t time_keep_low = 480;
	noInterrupts();
	//Tx階段：Step 1.主機發送重置脈沖
	pinMode(_g_dq_pin, OUTPUT);
	//Tx階段：Step 2.主機拉低電位
	digitalWrite(_g_dq_pin, LOW);
	interrupts();
	//Tx階段：Step 3.主機持續於低電位
	delayMicroseconds(time_keep_low);
	//Tx階段：Step 4.主機釋放電位控制，轉為輸入狀態
	pinMode(_g_dq_pin, INPUT);
}

uint8_t SimpleDs18b20::RxResult(void)
{
	uint8_t time_wait_read=70;
	//Rx階段：Step 5.延時並讀取DS18B20回應電位值
	delayMicroseconds(time_wait_read);
	return !digitalRead(_g_dq_pin);
}

void SimpleDs18b20::ThroughRx(void)
{
	//Step 6.延時並讓其超過Rx的480us時間
	uint16_t time_through =410;
	delayMicroseconds(time_through);
}