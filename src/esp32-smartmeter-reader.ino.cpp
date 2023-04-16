# 1 "C:\\Users\\lukas\\AppData\\Local\\Temp\\tmpt1edmwdk"
#include <Arduino.h>
# 1 "C:/Users/lukas/OneDrive/Documents/PlatformIO/Projects/230120-142925-nodemcu-32s/src/esp32-smartmeter-reader.ino"
#include "config.h"
#include "mbedtls/aes.h"
#include <FastCRC.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
void SetupWifi();
void ReconnectMQTT();
void ReadSerialData();
int BytesToInt(byte bytes[], unsigned int left, unsigned int right);
void ParseReceivedData();
bool ValidateCRC();
void DecryptMessage(byte decrpyted_message[74]);
void setup();
void loop();
#line 10 "C:/Users/lukas/OneDrive/Documents/PlatformIO/Projects/230120-142925-nodemcu-32s/src/esp32-smartmeter-reader.ino"
void SetupWifi() {
  delay(10);
  console->println();
  console->print("Connecting to ");
  console->println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    console->print(".");
  }

  console->println("");
  console->println("WiFi connected");
  console->println("IP address: ");
  console->println(WiFi.localIP());
}



WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

void ReconnectMQTT() {
  while (!mqtt_client.connected()) {
    console->print("Attempting MQTT connection...");
    if (mqtt_client.connect("ESPClient", MQTT_USER, MQTT_PASS)) {
      console->println("connected");
    } else {
      console->print("failed, rc=");
      console->print(mqtt_client.state());
      console->println(" try again in 5 seconds");
      delay(5000);
    }
  }
}



const unsigned int MESSAGE_LENGTH = 105;
byte received_data[MESSAGE_LENGTH];

void ReadSerialData() {
  static byte previous_byte = 0;
  static bool receiving = false;
  static unsigned int pos;

  while (smart_meter->available() > 0) {
    byte current_byte = smart_meter->read();
    if (!receiving) {

      console->println(received_data[0]);
      console->println(received_data[1]);
      console->println("-");
      if (previous_byte == 0x7E && current_byte == 0xA0) {
        receiving = true;
        received_data[0] = 0x7E;
        received_data[1] = 0xA0;
        pos = 2;
      }
    } else {
      if (pos < MESSAGE_LENGTH) {
        received_data[pos] = current_byte;
        pos++;
      } else {
        ParseReceivedData();
        receiving = false;
      }
    }
    previous_byte = current_byte;
  }
}



int BytesToInt(byte bytes[], unsigned int left, unsigned int right) {
  int result = 0;
  for (unsigned int i = left; i < right; i++) {
    result = result * 256 + bytes[i];
  }
  return result;
}

void ParseReceivedData() {

  if (!ValidateCRC()) {
    return;
  }


  byte decrpyted_message[74];
  DecryptMessage(decrpyted_message);


  int year = BytesToInt(decrpyted_message, 22, 24);
  int month = BytesToInt(decrpyted_message, 24, 25);
  int day = BytesToInt(decrpyted_message, 25, 26);
  int hour = BytesToInt(decrpyted_message, 27, 28);
  int minute = BytesToInt(decrpyted_message, 28, 29);
  int second = BytesToInt(decrpyted_message, 29, 30);
  char timestamp[19];
  sprintf(timestamp, "%02d.%02d.%04d %02d:%02d:%02d", day, month, year, hour, minute, second);


  StaticJsonDocument<256> doc;
  doc["timestamp"] = timestamp;
  doc["+A"] = BytesToInt(decrpyted_message, 35, 39)/1000.0;
  doc["-A"] = BytesToInt(decrpyted_message, 40, 44)/1000.0;
  doc["+R"] = BytesToInt(decrpyted_message, 45, 49)/1000.0;
  doc["-R"] = BytesToInt(decrpyted_message, 50, 54)/1000.0;
  doc["+P"] = BytesToInt(decrpyted_message, 55, 59);
  doc["-P"] = BytesToInt(decrpyted_message, 60, 64);
  doc["+Q"] = BytesToInt(decrpyted_message, 65, 69);
  doc["-Q"] = BytesToInt(decrpyted_message, 70, 74);
  char payload[256];
  serializeJson(doc, payload);


  console->println(payload);
  if (MQTT_ENABLED) {
    mqtt_client.publish(MQTT_TOPIC, payload, false);
  }
}



FastCRC16 CRC16;

bool ValidateCRC() {
  byte message[101];
  memcpy(message, received_data + 1, 101);
  int crc = CRC16.x25(message, 101);
  int expected_crc = received_data[103] * 256 + received_data[102];
  if (crc != expected_crc) {
    console->println("WARNING: CRC check failed");
    return false;
  }
  return true;
}



mbedtls_aes_context aes;

void DecryptMessage(byte decrpyted_message[74]) {

  byte encrpyted_message[74] = {0};
  memcpy(encrpyted_message, received_data + 28, 74);
  byte nonce[16] = {0};
  memcpy(nonce, received_data + 14, 8);
  memcpy(nonce + 8, received_data + 24, 4);
  nonce[15] = 0x02;


  size_t nc_off = 0;
  unsigned char stream_block[16] = {0};
  mbedtls_aes_crypt_ctr(&aes, 74, &nc_off, nonce, stream_block, encrpyted_message, decrpyted_message);
}



void setup() {
  console->begin(CONSOLE_BAUD_RATE);
  smart_meter->begin(SMARTMETER_BAUD_RATE);
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, KEY, 128);
  if (MQTT_ENABLED) {
    SetupWifi();
    mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
  }
}

void loop() {
  if (!mqtt_client.connected()) {
    ReconnectMQTT();
  }
  mqtt_client.loop();
  ReadSerialData();
}