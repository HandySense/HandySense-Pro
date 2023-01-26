#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <MCP3424.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ModbusMaster.h>
#include "RTClib.h"
#include "time.h"
#include "MCP23008.h"
#include "Adafruit_SHT31.h"
#include <BH1750.h>

#define HS_DEBUG
#define sw_1  1
#define sw_2  2
#define sw_3  3
#define sw_4  4
/* สำหรับเก็บค่าเวลาจาก Web App */
int t[20];
#define state_On_Off_relay        t[0]
#define value_monday_from_Web     t[1]
#define value_Tuesday_from_Web    t[2]
#define value_Wednesday_from_Web  t[3]
#define value_Thursday_from_Web   t[4]
#define value_Friday_from_Web     t[5]
#define value_Saturday_from_Web   t[6]
#define value_Sunday_from_Web     t[7]
#define value_hour_Open           t[8]
#define value_min_Open            t[9]
#define value_hour_Close          t[11]
#define value_min_Close           t[12]
#define OPEN 1
#define CLOSE 0
#define Open_relay(j)   digitalWrite(relay_pin[j], HIGH)
#define Close_relay(j)  digitalWrite(relay_pin[j], LOW)
/* Set led pin of mcp23008*/
#define status_wifi_error     0
#define status_ready_error    4
#define status_sht31_error    5
#define status_light_error    6
#define status_soil_error     7
/* HandySense Pro PIN */
const int SW[4]         = {36, 39, 34, 35};
const int relay_pin[4]  = {32, 33, 25, 26};
/* สถานะการรเชื่อมต่อ wifi */
#define cannotConnect   0
#define wifiConnected   1
#define serverConnected 2
#define editDeviceWifi  3
int connectWifiStatus = cannotConnect;
ModbusMaster    light, temp_rs485;
MCP23008 MCP    (0x24);
MCP3424         MCP_soil(6);  /* Declaration of MCP3424 */
RTC_DS1307 rtc;               /* ประกาศใช้ rtc */
Adafruit_SHT31 sht31 = Adafruit_SHT31(); /* ประกาศตัวแปรเรียกใช้ SHT31 */
BH1750 lightMeter;
/* ประกาศใช้เวลาบน Internet */
const char* ntpServer = "pool.ntp.org";
const char* nistTime = "time.nist.gov";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;
int hourNow,
    minuteNow,
    secondNow,
    dayNow,
    monthNow,
    yearNow,
    weekdayNow;
int curentTimer;
/* ประกาศตัวแปรสื่อสารกับ web App */
byte STX = 02, ETX = 03;
uint8_t START_PATTERN[3] = { 0, 111, 222 };
const size_t capacity = JSON_OBJECT_SIZE(7) + 320;
DynamicJsonDocument jsonDoc(capacity);
String mqtt_server,
       mqtt_Client,
       mqtt_password,
       mqtt_username,
       password,
       mqtt_port,
       ssid;
String client_old;
WiFiClient espClient; /* ประกาศใช้ WiFiClient */
PubSubClient client(espClient);
WiFiUDP ntpUDP; /* ประกาศใช้ WiFiUDP */
NTPClient timeClient(ntpUDP);
/* อ่านค่า temp และ soil sensor ทุก ๆ 2 วินาที */
const unsigned long eventInterval = 2 * 1000;
unsigned long previousTime_Temp_soil = 0, previousTime_wifi = 0;
/* ประกาศตัวแปรกำหนดการนับเวลาเริ่มต้น */
unsigned long previousTime_Update_data = 0;
/* Send to data to web เช่น 2*60*1000 ส่งทุก 2 นาที */
const unsigned long eventInterval_publishData = 5 * 60 * 1000;
/* ประกาศตัวแปรสำหรับเก็บค่าเซ็นเซอร์ */
float temp = 0, humidity = 0, luxMy = 0, soil = 0;
int temp_error = 0, hum_error  = 0, lux_error  = 0;
int soil_error = 0, soil_error_count = 0;

/* Array สำหรับทำ Movie Arg. ของค่าเซ็นเซอร์ทุก ๆ ค่า */
float ma_temp[5], ma_hum[5], ma_soil[5], ma_lux[5];
/* ตัวแปรเก็บค่าการตั้งเวลาทำงานอัตโนมัติ */
unsigned int time_open[4][7][3] = { { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } },
  { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } },
  { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } },
  { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } }
};
unsigned int time_close[4][7][3] = { { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } },
  { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } },
  { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } },
  { { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 }, { 2000, 2000, 2000 } }
};

float Max_Soil[4], Min_Soil[4];
float Max_Temp[4], Min_Temp[4];

unsigned int statusTimer_open[4] = { 1, 1, 1, 1 };
unsigned int statusTimer_close[4] = { 1, 1, 1, 1 };
unsigned int status_manual[4];

unsigned int statusSoil[4] = { 0, 0, 0, 0 };
unsigned int statusTemp[4] = { 0, 0, 0, 0 };

/* สถานะการทำงานของ Relay ด้ววยค่า Min Max */
int relayMaxsoil_status[4];
int relayMinsoil_status[4];
int relayMaxtemp_status[4];
int relayMintemp_status[4];

int RelayStatus[4];
TaskHandle_t WifiStatus, WaitSerial;
unsigned int oldTimer;
int check_sendData_status = 0;
int check_sendData_toWeb = 0;
int check_sendData_SoilMinMax = 0;
int check_sendData_tempMinMax = 0;
int init_checkTemp[4], init_checkSoil[4];
int _State = 0;

/* --------- Callback function get data from web ---------- */
void callback(String topic, byte* payload, unsigned int length) {
  //Serial.print("Message arrived [");
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message = message + (char)payload[i];
  }
  /* ------- topic timer ------- */
  if (topic.substring(0, 14) == "@private/timer") {
    timmer_setting(topic, payload, length);
    sent_dataTimer(topic, message);
  }
  /* ------- topic manual_control relay ------- */
  else if (topic.substring(0, 12) == "@private/led") {
    int relay_ch = topic.substring(topic.length() - 1).toInt();
    status_manual[relay_ch] = 0;
    ControlRelay_Bymanual(relay_ch, message);
  }
  /* ------- topic Soil min max ------- */
  else if (topic.substring(0, 17) == "@private/max_temp" || topic.substring(0, 17) == "@private/min_temp") {
    TempMaxMin_setting(topic, message, length);
  }
  /* ------- topic Temp min max ------- */
  else if (topic.substring(0, 17) == "@private/max_soil" || topic.substring(0, 17) == "@private/min_soil") {
    SoilMaxMin_setting(topic, message, length);
  }
}

/* ----------------------- Sent Timer --------------------------- */
void sent_dataTimer(String topic, String message) {
  String _numberTimer = topic.substring(topic.length() - 2).c_str();
  String _payload = "{\"data\":{\"value_timer";
  _payload += _numberTimer;
  _payload += "\":\"";
  _payload += message;
  _payload += "\"}}";
  Serial.print("incoming : ");
  Serial.println((char*)_payload.c_str());
  client.publish("@shadow/data/update", (char*)_payload.c_str());
}

/* --------- UpdateData_To_Server --------- */
void UpdateData_To_Server() {
  String DatatoWeb;
  char msgtoWeb[200];
  if (soil_error == 1) soil = 0.00;
  if (temp_error == 1) temp = 0.00;
  if (hum_error == 1)  humidity = 0.00;
  if (lux_error == 1)  luxMy = 0.00;
  DatatoWeb = "{\"data\": {\"temperature\":" + String(temp) +
              ",\"humidity\":" + String(humidity) + ",\"lux\":" +
              String(luxMy) + ",\"soil\":" + String(soil)  + "}}";
  Serial.print("DatatoWeb : "); Serial.println(DatatoWeb);
  DatatoWeb.toCharArray(msgtoWeb, (DatatoWeb.length() + 1));
  if (client.publish("@shadow/data/update", msgtoWeb)) {
    Serial.println(" Send Data Complete ");
  }
}

/* --------- sendStatus_RelaytoWeb --------- */
void sendStatus_RelaytoWeb() {
  String _payload;
  char msgUpdateRalay[200];
  if (check_sendData_status == 1) {
    _payload = "{\"data\": {\"led0\":\"" + String(RelayStatus[0]) + "\",\"led1\":\"" + String(RelayStatus[1]) + "\",\"led2\":\"" + String(RelayStatus[2]) + "\",\"led3\":\"" + String(RelayStatus[3]) + "\"}}";
    Serial.print("_payload : ");
    Serial.println(_payload);
    _payload.toCharArray(msgUpdateRalay, (_payload.length() + 1));
    if (client.publish("@shadow/data/update", msgUpdateRalay)) {
      check_sendData_status = 0;
      Serial.println("Send StatusRelay FULL");
    }
  }
}

/* --------- Respone soilMinMax toWeb --------- */
void send_soilMinMax() {
  String soil_payload;
  char soilMinMax_data[450];
  if (check_sendData_SoilMinMax == 1) {
    soil_payload = "{\"data\": {\"min_soil0\":" + String(Min_Soil[0]) + ",\"max_soil0\":" + String(Max_Soil[0]) + ",\"min_soil1\":" + String(Min_Soil[1]) + ",\"max_soil1\":" + String(Max_Soil[1]) + ",\"min_soil2\":" + String(Min_Soil[2]) + ",\"max_soil2\":" + String(Max_Soil[2]) + ",\"min_soil3\":" + String(Min_Soil[3]) + ",\"max_soil3\":" + String(Max_Soil[3]) + "}}";
    Serial.print("_payload : ");
    Serial.println(soil_payload);
    soil_payload.toCharArray(soilMinMax_data, (soil_payload.length() + 1));
    if (client.publish("@shadow/data/update", soilMinMax_data)) {
      check_sendData_SoilMinMax = 0;
    }
  }
}

/* --------- Respone tempMinMax toWeb --------- */
void send_tempMinMax() {
  String temp_payload;
  char tempMinMax_data[400];
  if (check_sendData_tempMinMax == 1) {
    temp_payload = "{\"data\": {\"min_temp0\":" + String(Min_Temp[0]) + ",\"max_temp0\":" + String(Max_Temp[0]) + ",\"min_temp1\":" + String(Min_Temp[1]) + ",\"max_temp1\":" + String(Max_Temp[1]) + ",\"min_temp2\":" + String(Min_Temp[2]) + ",\"max_temp2\":" + String(Max_Temp[2]) + ",\"min_temp3\":" + String(Min_Temp[3]) + ",\"max_temp3\":" + String(Max_Temp[3]) + "}}";
    Serial.print("_payload : ");
    Serial.println(temp_payload);
    temp_payload.toCharArray(tempMinMax_data, (temp_payload.length() + 1));
    if (client.publish("@shadow/data/update", tempMinMax_data)) {
      check_sendData_tempMinMax = 0;
    }
  }
}

/* ----------------------- Setting Timer --------------------------- */
void timmer_setting(String topic, byte* payload, unsigned int length) {
  int timer, relay;
  char* str;
  unsigned int count = 0;
  char message_time[50];
  timer = topic.substring(topic.length() - 1).toInt();
  relay = topic.substring(topic.length() - 2, topic.length() - 1).toInt();
  Serial.println();
  Serial.print("timeer     : ");
  Serial.println(timer);
  Serial.print("relay      : ");
  Serial.println(relay);
  for (int i = 0; i < length; i++) {
    message_time[i] = (char)payload[i];
  }
  Serial.println(message_time);
  str = strtok(message_time, " ,,,:");
  while (str != NULL) {
    t[count] = atoi(str);
    count++;
    str = strtok(NULL, " ,,,:");
  }
  if (state_On_Off_relay == 1) {
    for (int k = 0; k < 7; k++) {
      if (t[k + 1] == 1) {
        time_open[relay][k][timer] = (value_hour_Open * 60) + value_min_Open;
        time_close[relay][k][timer] = (value_hour_Close * 60) + value_min_Close;
      } else {
        time_open[relay][k][timer] = 3000;
        time_close[relay][k][timer] = 3000;
      }
      int address = ((((relay * 7 * 3) + (k * 3) + timer) * 2) * 2) + 2100;
      EEPROM.write(address, time_open[relay][k][timer] / 256);
      EEPROM.write(address + 1, time_open[relay][k][timer] % 256);
      EEPROM.write(address + 2, time_close[relay][k][timer] / 256);
      EEPROM.write(address + 3, time_close[relay][k][timer] % 256);
      EEPROM.commit();
      Serial.print("time_open  : ");
      Serial.println(time_open[relay][k][timer]);
      Serial.print("time_close : ");
      Serial.println(time_close[relay][k][timer]);
    }
  } else if (state_On_Off_relay == 0) {
    for (int k = 0; k < 7; k++) {
      time_open[relay][k][timer] = 3000;
      time_close[relay][k][timer] = 3000;
      int address = ((((relay * 7 * 3) + (k * 3) + timer) * 2) * 2) + 2100;
      EEPROM.write(address, time_open[relay][k][timer] / 256);
      EEPROM.write(address + 1, time_open[relay][k][timer] % 256);
      EEPROM.write(address + 2, time_close[relay][k][timer] / 256);
      EEPROM.write(address + 3, time_close[relay][k][timer] % 256);
      EEPROM.commit();
      Serial.print("time_open  : ");
      Serial.println(time_open[relay][k][timer]);
      Serial.print("time_close : ");
      Serial.println(time_close[relay][k][timer]);
    }
  } else {
    Serial.println("Not enabled timer, Day !!!");
  }
}

/* ------------ Control Relay By Timmer ------------- */
int get_curentTimer() {
  int cal_weekdayNow;
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, nistTime);
    if (getLocalTime(&timeinfo)) {
      yearNow = timeinfo.tm_year + 1900;
      monthNow = timeinfo.tm_mon + 1;
      dayNow = timeinfo.tm_mday;
      weekdayNow = timeinfo.tm_wday;
      hourNow = timeinfo.tm_hour;
      minuteNow = timeinfo.tm_min;
      secondNow = timeinfo.tm_sec;
      if (rtc.begin()) rtc.adjust(DateTime(yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow));
      cal_weekdayNow = weekdayNow - 1;
      if (cal_weekdayNow == -1) cal_weekdayNow = 6;
      int curentTimer = (cal_weekdayNow * 24 * 60) + (hourNow * 60) + minuteNow;

#ifdef HS_DEBUG_
      Serial.println("Time on Internet.");
      //Serial.print("CurentTimer : "); Serial.println(curentTimer);
      //Serial.print("dayofweek : "); Serial.print(curentTimer / 60 / 24);
      //Serial.println();
#endif
      return curentTimer ;
    }
  }
  if (rtc.begin()) {
    DateTime _now = rtc.now();
    cal_weekdayNow = _now.dayOfTheWeek() - 1;
    if (cal_weekdayNow == -1) cal_weekdayNow = 6;
    int curentTimer = (cal_weekdayNow * 24 * 60) + (_now.hour() * 60) + _now.minute();
    if (curentTimer >= 0 || curentTimer <= 10080) {

#ifdef HS_DEBUG_
      Serial.println("Time in RTC.");
      //Serial.print("CurentTimer : "); Serial.println(curentTimer);
      //Serial.print("dayofweek : "); Serial.print(curentTimer / 60 / 24);
      //Serial.println();
#endif
      return curentTimer ;
    }
  }
  return -1;
}

void ControlRelay_Bytimmer() {
  int _time = get_curentTimer();
  if (_time == -1) return;
  else {
    int dayofweek =  _time / 60 / 24;
    int curentTimer = _time - (dayofweek * 60 * 24);
    if (curentTimer != oldTimer) {
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
          if (time_open[i][dayofweek][j] == curentTimer) {
            RelayStatus[i] = 1;
            check_sendData_status = 1;
            Open_relay(i);
            //MCP.digitalWrite(i, HIGH);
#ifdef HS_DEBUG
            Serial.println("timer On");
            Serial.print("curentTimer : ");
            Serial.println(curentTimer);
            Serial.print("oldTimer    : ");
            Serial.println(oldTimer);
#endif
          } else if (time_close[i][dayofweek][j] == curentTimer) {
            RelayStatus[i] = 0;
            check_sendData_status = 1;
            Close_relay(i);
            //MCP.digitalWrite(i, LOW);
#ifdef HS_DEBUG
            Serial.println("timer Off");
            Serial.print("curentTimer : ");
            Serial.println(curentTimer);
            Serial.print("oldTimer    : ");
            Serial.println(oldTimer);
#endif
          } else if (time_open[i][dayofweek][j] == 2000 && time_close[i][dayofweek][j] == 2000) {
            //Close_relay(i);
            //Serial.println(" Not check day, Not Working relay");
          }
        }
      }
      oldTimer = curentTimer;
    }
  }
}

/* ----------------------- Manual Control --------------------------- */
void ControlRelay_Bymanual(int ch_relay, String message) {
  Serial.println();
  Serial.print("manual_message : ");
  Serial.println(message);
  Serial.print("ch_relay       : ");
  Serial.println(ch_relay);
  if (status_manual[ch_relay] == 0) {
    status_manual[ch_relay] = 1;
    if (message == "on") {
      Open_relay(ch_relay);
      //MCP.digitalWrite(ch_relay, HIGH);
      RelayStatus[ch_relay] = 1;
      Serial.println("ON manual");
    } else if (message == "off") {
      Close_relay(ch_relay);
      //MCP.digitalWrite(ch_relay, LOW);
      RelayStatus[ch_relay] = 0;
      Serial.println("OFF manual");
    }
    check_sendData_status = 1;
  }
}

/* ----------------------- SoilMaxMin_setting --------------------------- */
void SoilMaxMin_setting(String topic, String message, unsigned int length) {
  String soil_message = message;
  String soil_topic = topic;
  int Relay_SoilMaxMin = topic.substring(topic.length() - 1).toInt();
  if (soil_topic.substring(9, 12) == "max") {
    relayMaxsoil_status[Relay_SoilMaxMin] = topic.substring(topic.length() - 1).toInt();
    Max_Soil[Relay_SoilMaxMin] = soil_message.toInt();
    EEPROM.write(Relay_SoilMaxMin + 2000, Max_Soil[Relay_SoilMaxMin]);
    EEPROM.commit();
    check_sendData_SoilMinMax = 1;
    Serial.print("Max_Soil : ");
    Serial.println(Max_Soil[Relay_SoilMaxMin]);
  } else if (soil_topic.substring(9, 12) == "min") {
    relayMinsoil_status[Relay_SoilMaxMin] = topic.substring(topic.length() - 1).toInt();
    Min_Soil[Relay_SoilMaxMin] = soil_message.toInt();
    EEPROM.write(Relay_SoilMaxMin + 2004, Min_Soil[Relay_SoilMaxMin]);
    EEPROM.commit();
    check_sendData_SoilMinMax = 1;
    Serial.print("Min_Soil : ");
    Serial.println(Min_Soil[Relay_SoilMaxMin]);
  }
  init_checkSoil[Relay_SoilMaxMin] = 0;
}

/* ----------------------- TempMaxMin_setting --------------------------- */
void TempMaxMin_setting(String topic, String message, unsigned int length) {
  String temp_message = message;
  String temp_topic = topic;
  int Relay_TempMaxMin = topic.substring(topic.length() - 1).toInt();
  //Serial.print("Relay_TempMaxMin : "); Serial.println(Relay_TempMaxMin);

  if (temp_topic.substring(9, 12) == "max") {
    Max_Temp[Relay_TempMaxMin] = temp_message.toInt();
    EEPROM.write(Relay_TempMaxMin + 2008, Max_Temp[Relay_TempMaxMin]);
    EEPROM.commit();
    check_sendData_tempMinMax = 1;
    Serial.print("Max_Temp : ");
    Serial.println(Max_Temp[Relay_TempMaxMin]);
  } else if (temp_topic.substring(9, 12) == "min") {
    Min_Temp[Relay_TempMaxMin] = temp_message.toInt();
    EEPROM.write(Relay_TempMaxMin + 2012, Min_Temp[Relay_TempMaxMin]);
    EEPROM.commit();
    check_sendData_tempMinMax = 1;
    Serial.print("Min_Temp : ");
    Serial.println(Min_Temp[Relay_TempMaxMin]);
  }
  init_checkTemp[Relay_TempMaxMin] = 0;
}

/* ----------------------- soilMinMax_ControlRelay --------------------------- */
void ControlRelay_BysoilMinMax() {
  Get_soil();
  for (int k = 0; k < 4; k++) {
    if (Min_Soil[k] != 0 && Max_Soil[k] != 0) {
      if (Min_Soil[k] < Max_Soil[k]) {
        if (soil < Min_Soil[k]) {
          if (statusSoil[k] == 0 || init_checkSoil[k] == 0) {
            Open_relay(k);
            statusSoil[k] = 1;
            RelayStatus[k] = 1;
            check_sendData_status = 1;
            Serial.println("soil On");
            init_checkSoil[k] = 1;
          }
        } else if (soil > Max_Soil[k]) {
          if (statusSoil[k] == 1 || init_checkSoil[k] == 0) {
            Close_relay(k);
            statusSoil[k] = 0;
            RelayStatus[k] = 0;
            check_sendData_status = 1;
            Serial.println("soil Off");
            init_checkSoil[k] = 1;
          }
        }
      }
      else {
        Serial.println("Error (Max_Soil < Min_Soil) ? ");
      }
    }
  }
  if (soil_error == 1) {
    for (int c = 0; c < 4; c++) {
      if (Max_Soil[c] > 0 ) {
        Close_relay(c);
        Serial.print("relay : "); Serial.println(c);
        Serial.println("soil Off");
        statusSoil[c] = 0;
        RelayStatus[c] = 0;
        check_sendData_status = 1;
        init_checkSoil[c] = 1;
      }
    }
  }
}

/* ----------------------- tempMinMax_ControlRelay --------------------------- */
void ControlRelay_BytempMinMax() {
  Get_sht31();
  for (int g = 0; g < 4; g++) {
    if (Min_Temp[g] != 0 && Max_Temp[g] != 0) {
      if (Max_Temp[g] > Min_Temp[g]) {
        if (temp < Min_Temp[g]) {
          if (statusTemp[g] == 1 || init_checkTemp[g] == 0) {
            Close_relay(g);
            Serial.print("relay : "); Serial.println(g);
            Serial.println("temp Off");
            statusTemp[g] = 0;
            RelayStatus[g] = 0;
            check_sendData_status = 1;
            init_checkTemp[g] = 1;
          }
        } else if (temp > Max_Temp[g]) {
          if (statusTemp[g] == 0 || init_checkTemp[g] == 0) {
            Open_relay(g);
            Serial.print("relay : "); Serial.println(g);
            Serial.println("temp On");
            statusTemp[g] = 1;
            RelayStatus[g] = 1;
            check_sendData_status = 1;
            init_checkTemp[g] = 1;
          }
        }
      }
      else {
        Serial.println("Error (Max_Temp < Min_Temp) ? ");
      }
    }
  }
  if (temp_error == 1) {
    for (int b = 0; b < 4; b++) {
      if (Max_Temp[b] > 0 ) {
        Close_relay(b);
        Serial.print("relay : "); Serial.println(b);
        Serial.println("temp Off");
        statusTemp[b] = 0;
        RelayStatus[b] = 0;
        check_sendData_status = 1;
        init_checkTemp[b] = 1;
      }
    }
  }
}

/* ----------------------- Mode for calculator sensor i2c --------------------------- */
int Mode(float* getdata) {
  int maxValue = 0;
  int maxCount = 0;
  for (int i = 0; i < sizeof(getdata); ++i) {
    int count = 0;
    for (int j = 0; j < sizeof(getdata); ++j) {
      if (round(getdata[j]) == round(getdata[i]))
        ++count;
    }
    if (count > maxCount) {
      maxCount = count;
      maxValue = round(getdata[i]);
    }
  }
  return maxValue;
}

/* ----------------------- Calculator sensor SHT31  --------------------------- */
void Get_sht31() {
  if (!sht31.begin(0x44)) {
    temp_error = 1;
    MCP.digitalWrite(status_sht31_error, LOW);
  } else {
    ma_temp[4] = ma_temp[3];
    ma_temp[3] = ma_temp[2];
    ma_temp[2] = ma_temp[1];
    ma_temp[1] = ma_temp[0];
    ma_temp[0] = sht31.readTemperature();
    temp = (ma_temp[0] + ma_temp[1] + ma_temp[2] + ma_temp[3] + ma_temp[4]) / 5.0f;

    ma_hum[4] = ma_hum[3];
    ma_hum[3] = ma_hum[2];
    ma_hum[2] = ma_hum[1];
    ma_hum[1] = ma_hum[0];
    ma_hum[0] = sht31.readHumidity();
    humidity =  (ma_hum[0] + ma_hum[1] + ma_hum[2] + ma_hum[3] + ma_hum[4]) / 5.0f;
    temp_error = 0;
    MCP.digitalWrite(status_sht31_error, HIGH);
  }
  /*
    uint8_t result_temp;
    result_temp = temp_rs485.readHoldingRegisters(1, 2);
    if (result_temp == temp_rs485.ku8MBSuccess) {
    ma_temp[4] = ma_temp[3];
    ma_temp[3] = ma_temp[2];
    ma_temp[2] = ma_temp[1];
    ma_temp[1] = ma_temp[0];
    ma_temp[0] = temp_rs485.getResponseBuffer(1) / 10;
    temp = (ma_temp[0] + ma_temp[1] + ma_temp[2] + ma_temp[3] + ma_temp[4]) / 5.0f;

    ma_hum[4] = ma_hum[3];
    ma_hum[3] = ma_hum[2];
    ma_hum[2] = ma_hum[1];
    ma_hum[1] = ma_hum[0];
    ma_hum[0] = temp_rs485.getResponseBuffer(0) / 10;
    humidity =  (ma_hum[0] + ma_hum[1] + ma_hum[2] + ma_hum[3] + ma_hum[4]) / 5.0f;
    temp_error = 0;
    MCP.digitalWrite(status_sht31_error, HIGH);
    } else {
    temp_error = 1;
    MCP.digitalWrite(status_sht31_error, LOW);
    Serial.println("temp_error : " + String(temp_error));
    }
  */
}

/* ----------------------- Calculator sensor Max44009 --------------------------- */
void Get_light() {
  uint8_t result_light;
  /* TEST */
  result_light = light.readHoldingRegisters(1, 1);
  if (result_light == light.ku8MBSuccess) {
    ma_lux[4] = ma_lux[3];
    ma_lux[3] = ma_lux[2];
    ma_lux[2] = ma_lux[1];
    ma_lux[1] = ma_lux[0];
    ma_lux[0] = light.getResponseBuffer(1) / 1000.00f;
    luxMy = (ma_lux[0] + ma_lux[1] + ma_lux[2] + ma_lux[3] + ma_lux[4]) / 5.0f;
    lux_error = 0;
    MCP.digitalWrite(status_light_error, HIGH);
  } else {
    lux_error = 1;
    //Serial.println("lux_error  : " + String(lux_error));
    MCP.digitalWrite(status_light_error, LOW);
  }
  /* ETT */
  /*result_light = light.readInputRegisters(4, 1);
    if (result_light == light.ku8MBSuccess) {
    ma_lux[4] = ma_lux[3];
    ma_lux[3] = ma_lux[2];
    ma_lux[2] = ma_lux[1];
    ma_lux[1] = ma_lux[0];
    ma_lux[0] = light.getResponseBuffer(0) / 1000;
    luxMy = (ma_lux[0] + ma_lux[1] + ma_lux[2] + ma_lux[3] + ma_lux[4]) / 5.0f;
    lux_error = 0;
    MCP.digitalWrite(status_light_error, HIGH);
    } else {
    lux_error = 1;
    Serial.println("lux_error  : " + String(lux_error));
    MCP.digitalWrite(status_light_error, LOW);
    }*/
  /* I2C */
  /*float buffer_lux = 0;
    float lux_cal = 0;
    int num_lux = 0;
    if (!lightMeter.begin()) {
    lux_error = 1;
    MCP.digitalWrite(status_light_error, LOW);
    } else {
    //Serial.println(MAX44009_LIB_VERSION);
    ma_lux[4] = ma_lux[3];
    ma_lux[3] = ma_lux[2];
    ma_lux[2] = ma_lux[1];
    ma_lux[1] = ma_lux[0];
    ma_lux[0] = (lightMeter.readLightLevel() * 2.15) / 1000; //(KLux) lightMeter.readLightLevel()
    luxMy = (ma_lux[0] + ma_lux[1] + ma_lux[2] + ma_lux[3] + ma_lux[4]) / 5.0f;
    lux_error = 0;
    MCP.digitalWrite(status_light_error, HIGH);
    }*/
}

/* ----------------------- Calculator sensor Soil  --------------------------- */
void Get_soil() {
  float buffer_soil = 0, volts0;
  MCP_soil.NewConversion();
  volts0 = MCP_soil.Measure();
  Serial.println("volts0 : " + String(volts0) + " Microvolt");
  /* 1800000, 840000 Soil 5 M. */
  /* 320000, 170000 Soil 15 M. */
  buffer_soil = map(volts0, 310000, 180000, 0, 100); /*buffer_soil = (-72.92 * abs(volts0)) + 230.76;*/
  if (buffer_soil < 0 || buffer_soil > 100 || isnan(buffer_soil)) {  // range 0 to 100 %
    if (soil_error_count >= 5) {
      soil_error = 1;
      MCP.digitalWrite(status_soil_error, LOW);
      //Serial.println("soil_error : " + String(soil_error) + " Soil : " + String(soil) + " %");
    } else soil_error_count++;
  } else {
    ma_soil[4] = ma_soil[3];
    ma_soil[3] = ma_soil[2];
    ma_soil[2] = ma_soil[1];
    ma_soil[1] = ma_soil[0];
    ma_soil[0] = buffer_soil;
    soil = (ma_soil[0] + ma_soil[1] + ma_soil[2] + ma_soil[3] + ma_soil[4]) / 5.0f;
    soil_error_count = 0;
    soil_error = 0;
    MCP.digitalWrite(status_soil_error, HIGH);
  }
}

int SW_ADC() {
  if (digitalRead(SW[0]) == LOW)      return sw_1;
  else if (digitalRead(SW[1]) == LOW) return sw_2;
  else if (digitalRead(SW[2]) == LOW) return sw_3;
  else if (digitalRead(SW[3]) == LOW) return sw_4;
  return 0;
}

void printLocalTime() {
  if (!getLocalTime(&timeinfo)) return; /*Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");*/
}

/* ----------------------- Set All Config --------------------------- */
void setAll_config() {
  for (int b = 0; b < 4; b++) {
    Max_Soil[b] = EEPROM.read(b + 2000);
    Min_Soil[b] = EEPROM.read(b + 2004);
    Max_Temp[b] = EEPROM.read(b + 2008);
    Min_Temp[b] = EEPROM.read(b + 2012);
    if (Max_Soil[b] >= 255) Max_Soil[b] = 0;
    if (Min_Soil[b] >= 255) Min_Soil[b] = 0;
    if (Max_Temp[b] >= 255) Max_Temp[b] = 0;
    if (Min_Temp[b] >= 255) Min_Temp[b] = 0;
    Serial.println("Max_Soil : " + String(b) + " : " + String(Max_Soil[b]) + " Min_Soil " + String(b) + " : " +
                   String(Min_Soil[b]) + " Max_Temp " + String(b) + " : " + String(Max_Temp[b]) + " Min_Temp " +
                   String(b) + " : " + String(Min_Temp[b]));
  }
  int count_in = 0;
  for (int eeprom_relay = 0; eeprom_relay < 4; eeprom_relay++) {
    for (int eeprom_timer = 0; eeprom_timer < 3; eeprom_timer++) {
      for (int dayinweek = 0; dayinweek < 7; dayinweek++) {
        int eeprom_address = ((((eeprom_relay * 7 * 3) + (dayinweek * 3) + eeprom_timer) * 2) * 2) + 2100;
        time_open[eeprom_relay][dayinweek][eeprom_timer] = (EEPROM.read(eeprom_address) * 256) + (EEPROM.read(eeprom_address + 1));
        time_close[eeprom_relay][dayinweek][eeprom_timer] = (EEPROM.read(eeprom_address + 2) * 256) + (EEPROM.read(eeprom_address + 3));

        if (time_open[eeprom_relay][dayinweek][eeprom_timer] >= 2000) {
          time_open[eeprom_relay][dayinweek][eeprom_timer] = 3000;
        }
        if (time_close[eeprom_relay][dayinweek][eeprom_timer] >= 2000) {
          time_close[eeprom_relay][dayinweek][eeprom_timer] = 3000;
        }
        Serial.println("cout : " + String(count_in) + " time_open : " + String(time_open[eeprom_relay][dayinweek][eeprom_timer]) +
                       " time_close : " + String(time_close[eeprom_relay][dayinweek][eeprom_timer]));
        count_in++;
      }
    }
  }
}

/* ----------------------- Delete All Config --------------------------- */
void Delete_All_config() {
  for (int b = 2000; b < 4096; b++) {
    EEPROM.write(b, 255);
    EEPROM.commit();
  }
}

/* ----------------------- Add and Edit device || Edit Wifi --------------------------- */
void Edit_device_wifi() {
  connectWifiStatus = editDeviceWifi;
  EepromStream eeprom(0, 1024);
  Serial.write(START_PATTERN, 3);               // ส่งข้อมูลชนิดไบต์ ส่งตัวอักษรไปบนเว็บ
  Serial.flush();                               // เครียบัฟเฟอร์ให้ว่าง
  deserializeJson(jsonDoc, eeprom);             // คือการรับหรืออ่านข้อมูล jsonDoc ใน eeprom
  client_old = jsonDoc["client"].as<String>();  // เก็บค่า client เก่าเพื่อเช็คกับ client ใหม่ ในการ reset Wifi ไม่ให้ลบค่า Config อื่นๆ
  Serial.write(STX);                            // 02 คือเริ่มส่ง
  serializeJsonPretty(jsonDoc, Serial);         // ส่งข่อมูลของ jsonDoc ไปบนเว็บ
  Serial.write(ETX);
  delay(1000);
  Serial.write(START_PATTERN, 3);  // ส่งข้อมูลชนิดไบต์ ส่งตัวอักษรไปบนเว็บ
  Serial.flush();
  jsonDoc["server"]   = NULL;
  jsonDoc["client"]   = NULL;
  jsonDoc["pass"]     = NULL;
  jsonDoc["user"]     = NULL;
  jsonDoc["password"] = NULL;
  jsonDoc["port"]     = NULL;
  jsonDoc["ssid"]     = NULL;
  jsonDoc["command"]  = NULL;
  Serial.write(STX);                     // 02 คือเริ่มส่ง
  serializeJsonPretty(jsonDoc, Serial);  // ส่งข่อมูลของ jsonDoc ไปบนเว็บ
  Serial.write(ETX);                     // 03 คือจบ
}

/* -------- webSerialJSON function ------- */
void webSerialJSON() {
  while (Serial.available() > 0) {
    Serial.setTimeout(10000);
    EepromStream eeprom(0, 1024);
    DeserializationError err = deserializeJson(jsonDoc, Serial);
    if (err == DeserializationError::Ok) {
      String command = jsonDoc["command"].as<String>();
      bool isValidData = !jsonDoc["client"].isNull();
      if (command == "restart") {
        delay(100);
        ESP.restart();
      }
      if (isValidData) {/* ------------------WRITING----------------- */
        serializeJson(jsonDoc, eeprom);
        eeprom.flush();
        if (client_old != jsonDoc["client"].as<String>()) {/* ถ้าไม่เหมือนคือเพิ่มอุปกรณ์ใหม่ ถ้าเหมือนคือการเปลี่ยน wifi */
          Delete_All_config();
        }
        delay(100);
        ESP.restart();
      }
    } else {
      Serial.read();
    }
  }
}

/* --------- อินเตอร์รัป แสดงสถานะการเชื่อม wifi ------------- */
void Blink_LED_wifi() {
  unsigned long currentTime_ledWifi = millis();
  if (currentTime_ledWifi - previousTime_wifi >= 500) { /* blink 1 ms. */
    if (WiFi.status() != WL_CONNECTED) {
      if (_State == 0) {
        MCP.digitalWrite(status_wifi_error, HIGH);
        _State = 1;
      } else if (_State == 1) {
        MCP.digitalWrite(status_wifi_error, LOW);
        _State = 0;
      }
    }
    else MCP.digitalWrite(status_wifi_error, HIGH);
    previousTime_wifi = currentTime_ledWifi;
  }
}

void setup() {
  delay(500);
  Serial.begin(9600); /* Connect to HandySense Dashboard 115200 */
  Serial2.begin(9600);
  EEPROM.begin(4096);
  Wire.begin();
  Wire.setClock(10000);
  MCP.begin();
  MCP.pinMode8(0x00);
  MCP_soil.Configuration(4, 16, 0, 1); /* Channel 1, 16 bits resolution, one-shot mode, amplifier gain = 1 */
  if (!rtc.begin());
  //light.begin(245, Serial2); /* BH1750 ETT Slave */
  light.begin(1, Serial2); /* Modbus slave ID 1 */
  temp_rs485.begin(1, Serial2);

  pinMode(SW[0], INPUT);
  pinMode(SW[1], INPUT);
  pinMode(SW[2], INPUT);
  pinMode(SW[3], INPUT);
  pinMode(relay_pin[0], OUTPUT);
  pinMode(relay_pin[1], OUTPUT);
  pinMode(relay_pin[2], OUTPUT);
  pinMode(relay_pin[3], OUTPUT);
  digitalWrite(relay_pin[0], LOW);
  digitalWrite(relay_pin[1], LOW);
  digitalWrite(relay_pin[2], LOW);
  digitalWrite(relay_pin[3], LOW);

  for (int pin = 0; pin < 8; pin++) MCP.digitalWrite(pin, HIGH);  delay(500); /* alternating HIGH/LOW */
  for (int pin = 0; pin < 8; pin++) MCP.digitalWrite(pin, LOW);   delay(500);

  Edit_device_wifi();               /* ประกาศ Object eepromSteam ที่ Address 0 ขนาด 1024 byte */
  EepromStream eeprom(0, 1024);     /* คือการรับหรืออ่านข้อมูล jsonDoc ใน eeprom */
  deserializeJson(jsonDoc, eeprom); /* ถ้าใน jsonDoc มีค่าแล้ว */

  if (!jsonDoc.isNull()) {
    mqtt_server   = jsonDoc["server"].as<String>();
    mqtt_Client   = jsonDoc["client"].as<String>();
    mqtt_password = jsonDoc["pass"].as<String>();
    mqtt_username = jsonDoc["user"].as<String>();
    password      = jsonDoc["password"].as<String>();
    mqtt_port     = jsonDoc["port"].as<String>();
    ssid          = jsonDoc["ssid"].as<String>();
  }
  xTaskCreatePinnedToCore(TaskWifiStatus, "WifiStatus", 8000, NULL, 1, &WifiStatus, 1);
  xTaskCreatePinnedToCore(TaskWaitSerial, "WaitSerial", 6000, NULL, 1, &WaitSerial, 1);
  setAll_config();
  for (int i = 0 ; i < 4 ; i++) {
    Get_light();
    Get_soil();
    Get_sht31();
  }
  MCP.digitalWrite(status_ready_error, HIGH); /* ready led status */
  delay(500);
}

void loop() {
  int SW_Manual = SW_ADC(); /* Get SW */
  if (SW_Manual > 0) {
    delay(100);
    status_manual[SW_Manual - 1] = 0;
    if (RelayStatus[SW_Manual - 1] == 1) ControlRelay_Bymanual(SW_Manual - 1, "off");
    else ControlRelay_Bymanual(SW_Manual - 1, "on");
    while (SW_ADC() > 0) delay(1);
  }
  unsigned long currentTime = millis();
  if (currentTime - previousTime_Temp_soil >= eventInterval) {
    ControlRelay_Bytimmer();
    if (soil_error == 0) ControlRelay_BysoilMinMax();
    if (temp_error == 0) ControlRelay_BytempMinMax();
    Get_light();
    Serial.println("Temp : " + String(temp, 2) + " C , Humid : " +
                   String(humidity, 2) + " %RH , Brightness : " +
                   String(luxMy, 2) + " Klux , Soil : " +
                   String(soil, 2) + " % ");
    previousTime_Temp_soil = currentTime;
  }
  Blink_LED_wifi();
  client.loop(); delay(10);
}

/* --------- Auto Connect Wifi and server and setup value init ------------- */
void TaskWifiStatus(void* pvParameters) {
  while (1) {
    connectWifiStatus = cannotConnect;
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    do {
      Serial.println("ssid = " + String(ssid.c_str()) + " pass = " + String(password.c_str()));
      delay(12000);
    } while (WiFi.status() != WL_CONNECTED);
    connectWifiStatus = wifiConnected;
    client.setServer(mqtt_server.c_str(), mqtt_port.toInt());
    client.setCallback(callback);
    timeClient.begin();
    do {
      client.connect(mqtt_Client.c_str(), mqtt_username.c_str(), mqtt_password.c_str());
      Serial.println("NETPIE2020 can not connect");
      delay(12000);
    } while (!client.connected()) ;
    connectWifiStatus = serverConnected;
    Serial.println("NETPIE2020 connected");
    client.subscribe("@private/#");
    get_curentTimer();
    check_sendData_status = 1;
    sendStatus_RelaytoWeb();
    delay(2000);
    while (WiFi.status() == WL_CONNECTED) {
      unsigned long currentTime_Update_data = millis();
      if (currentTime_Update_data - previousTime_Update_data > (eventInterval_publishData)) {
        UpdateData_To_Server();
        previousTime_Update_data = currentTime_Update_data;
      }
      sendStatus_RelaytoWeb();
      send_soilMinMax();
      send_tempMinMax();
      delay(500);
      if (!client.connected()) {
        break;
      }
    }
  }
}

/* --------- Auto Connect Serial ------------- */
void TaskWaitSerial(void* WaitSerial) {
  while (1) {
    if (Serial.available()) webSerialJSON();
    delay(500);
  }
}
