#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_PWMServoDriver.h>  // PCA9685用ライブラリ
#include <WiFiUdp.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>

#include <cstring>

const char ssid[] = "Buffalo-G-FD18";
const char pass[] = "xxxxxxxxxxxxxxxxxx";
const char hostname[] = "esp32-taguro";

const bool useDhcp = true;
const IPAddress local_ip(192,168,11,102);
const IPAddress gateway(255,255,255,0);
const IPAddress subnet(255,255,255,0);
const IPAddress dns1(8,8,8,8);
const IPAddress dns2(8,8,4,4);

#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

static AsyncWebServer server(80);
static Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
static WiFiUDP udp;
static const uint16_t localUdpPort = 4210;

// 後でLogLevel等を設定したい。ラッピング用関数をここに。
void serialPrintf(const char* format, ...) {
  char sendbuffer[128];  // 適切なサイズを確保
  va_list args;
  va_start(args, format);
  vsnprintf(sendbuffer, sizeof(sendbuffer), format, args);
  va_end(args);
  Serial.write(sendbuffer);
}

void setup() {
  Serial.begin(115200);
  serialPrintf("Connecting to WIFI SSID=%s ...\n", ssid);

  WiFi.mode(WIFI_STA);
  if (!useDhcp) {
    WiFi.config(local_ip, gateway, subnet, dns1, dns2);
  }
  WiFi.setHostname(hostname);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    serialPrintf(".");
  }
  serialPrintf("\n");
  IPAddress myIP = WiFi.localIP();
  serialPrintf("IP Address: %s\n", myIP.toString().c_str());

  // CORSヘッダーを設定するための関数
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  
  // OPTIONSリクエスト（プリフライトリクエスト）に対応
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404);
    }
  });

  // ルートへのアクセス
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->addHeader("Server","ESP Async Web Server");
    response->printf("<!DOCTYPE html><html><head><title>Webpage at %s</title></head><body>", request->url().c_str());
    response->print("<h2>Hello ");
    response->print(request->client()->remoteIP());
    response->print("</h2>");
    response->print("<h3>General</h3>");
    response->print("<ul>");
    response->printf("<li>Version: HTTP/1.%u</li>", request->version());
    response->printf("<li>Method: %s</li>", request->methodToString());
    response->printf("<li>URL: %s</li>", request->url().c_str());
    response->printf("<li>Host: %s</li>", request->host().c_str());
    response->printf("<li>ContentType: %s</li>", request->contentType().c_str());
    response->printf("<li>ContentLength: %u</li>", request->contentLength());
    response->printf("<li>Multipart: %s</li>", request->multipart()?"true":"false");
    response->print("</ul>");

    response->print("<h3>Headers</h3>");
    response->print("<ul>");
    int headers = request->headers();
    for(int i=0;i<headers;i++){
      const AsyncWebHeader* h = request->getHeader(i);
      response->printf("<li>%s: %s</li>", h->name().c_str(), h->value().c_str());
    }
    response->print("</ul>");
    response->print("<h3>Parameters</h3>");
    response->print("<ul>");
    int params = request->params();
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        response->printf("<li>FILE[%s]: %s, size: %u</li>", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        response->printf("<li>POST[%s]: %s</li>", p->name().c_str(), p->value().c_str());
      } else {
        response->printf("<li>GET[%s]: %s</li>", p->name().c_str(), p->value().c_str());
      }
    }
    response->print("</ul>");
    response->print("</body></html>");
    //send the response last
    request->send(response);
  });

  server.on("/servos", HTTP_POST, [](AsyncWebServerRequest *request) {
    //request->send(200, "text/plain", "Received POST");
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String jsonBody;
    for (size_t i = 0; i < len; i++) jsonBody += (char)data[i];
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonBody);
    int servonum = 0;
    uint16_t on_time = 0;
    uint16_t off_time = 0;
    bool validation_error = false;
    if (error) {
      request->send(400, "application/json", "{\"status\":\"failed to parse json\"}");
      return;
    }
    //String name = doc["name"];
    //const char *p = name.c_str()
    const char *keys[] = {"id", "on_time", "off_time"};
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
      for (int i=0;i<(sizeof(keys)/sizeof(const char *));i++) {
        const char *key = keys[i];
        bool isNUll = obj[key].isNull();
        if (isNUll) {
          request->send(400, "application/json", "{\"status\":\"validation error\"}");
          return;
        }
      }
      servonum = obj["id"];
      on_time = obj["on_time"];
      off_time = obj["off_time"];
      pwm.setPWM(servonum, on_time, off_time);
    }
    request->send(200, "application/json", "{\"status\":\"ok\"}");
    return;
  });

  server.on("/stop_all", HTTP_POST, [](AsyncWebServerRequest *request) {
    for (int i=0;i<16;i++) pwm.setPWM(i, 0, 0);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  }, NULL, NULL);

  server.on("/servos", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->addHeader("Server","ESP Async Web Server");
    //char buffer[100];
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for(int i=0;i<16;i++) {
      uint16_t on_time =  pwm.getPWM(i, false);
      uint16_t off_time =  pwm.getPWM(i, true);
      JsonObject tmp = arr.add<JsonObject>();
      tmp["id"] = i;
      tmp["on_time"] = on_time;
      tmp["off_time"] = off_time;
    } 
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  /*
  server.on("^\\/adc\\/([0-9]+)$", HTTP_GET, [](AsyncWebServerRequest *request){
    String sensorIdStr = request->pathArg(0);
    int sensorId = sensorIdStr.toInt();
  });
  */

  server.begin();

  // UDPリスナー開始
  Serial.printf("Listening UDP Server on Port %d\n", localUdpPort);
  udp.begin(localUdpPort);

  // PCA9685の初期化を実施
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
  delay(10);
}

void wificheck() {
    static uint16_t currentWifiStatus = 0;
    while (WiFi.status() != currentWifiStatus) {
    currentWifiStatus = WiFi.status();
    switch (currentWifiStatus) {
      case WL_NO_SHIELD:        Serial.println("WiFi NO_SHIELD.");        break;
      case WL_IDLE_STATUS:      Serial.println("WiFi IDLE_STATUS.");      break;
      case WL_NO_SSID_AVAIL:    Serial.println("WiFi NO_SSID_AVAIL.");    break;
      case WL_SCAN_COMPLETED:   Serial.println("WiFi SCAN_COMPLETED.");   break;
      case WL_CONNECTED:        Serial.println("WiFi CONNECTED.");        break;
      case WL_CONNECT_FAILED:   Serial.println("WiFi CONNECT_FAILED.");   break;
      case WL_CONNECTION_LOST:  Serial.println("WiFi CONNECTION_LOST.");  break;
      case WL_DISCONNECTED:     Serial.println("WiFi DISCONNECTED.");     break;
      default:                  Serial.println("WiFi STATUS unknown!!");
    }
  }
}

struct ServoPwmStatus {
  uint16_t on_time;
  uint16_t off_time;
  float angle;
};

ServoPwmStatus readServoPwmStatus(uint8_t servonum, bool show = true, int max_angle = 180) {
  uint16_t on_time =  pwm.getPWM(servonum, false);
  uint16_t off_time =  pwm.getPWM(servonum, true);
  if (show) {
    serialPrintf("Servo[ID=%d]: ON= %d OFF=%d \r\n", servonum, on_time, off_time);
  }
  ServoPwmStatus ret;
  ret.off_time = off_time;
  ret.on_time = on_time;
  
  // 50Hz -> 0.02 sec = 20msec
  // 20 / 4096 = 0.0048828125
  float pulse_msec = 0.0048828125 * off_time;
  ret.angle = ((pulse_msec - 0.5) / 2.0) * max_angle;
  if (ret.angle <= 0.0) ret.angle = 0;
  if (ret.angle >= max_angle) ret.angle = max_angle;
  return ret;
}

void setServoPwm(uint8_t servonum, float angle, int max_angle = 180) {
  if (angle <= 0) angle = 0;
  if (angle >= max_angle) angle = max_angle;
  float pulse_msec = angle * (2.0 / max_angle) + 0.5;
  int16_t pluse_step = (int16_t)(pulse_msec / 0.0048828125);
  serialPrintf("step = %d\n", pluse_step);
  pwm.setPWM(servonum, 0, pluse_step);
}

void removeNewline(char* str) {
    int readIdx = 0;
    int writeIdx = 0;
    while (str[readIdx] != '\0') {
        if (str[readIdx] != '\n') {
            str[writeIdx++] = str[readIdx];
        }
        readIdx++;
    }
    str[writeIdx] = '\0'; // null終端を忘れずに
}

void floatArrayToCSV(char* buffer, size_t bufferSize, float* values, size_t length) {
    size_t offset = 0;
    for (size_t i = 0; i < length; ++i) {
        int written = snprintf(buffer + offset, bufferSize - offset, "%.2f%s",
                               values[i], (i < length - 1) ? "," : "");
        if (written < 0 || written >= (int)(bufferSize - offset)) {
            // エラーまたはバッファオーバー
            break;
        }
        offset += written;
    }
}

void commandInterpreter(char *incomingPacket, char *replyPacket, size_t replyPacketSize, bool *sendReply) {
  // コマンドとパラメーターによって要調整
  static char splitedIncomingPacket[10][255];

  // コマンドとパラメータに分離
  const int maxToken = sizeof(splitedIncomingPacket) / sizeof(splitedIncomingPacket[0]);
  for (int i=0;i<sizeof(splitedIncomingPacket);i++) {
    ((char*)splitedIncomingPacket)[i] = '\0';
  }

  int tokenNum = 0;
  char* token = strtok(incomingPacket, ",");
  while (token != NULL) {
      //serialPrintf("Token(%d/%d):%s\n", tokenNum, maxToken, token);
      strncpy(splitedIncomingPacket[tokenNum], token, sizeof(splitedIncomingPacket[0]) - 1);
      token = strtok(NULL, ",");
      tokenNum++;
      if (tokenNum == maxToken) break;
  }
  
  const char *command = splitedIncomingPacket[0];
  if (strcmp(command, "CONNECT") == 0) {
    Serial.printf("Connected!\n");
    snprintf(replyPacket, replyPacketSize, "OK");
    *sendReply = true;
  }else if (strcmp(command, "SET_JOINT_ANGLE") == 0) {
    if (tokenNum == 4) {
      char* endptr;
      int servoId = (int)strtol(splitedIncomingPacket[1], &endptr, 10);
      float angle = strtof(splitedIncomingPacket[2], &endptr);
      float speed = strtof(splitedIncomingPacket[3], &endptr);
      serialPrintf("ServoId=%d\n", servoId);
      serialPrintf("Angle=%f\n", angle);
      serialPrintf("Speed=%f\n", speed);
      snprintf(replyPacket, replyPacketSize, "OK");
      
      setServoPwm(servoId, angle);

    } else {
      serialPrintf("Invalid Parameters\n");
      snprintf(replyPacket, replyPacketSize, "NG");
    }
    *sendReply = true;
  }else if(strcmp(command, "SET_ALL_JOINT_ANGLES") == 0) {
    if (tokenNum == 9) {
      char* endptr;
      float angle;
      for(int servoId=0;servoId<7;servoId++) {
        float angle = strtof(splitedIncomingPacket[1+servoId], &endptr);
        serialPrintf("Servo(%d)=%f [Degree]\n", servoId, angle);
        setServoPwm(servoId, angle);
      }
      float speed = strtof(splitedIncomingPacket[8], &endptr);
      serialPrintf("Speed=%f\n", speed);

      snprintf(replyPacket, replyPacketSize, "OK");
    } else {
      serialPrintf("Invalid Parameters\n");
      snprintf(replyPacket, replyPacketSize, "NG");
    }
    *sendReply = true;
  } else if(strcmp(command, "GET_JOINT_ANGLES") == 0) {
    float tmp[7];
    for(int i=0;i<7;i++) {
      ServoPwmStatus servo = readServoPwmStatus(i);
      tmp[i] = (float)servo.angle;
    }
    floatArrayToCSV(replyPacket, replyPacketSize, tmp, 7);
    *sendReply = true;
  } else if(strcmp(command, "DISCONNECT") == 0) {
    snprintf(replyPacket, replyPacketSize, "OK");
    *sendReply = true;
  }else {
    // Unknown Packet
    serialPrintf("Unknown Command (%s) \n", incomingPacket);
    *sendReply = false;
  }
}

void processUdpPacket() {
  static char incomingPacket[255];
  static char replyPacket[255];
  // UDPパケット受信処理
  int packetSize = udp.parsePacket();
  if (packetSize) {
    // パケット情報の表示
    Serial.printf("パケット受信: %d バイト, 送信元: %s, ポート: %d\n", 
                  packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    // パケットの内容を読み取り
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';  // 終端文字を追加
    }
    removeNewline(incomingPacket); // 改行コードの削除
    serialPrintf("Packet Content: [%s]\n", incomingPacket);   
    bool sendReply = false;
    commandInterpreter(incomingPacket, replyPacket, sizeof(replyPacket), &sendReply);
    if (sendReply) {
      serialPrintf("Sending reply (%s) over UDP \n", replyPacket);
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.write((uint8_t*)replyPacket, strlen(replyPacket));
      udp.endPacket();
    }
  }
}

void processSerialMessage() {
  static char recvBuffer[255];
  static int recvCounter = 0;
  static char replyBuffer[255];

  if (!Serial.available()) return;
  int inByte = Serial.read();
  if (inByte == 0x0D) {
    // Ignore
  }else if (inByte == 0x0A) { // Enterを押したときの処理
    recvBuffer[recvCounter] = '\0';
    //serialPrintf("recvBuffer=[%s] recvCounter=%d\r\n", recvBuffer, recvCounter);
    bool sendReply = false;
    commandInterpreter(recvBuffer, replyBuffer, sizeof(replyBuffer), &sendReply);
    if (sendReply) {
      serialPrintf("%s\n", replyBuffer);
    }
    recvCounter = 0;
    recvBuffer[0] = '\0';
  }else {
    //serialPrintf("inByte=[%c] (0x%x)\n", inByte, inByte);
    recvBuffer[recvCounter++] = (unsigned char)inByte;
  }
}

void loop(){

  //Wifiが不安定なら再接続させる
  static unsigned long previousMillis = 0;
  static unsigned long interval = 30000;
  wificheck();
  unsigned long currentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    serialPrintf("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  
  // シリアル通信経由の制御指令を受け取る
  processSerialMessage();

  // UDP経由でメッセージを受け取る
  processUdpPacket();
}
