/*
****************************************************************************
* Copyright (c) 2015 Arduino srl. All right reserved.
*
* File : ArduinoWiFi.cpp
* Date : 2016/03/24
* Author: adriano[at]arduino[dot]org - rev. 0.0.1
*
* Modified: 02 Feb 2017 - sergio@arduino.org - rev. 0.0.3
*
* Revision: 0.0.3
*
****************************************************************************
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/


#include <UnoWiFiDevEd.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(__AVR_ATmega328P__)
#define TOPIC_NUM  3		//number of topic

WifiData espSerial;
ESP esp(&espSerial);
REST rest(&esp);
MQTT mqtt(&esp);
//mqtt
boolean wifiConnected = false;
boolean attached = false;
String mqtt_topic =""; //topic event
String mqtt_data=""; // data event
String mqtt_data_tmp=""; //temporary data event

char* topic_array[TOPIC_NUM]; //subscribed topic
int topic_number=0; //number of subscribed topics
//rest
char response[64] = "";
char cstr[8] = "";

void wifiCb(void* response)
{
	uint32_t status;
	RESPONSE res(response);

	if(res.getArgc() == 1) {
		res.popArgs((uint8_t*)&status, 4);
		if(status == STATION_GOT_IP){
			espSerial.println("DBG: Internet Connected");
			wifiConnected = true;
		}
		else {
			wifiConnected = false;
		}
	}
}

void mqttData(void* response)
{
  RESPONSE res(response);
	mqtt_topic = res.popString();
	mqtt_data_tmp = res.popString();
}

void ArduinoWifiClass::connect(char* ssid,char* pwd){
	esp.wifiConnect(ssid, pwd);
}
boolean ArduinoWifiClass::connected(){
	return wifiConnected;
}

void WifiBegin(short isCiao) {
	espSerial.begin(9600);
	if(espSerial.ping()!=1) {
		espSerial.println("DBG: esp not found");
		while(1);
	}
	else {
		//espSerial.println("device found");
	}
	//put GPIO control here !!!
	esp.enable();
	delay(1000);
	esp.reset();
	delay(1000);
	while(!esp.ready());
	if (isCiao == 0)
		esp.wifiCb.attach(&wifiCb);

	espSerial.println("\nDBG: UnoWiFi Start");
}
void ArduinoWifiClass::begin() {
	WifiBegin(0);
}

void check_topic(const char* topic){
	if(!attached){
		mqtt.dataCb.attach(&mqttData);			//attach mqttdata callback
		attached = true;
	}
	if(topic_number < TOPIC_NUM){
		if(topic_number==0){
			mqtt.subscribe(topic);
			topic_number++;
		}
		else{
			int check=1;
			for(int idx=0; idx < topic_number; idx++)
				check = strcmp(topic,topic_array[idx]);
			if(check){
				mqtt.subscribe(topic);
				topic_number++;
			}
		}
	}

}

void CiaoClass::begin() {

	WifiBegin(1);
	if(!mqtt.begin("", "", "", 120, 1)) {
	 while(1);
	}
	rest.begin("google.com");
	rest.get("/");
	delay(3000);
}
CiaoData responseREAD(){
	CiaoData ciao_data;
	delay(400);
	memset(response, 0, 64);
	memset(cstr, 0, 8);

	int ret = rest.getResponse(response, 64);
	//espSerial.println(ret);

	snprintf(cstr,8,"%d",ret);

		ciao_data.msg_split[0]="id";
		ciao_data.msg_split[1]=cstr;
		ciao_data.msg_split[2]=response;

	return ciao_data;
}
CiaoData requestPOST(const char* hostname, uint16_t port, String data){
	rest.begin(hostname, port, false);
	//delay(1000);	fix
	//esp.process();
	rest.post((const char*) data.c_str(),0);
	return responseREAD();
}
CiaoData requestGET(const char* hostname, uint16_t port, String data){

	rest.begin(hostname, port, false);
	//delay(1000);   fix
	//esp.process();
	rest.get((const char*) data.c_str());
	return responseREAD();
}
CiaoData PassThroughRead(const char* connector, const char* hostname, uint16_t port, String data, const char* method){


	//short mode = 0;

	if (!strcmp(connector, "rest")){
		//mode = 0;
		if (!strcmp(method, "GET")){
			return requestGET(hostname, port, data);
		}
		else if (!strcmp(method, "POST")){
			return requestPOST(hostname, port, data);
		}
		else{
			CiaoData ciao_data;
			ciao_data.msg_split[0]="";
			ciao_data.msg_split[1]="";
			ciao_data.msg_split[2]="Method Error";
			return ciao_data;
		}
	}
	else if (!strcmp(connector, "mqtt")){
		//mode = 1;
		CiaoData ciao_data;
		esp.process();
		check_topic(hostname);
		char topic[mqtt_topic.length()+1];
		mqtt_topic.toCharArray(topic,mqtt_topic.length()+1);
		if(mqtt_data_tmp != "" && !strcmp(hostname,topic)){
				mqtt_data = mqtt_data_tmp;
				ciao_data.msg_split[0]="mqtt";
				ciao_data.msg_split[1]= hostname;
				ciao_data.msg_split[2]= mqtt_data.c_str();
				mqtt_topic="";
				mqtt_data_tmp="";
		 }
		else{
				ciao_data.msg_split[0]="mqtt";
				ciao_data.msg_split[1]=hostname;
				ciao_data.msg_split[2]="";
		}
		return ciao_data;
	}
	else {
		CiaoData ciao_data;
		ciao_data.msg_split[0]="";
		ciao_data.msg_split[1]="";
		ciao_data.msg_split[2]="Protocol Error";
		return ciao_data;
	}
}

CiaoData PassThroughWrite(const char* connector, const char* hostname, uint16_t port, String data, const char* method){

	//short mode = 0;
	if (!strcmp(connector, "rest")){
		//mode = 0;
		if (!strcmp(method, "GET")){
			return requestGET(hostname, port, data);
		}
		else if (!strcmp(method, "POST")){
			return requestPOST(hostname, port, data);
		}
		else{
			CiaoData ciao_data;
			ciao_data.msg_split[0]="";
			ciao_data.msg_split[1]="";
			ciao_data.msg_split[2]="Method Error";
			return ciao_data;
		}
	}
	else if (!strcmp(connector, "mqtt")){
		//mode = 1;
		mqtt.publish(hostname, (char*) data.c_str());
		CiaoData ciao_data;
		ciao_data.msg_split[0]="mqtt";
		ciao_data.msg_split[1]="OK";
		ciao_data.msg_split[2]="";
		return ciao_data;
	}
	else {
		CiaoData ciao_data;
		ciao_data.msg_split[0]="";
		ciao_data.msg_split[1]="";
		ciao_data.msg_split[2]="Protocol Error";
		return ciao_data;
	}

}

CiaoData CiaoClass::read(const char* connector, const char* hostname, const char* data, const char* method) {
	return PassThroughRead(connector, hostname, 80, data, method);
}

CiaoData CiaoClass::write(const char* connector, const char* hostname, const char* data, const char* method) {
	return PassThroughWrite(connector, hostname, 80, data, method);
}

CiaoData CiaoClass::read( const char* connector, const char* hostname, String data, String method){
	return PassThroughRead(connector, hostname, 80, data.c_str(), method.c_str());
}

CiaoData CiaoClass::write( const char* connector, const char* hostname, String data, String method){
	return PassThroughWrite(connector, hostname, 80, data.c_str(), method.c_str());
}

bool _resp = true;
void _timeCb(void* data) {
  //Serial.println("callback from timeCb");

	RESPONSE res(data);

  //Serial.println("argc: " + String(res.getArgc()));
	if(res.getArgc() == 4) {
    int32_t day, hours, minutes, seconds;

		res.popArgs((uint8_t*)&day, 4);
		res.popArgs((uint8_t*)&hours, 4);
		res.popArgs((uint8_t*)&minutes, 4);
		res.popArgs((uint8_t*)&seconds, 4);

    static int8_t hoursOn[24] = {
      1, //00:00
      0, //01:00
      0, //02:00
      0, //03:00
      0, //04:00
      0, //05:00
      0, //06:00
      0, //07:00
      0, //08:00
      0, //09:00
      0, //10:00
      0, //11:00
      0, //12:00
      0, //13:00
      0, //14:00
      0, //15:00
      0, //16:00
      0, //17:00
      0, //18:00
      1, //19:00
      1, //20:00
      1, //21:00
      1, //22:00
      1, //23:00
    };

    int on = hoursOn[hours];
    digitalWrite(13, !on);
    //digitalWrite(12, 1);

#if 1
    Serial.print(day);
    Serial.print(' ');
    Serial.print(hours);
    Serial.print(':');
    Serial.print(minutes);
    Serial.print(':');
    Serial.print(seconds);
    Serial.print(" light is ");
    Serial.println(on);
#endif
  } else {
    Serial.println("wrong response");
  }

  _resp = true;
}

void ArduinoWifiClass::getTime() {
  timeCb.attach(&_timeCb);

  uint16_t crc = esp.request(CMD_GET_TIME, (uint32_t)&timeCb, 0, 0);
  esp.request(crc);

  _resp = false;
  uint32_t wait = millis();
  while(_resp == false && (millis() - wait < 1000))
    esp.process();
}

CiaoClass Ciao;
ArduinoWifiClass Wifi;

#endif
