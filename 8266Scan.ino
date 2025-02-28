#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SSID ""
#define PASSWORD ""

#define SS_PIN 15 // D8
#define RST_PIN 2 // D4
#define BUZZER 5 // D1

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

WebSocketsServer webSocket = WebSocketsServer(81);
uint8_t clientID;

bool addMode = false;
bool readMode = true;
bool extendMode = false;
bool extendFirstScan = false;
bool deleteMode = false;
bool deleteFirstScan = false;

unsigned long buzzerStartTime = 0;
unsigned long buzzerTime = 0;
bool buzzerActive = false;

unsigned long restartStartTime = 0;
unsigned long restartTime = 0;
bool restartActive = false;

int blockNum; 
byte blockData[16];
byte bufferLen = 18;
byte readBlockData[18];

String numberReceive;
String ownerReceive;
String timestampReceive;
String expirationDateReceive;
String currentDate;

String numberSend;
String ownerSend;
String timestampSend;
String expirationDateSend;
String expirationDateUpdate;

void writeStringToBlocks(int startBlock, String data);
String readStringFromBlocks(int startBlock, int blockCount);
void writeStringToBlock(int block, String data);
String readStringFromBlock(int block);
void writeDataToBlock(int blockNum, byte blockData[]);
void readDataFromBlock(int blockNum, byte readBlockData[]);
void getDate();
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  String message = String((char *)data).substring(0, len);
  if (message.startsWith("Database:")) {
    int index1 = message.indexOf(',');
    int index2 = message.indexOf(',', index1 + 1);

    numberReceive = message.substring(9, index1);
    ownerReceive = message.substring(index1 + 1, index2);
    timestampReceive = message.substring(index2 + 1);

    addMode = true;
    readMode = false;
    extendMode = false;
    deleteMode = false;

    String message = "ReceiveDatabaseSuccessfully";
    webSocket.broadcastTXT(message);

    Serial.println("Sẵn sàng thêm người dùng mới");
    Serial.println("Chủ sở hữu: " + ownerReceive);
    Serial.println("Biển số xe: " + numberReceive);
    Serial.println("Tên ảnh: " + timestampReceive + ".jpg");
    Serial.println();
  } 

  else if (message.startsWith("HideNotification")) {
    String message = "HideNotification";
    webSocket.broadcastTXT(message);
  } 

  else if (message.startsWith("ErrorFound")) {
    String message = "ErrorFound";
    webSocket.broadcastTXT(message);
  } 

  else if (message.startsWith("ExtendExpirationDate")) {
    addMode = false;
    readMode = false;
    extendMode = true;
    extendFirstScan = true;
    deleteMode = false;

    String message = "ReadyToExtend";
    webSocket.broadcastTXT(message);

    Serial.println("Sẵn sàng để gia hạn");
  } 

  else if (message.startsWith("DeleteUser")) {
    addMode = false;
    readMode = false;
    extendMode = false;
    deleteMode = true;
    deleteFirstScan = true;

    String message = "ReadyToDelete";
    webSocket.broadcastTXT(message);

    Serial.println("Sẵn sàng để xóa người dùng");
  } 

  else if (message.startsWith("ResetSystem")) {
    addMode = false;
    readMode = true;
    extendMode = false;
    extendFirstScan = false;
    deleteMode = false;
    deleteFirstScan = false;

    numberReceive = "";
    ownerReceive = "";
    timestampReceive = "";
    expirationDateReceive = "";
    currentDate = "";

    numberSend = "";
    ownerSend = "";
    timestampSend = "";
    expirationDateSend = "";
    expirationDateUpdate = "";
    
    String message = "ResetSystem";
    webSocket.broadcastTXT(message);

    Serial.println("Đã đặt lại hệ thống");
  } 

  else if (message.startsWith("RestartSystem")) {
    Serial.println("Khởi động lại hệ thống");
    String message = "RestartSystem";
    webSocket.broadcastTXT(message);

    restartStartTime = millis();
    restartTime = 1000;
    restartActive = true;
  } 
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER, OUTPUT);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Đã kết nối WiFi");
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  timeClient.begin();
  timeClient.setTimeOffset(7*3600);

  SPI.begin();            
  mfrc522.PCD_Init();     
  delay(4);               
  mfrc522.PCD_DumpVersionToSerial();  

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

void loop() {
  webSocket.loop();

  if (buzzerActive && (millis() - buzzerStartTime >= buzzerTime)) {
    digitalWrite(BUZZER, LOW);
    buzzerActive = false;
  }

  if (restartActive && (millis() - restartStartTime >= restartTime)) {
    ESP.restart();
    restartActive = false;
  }

  if ((!mfrc522.PICC_IsNewCardPresent())||(!mfrc522.PICC_ReadCardSerial())) {
    return;
  }

  if (readMode) {
    ownerSend = readStringFromBlocks(4, 3);
    numberSend = readStringFromBlocks(8, 3);
    expirationDateSend = readStringFromBlocks(12, 3);
    timestampSend = readStringFromBlocks(16, 3);

    getDate();
    bool expired = compareDate(expirationDateSend, currentDate);
    if (expired) {
      Serial.println("Thẻ đã hết hạn");
    }

    buzzerStartTime = millis();
    buzzerTime = 200;
    buzzerActive = true;
    digitalWrite(BUZZER, HIGH);

    String message = "Log:" + numberSend + "," + ownerSend + "," + timestampSend + "," + expired;
    webSocket.broadcastTXT(message);

    if ((ownerSend!="")&&(numberSend!="")&&(timestampSend!="")) {
      String message = "ShowInformation";
      webSocket.broadcastTXT(message);
    } else {
        String message = "InvalidInformation";
        webSocket.broadcastTXT(message);
      }

    Serial.println("Đã đọc dữ liệu từ thẻ: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();
    Serial.println("Chủ sỡ hữu: " + ownerSend);
    Serial.println("Biển số xe: " + numberSend);
    Serial.println("Ngày hết hạn: " + expirationDateSend);
    Serial.println("Tên ảnh: " + timestampSend + ".jpg");
    Serial.println();
  }
  
  if (addMode) {
    buzzerStartTime = millis();
    buzzerTime = 200;
    buzzerActive = true;
    digitalWrite(BUZZER, HIGH);

    writeStringToBlocks(4, ownerReceive);
    writeStringToBlocks(8, numberReceive);
    getDate();
    writeStringToBlocks(12, expirationDateReceive);
    writeStringToBlocks(16, timestampReceive);
      
    String message = "Database:," + ownerReceive + "," + numberReceive + "," + expirationDateReceive;
    webSocket.broadcastTXT(message);

    addMode = false;
    readMode = true;
    extendMode = false;
    deleteMode = false;

    Serial.println("Đã ghi dữ liệu vào thẻ: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();
    Serial.println("Chủ sỡ hữu: " + ownerReceive);
    Serial.println("Biển số xe: " + numberReceive);
    Serial.println("Ngày hết hạn: " + expirationDateReceive);
    Serial.println("Tên ảnh: " + timestampReceive + ".jpg");
    Serial.println();
  }

  if (extendMode) {
    buzzerStartTime = millis();
    buzzerTime = 200;
    buzzerActive = true;
    digitalWrite(BUZZER, HIGH);

    if (!extendFirstScan) {
      writeStringToBlocks(12, expirationDateUpdate);

      String message = "Database:?," + ownerSend + "," + numberSend + "," + expirationDateUpdate;
      webSocket.broadcastTXT(message);

      addMode = false;
      readMode = true;
      extendMode = false;
      deleteMode = false;

      Serial.println("Đã cập nhật ngày hết hạn: " + expirationDateUpdate);
    } else {
        ownerSend = readStringFromBlocks(4, 3);
        numberSend = readStringFromBlocks(8, 3);
        expirationDateSend = readStringFromBlocks(12, 3);
        updateExpirationDate();
        String message = "ExtendFirstScanDone";
        webSocket.broadcastTXT(message);
        extendFirstScan = false;
      }
  }

  if (deleteMode) {
    buzzerStartTime = millis();
    buzzerTime = 200;
    buzzerActive = true;
    digitalWrite(BUZZER, HIGH);

    if (!deleteFirstScan) {
      writeStringToBlocks(4, "");
      writeStringToBlocks(8, "");
      writeStringToBlocks(12, "");
      writeStringToBlocks(16, "");

      String message = "Delete:" + ownerSend + "," + numberSend;
      webSocket.broadcastTXT(message);

      addMode = false;
      readMode = true;
      extendMode = false;
      deleteMode = false;

      Serial.println("Đã xóa thẻ: " + ownerSend + "," + numberSend);
    } else {
        ownerSend = readStringFromBlocks(4, 3);
        numberSend = readStringFromBlocks(8, 3);
        String message = "DeleteFirstScanDone";
        webSocket.broadcastTXT(message);
        deleteFirstScan = false;
      }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void writeDataToBlock(int blockNum, byte blockData[]) {
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    return;
  }
  status = mfrc522.MIFARE_Write(blockNum, blockData, 16);
  if (status != MFRC522::STATUS_OK) {
    return;
  }
}

void writeStringToBlocks(int startBlock, String data) {
  int len = data.length();
  byte buffer[16];
  for (int i = 0; i < 3; i++) {
    int offset = i * 16;
    for (int j = 0; j < 16; j++) {
      if (offset + j < len) {
        buffer[j] = data[offset + j];
      } else {
          buffer[j] = 0;
        }
    }
    writeDataToBlock(startBlock + i, buffer);
  }
}

void readDataFromBlock(int blockNum, byte readBlockData[]) {
  byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    return;
  }
  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    return;
  }
}

String readStringFromBlocks(int startBlock, int blockCount) {
  String result = "";
  byte buffer[18];
  for (int i = 0; i < blockCount; i++) {
    readDataFromBlock(startBlock + i, buffer);
    for (int j = 0; j < 16; j++) {
      if (buffer[j] != 0) {
        result += (char)buffer[j];
      }
    }
  }
  return result;
}

// lấy định dạng ngày tháng năm dd/mm/yyyy
void getDate() {
  timeClient.update();

  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  int currentYear = ptm->tm_year + 1900;

  char dateBuffer[11];

  sprintf(dateBuffer, "%02d/%02d/%04d", monthDay, currentMonth, currentYear);
  currentDate = String(dateBuffer);

  int nextMonth = currentMonth + 1;
  int nextYear = currentYear;

  if (nextMonth > 12) {
    nextMonth = 1;
    nextYear++;
  }

  sprintf(dateBuffer, "%02d/%02d/%04d", monthDay, nextMonth, nextYear);
  expirationDateReceive = String(dateBuffer);
}

// gia hạn thêm 1 tháng kể từ ngày hết hạn đọc được
void updateExpirationDate() {
  int day = expirationDateSend.substring(0, 2).toInt();
  int month = expirationDateSend.substring(3, 5).toInt();
  int year = expirationDateSend.substring(6, 10).toInt();

  month += 1;

  if (month > 12) {
    month = 1;
    year += 1;
  }

  expirationDateUpdate = 
    (day < 10 ? "0" : "") + String(day) + "/" +
    (month < 10 ? "0" : "") + String(month) + "/" +
    String(year);
}

bool compareDate(String expirationDate, String currentDate) {
  int index1 = expirationDate.indexOf('/');
  int index2 = expirationDate.indexOf('/', index1 + 1);

  int ddE = (expirationDate.substring(0, index1)).toInt();
  int mmE = (expirationDate.substring(index1 + 1, index2)).toInt();
  int yyyyE = (expirationDate.substring(index2 + 1)).toInt();

  index1 = currentDate.indexOf('/');
  index2 = currentDate.indexOf('/', index1 + 1);

  int ddC = (currentDate.substring(0, index1)).toInt();
  int mmC = (currentDate.substring(index1 + 1, index2)).toInt();
  int yyyyC = (currentDate.substring(index2 + 1)).toInt();

  if (yyyyE < yyyyC) return true;
    else if (yyyyE == yyyyC && mmE < mmC) return true;
      else if (yyyyE == yyyyC && mmE == mmC && ddE < ddC) return true;
        else return false;
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_TEXT:
      handleWebSocketMessage(NULL, payload, length);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[%u] ngắt kết nối\n", num);
      break;
    case WStype_CONNECTED: {
      clientID = num;
      IPAddress ip = webSocket.remoteIP(clientID);
      Serial.printf("[%u] kết nối từ %s\n", clientID, ip.toString().c_str());
      String message = "HideNotification";
      webSocket.sendTXT(clientID, message);
      break;
    }
  }
}
