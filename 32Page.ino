#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SSID ""
#define PASSWORD ""

IPAddress localIP;

#define WEBAPP_URL "https://script.google.com/macros/s//exec"

const String FIREBASE_STORAGE_BUCKET = "";

char esp8266LocalIP[16];
char serverIP[16];

// lấy IP tự động thay thế octet 4
void createModifiedIP(IPAddress ip, uint8_t newLastOctet, char* result) {
  snprintf(result, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], newLastOctet);
}

String Feedback = ""; 
String Command = "";
String cmd = "";
String pointer = "";

byte receiveState = 0;
byte cmdState = 1;
byte strState = 1;
byte questionstate = 0;
byte equalstate = 0;
byte semicolonstate = 0;

int flashValue = 0;

String status = "Đang chờ";
String statusColor;
const String black = "#000";
const String red = "#FF6060";
const String green = "#4CAF50";

String notificationMain = "";
String notificationSub = "";
String notificationColor = "#4CAF50";

unsigned long notificationStartTime = 0;
unsigned long notificationTime = 0;
bool notificationActive = false;

unsigned long logStartTime = 0;
unsigned long logTime = 0;
bool logActive = false;

String numberSend;
String ownerSend;
String timestampSend;

String numberReceive = "Chưa xác định";
String ownerReceive = "Chưa xác định";
String timestampReceive;
String expirationDateReceive;

String imagesrcPlate;

String currentDate;
String currentTime;
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WiFiServer server(80);
WiFiClient client;
WebSocketsClient webSocket;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.fb_count = 2;
  } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_count = 1;
    }

  config.jpeg_quality = 10;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Khởi tạo camera lỗi 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();

  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
  }


  s->set_framesize(s, FRAMESIZE_CIF); // QVGA (320 x 240); CIF (352 x 288); VGA (640 x 480); SVGA (800 x 600); XGA (1024 x 768); SXGA (1280 x 1024); UXGA (1600 x 1200);
  s->set_brightness(s, 0);
  s->set_contrast(s, 2);
  s->set_saturation(s, 0);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Đã kết nối WiFi.");
  localIP = WiFi.localIP();
  Serial.println(localIP);

  createModifiedIP(localIP, 100, esp8266LocalIP);
  Serial.print("ESP8266 Local IP: ");
  Serial.println(esp8266LocalIP);
  
  createModifiedIP(localIP, 50, serverIP);

  server.begin();

  webSocket.begin(esp8266LocalIP, 81, "/");
  webSocket.onEvent(onWebSocketEvent);
  webSocket.setReconnectInterval(1000);

  timeClient.begin();
  timeClient.setTimeOffset(7*3600);
}

// bảng thông báo
String showNotification() {
  String head = 
    "<head>"
      "<meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
      "<link rel='preconnect' href='https://fonts.gstatic.com'>"
      "<link href='https://fonts.googleapis.com/css2?family=Roboto&display=swap' rel='stylesheet'>"
    "</head>";

  String css =
      "<style>"
        "body {font-family: 'Montserrat', sans-serif; background-color: #fff; overflow: hidden;}"
        ".notification {padding: 10px 20px 10px 20px; margin: auto auto;}"
        ".notificationMain {color: " + notificationColor + ";}"
        ".notificationSub {font-weight: normal;}"
      "</style>";

  String body =
    "<body>"
      "<div class='notification' id='notification'>"
        "<h2 class='notificationMain' id='title'>" + notificationMain + "</h2>"
        "<h3 class='notificationSub' id='content'>" + notificationSub + "</h3>"
      "</div>"
    "</body>";
  
  String notify = head + css + body;

  return notify;
}

// ghi dữ liệu vào bảng tính /tên bảng tính, /nội dung cột A:D
void writeToSheet(String sheet, String content1, String content2, String content3, String content4) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  http.begin(WEBAPP_URL);
  http.addHeader("Content-Type", "application/json");

  String jsonData = 
    "{"
      "\"sheet\":\"" + sheet + "\","
      "\"content1\":\"" + content1 + "\","
      "\"content2\":\"" + content2 + "\","
      "\"content3\":\"" + content3 + "\","
      "\"content4\":\"" + content4 + "\""
    "}";

  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode > 0) {
    Serial.println("Tải lên thành công");
  } else {
    Serial.println("Tải lên thất bại");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println(response);
  }

  http.end();
}

// thông báo lỗi
void error() {
  notificationColor = red;
  notificationMain = "Đã xảy ra lỗi";
  notificationSub = "Vui lòng khởi động lại hệ thống";
  webSocket.sendTXT("ErrorFound");
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  String message = String((char *)data).substring(0, len);

  if (message.startsWith("ReceiveDatabaseSuccessfully")) {
    notificationColor = green;
    notificationMain = "Đang chờ quét thẻ";
    notificationSub = "Quét thẻ để thêm người dùng mới";
  
    Serial.println("Esp8266 nhận dữ liệu người dùng mới thành công");
  } 

  else if (message.startsWith("InvalidInformation")) {
    notificationColor = red;
    notificationMain = "Thẻ không tồn tại";
    notificationSub = "Hãy kiểm tra thẻ và thử lại";

    notificationStartTime = millis();
    notificationTime = 1000;
    notificationActive = true;

    Serial.println("Thẻ không tồn tại");
  }

  else if (message.startsWith("ReadyToExtend")) {
    notificationColor = green;
    notificationMain = "Đang chờ quét thẻ";
    notificationSub = "Hãy quét thẻ để lấy thông tin";

    Serial.println("Đã sẵn sàng gia hạn");
  }

  else if (message.startsWith("ExtendFirstScanDone")) {
    notificationColor = green;
    notificationMain = "Hãy quét lại thẻ";
    notificationSub = "Quét lại thẻ để gia hạn";

    Serial.println("Quét lại thẻ lần nữa");
  }

  else if (message.startsWith("ReadyToDelete")) {
    notificationColor = red;
    notificationMain = "Đang chờ quét thẻ";
    notificationSub = "Hãy quét thẻ để lấy thông tin";

    Serial.println("Đã sẵn sàng xóa thẻ");
  }

  else if (message.startsWith("DeleteFirstScanDone")) {
    notificationColor = red;
    notificationMain = "Hãy quét lại thẻ";
    notificationSub = "Quét lại thẻ để xóa";

    Serial.println("Quét lại thẻ lần nữa");
  }

  else if (message.startsWith("Delete:")) {
    int index1 = message.indexOf(',');

    ownerReceive = message.substring(7, index1);
    numberReceive = message.substring(index1+1);

    notificationStartTime = millis();
    notificationTime = 500;
    notificationActive = true;

    writeToSheet("Database", "/", ownerReceive, numberReceive, "");

    Serial.println("Nhận dữ liệu từ esp8266 (Delete):");
    Serial.println("Chủ sở hữu: " + ownerReceive);
    Serial.println("Biển số xe: " + numberReceive);
    Serial.println();
  }

  else if (message.startsWith("RestartSystem")) {
    Serial.println("Khởi động lại hệ thống");
    ESP.restart();
  }

  else if (message.startsWith("ResetSystem")) {
    status = "Đang chờ";

    notificationMain = "";
    notificationSub = "";
    
    numberSend = "";
    ownerSend = "";
    timestampSend = "";

    numberReceive = "Chưa xác định";
    ownerReceive = "Chưa xác định";
    timestampReceive = "";
    expirationDateReceive = "";

    Serial.println("Đã đặt lại hệ thống");
  }

  else if (message.startsWith("Log:")) {
    int index1 = message.indexOf(',');
    int index2 = message.indexOf(',', index1 + 1);
    int index3 = message.indexOf(',', index2 + 1);

    numberReceive = message.substring(4, index1);
    ownerReceive = message.substring(index1 + 1, index2);
    timestampReceive = message.substring(index2 + 1, index3);
    String expired = message.substring(index3 + 1);

    if (expired == "1") {
      statusColor = red;
      status = "Thẻ đã hết hạn";
    } else {
        statusColor = black;
        status = "Đang chờ";
      }

    logStartTime = millis();
    logTime = 2000;
    logActive = true;

    Serial.println("Nhận dữ liệu từ esp8266 (Log):");
    Serial.println("Biển số xe: " + numberReceive);
    Serial.println("Chủ sở hữu: " + ownerReceive);
    Serial.println("Tên ảnh: " + timestampReceive + ".jpg");
    Serial.println();
  }

  else if (message.startsWith("Database:")) {
    int index1 = message.indexOf(',');
    int index2 = message.indexOf(',', index1 + 1);
    int index3 = message.indexOf(',', index2 + 1);

    String sign = message.substring(9, index1);
    ownerReceive = message.substring(index1 + 1, index2);
    numberReceive = message.substring(index2 + 1, index3);
    expirationDateReceive = message.substring(index3 + 1);
    
    notificationColor = green;

    if (sign == "?") {
      notificationMain = "Đã gia hạn thành công";
      notificationSub = "Gia hạn thẻ thành công";
    } else {
        notificationMain = "Hoàn tất";
        notificationSub = "Đã thêm người dùng mới thành công";
      }
            
    notificationStartTime = millis();
    notificationTime = 500;
    notificationActive = true;

    writeToSheet("Database", sign, ownerReceive, numberReceive, expirationDateReceive);
            
    Serial.println("Nhận từ esp8266 (Database):");
    Serial.println("Dấu: " + sign);
    Serial.println("Biển số xe: " + numberReceive);
    Serial.println("Chủ sở hữu: " + ownerReceive);
    Serial.println("Ngày hết hạn: " + expirationDateReceive);
  }
}

void onWebSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("Đã kết nối vào WebSocket");
      break;
    case WStype_DISCONNECTED:
      Serial.println("Đã ngắt kết nối với WebSocket");
      break;
    case WStype_TEXT:
      handleWebSocketMessage(NULL, payload, length);
      break;
  }
}

static const char PROGMEM INDEX_HTML[] = R"rawliteral(

<!DOCTYPE HTML><html>
<head>
  <title>Parking Management</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="preconnect" href="https://fonts.gstatic.com">
  <link href="https://fonts.googleapis.com/css2?family=Roboto&display=swap" rel="stylesheet">
</head>

<style>
  body {font-family: 'Montserrat', sans-serif; background-color: #f4f4f4;}
  .notification {display:flex; width: 440px; margin: auto auto; border: none; border-radius: 10px; transition: all 0.3s ease; box-shadow: 0px 0px 20px rgba(0,0,0,0.2);}
  .mainObj {display: flex; flex-direction: column;  margin: -8px auto auto auto; width: 780px; padding: 20px; box-shadow: 0px 0px 20px rgba(0,0,0,0.2); background-color: #fff; border-radius: 5px;}
  .mainObj > div {margin: 4px auto; padding: 4px;}
  .highContainer {display: flex; flex-direction: row; background-color: #fff;}
  .highContainer > div {padding: 0px 40px 0px 0px; margin: 0px auto;}
  .highRightContainer {text-align: center;}
  button {background-color: #fff; margin: 10px auto auto auto; padding: 10px; color: #000; border: solid 2px #4CAF50; border-radius: 5px; cursor: pointer;}
  button:hover {background-color: #45a049; transform: scale(1.1); color: #fff; font-weight: bold;}
  .redBtn {border: solid 2px #FF6060;}
  .redBtn:hover {background-color: #FF6060;}
  .restartContainer {position: relative; width: 40px; height: 40px;}
  .restart {position: absolute; top: 0; left: 0; width: 100%; height: 100%; border: solid 2px #FF6060; background-color: transparent; cursor: pointer;}
  .restart:hover {background-color: #fff;}
  .restartLabel {position: absolute; top: 12px; left: 2px; width: 36px; height: 36px; border: none; border-radius: 5px; pointer-events: none;}
  .lowContainer {display: flex; flex-direction: row; background-color: #fff;}
  .lowContainer > div {padding: 0px 40px 0px 0px; margin: 0px auto;}
  img {width: 352px; height: 288px; background-color: #fff;}
  .LRContainer {display: flex; flex-direction: row; margin-top: 28px;}
  .flashContainer {display: flex; flex-direction: column; align-items: center; position: absolute; top: 10px; right: 10px; }
  .flashLabel img {width: 40px; height: 40px; cursor: pointer; border-radius: 10px; transition: transform 0.3s, box-shadow 0.3s; }
  .flashLabel img:hover {transform: scale(1.1); box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
  .barContainer {display: flex; margin-top: 0px; padding: 10px; background-color: #fff; border-radius: 10px; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1); transition: opacity 0.3s, transform 0.3s; }
  .flashBar {width: 4px; height: 140px; -webkit-appearance: slider-vertical; }
  .optionsContainer {display: flex; flex-direction: column; position: absolute; text-align: center; top: 10px; left: 10px; }
  .optionsLabel{width: 40px; height: 40px; background-color: #4CAF50; cursor: pointer; border-radius: 10px; transition: transform 0.3s, box-shadow 0.3s; }
  .optionsLabel img {width: 24px; height: 24px; position: relative; top: 8px; background-color: #4CAF50; }
  .optionsLabel:hover {transform: scale(1.1); box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
  .featureContainer {display: flex; flex-direction: column; background:transparent; border-radius: 10px; transition: opacity 0.3s, transform 0.3s;}
  .featureButton {width: 140px;}
  .selectionContainer {display: flex; width: 120px; flex-direction: row; justify-content: flex-end; padding: 10px; background:transparent; border-radius: 8px; transition: opacity 0.3s, transform 0.3s;}
  .hidden {display: none;}
</style>

<body>
  <iframe id="notification" class="notification hidden"> </iframe>

  <div class="LRContainer">
    <div class="optionsContainer">
      <input type="checkbox" class="hidden" id="optionsCheckbox" onchange="showOptions(this)">
      <label class="optionsLabel" for="optionsCheckbox">
          <img src="https://firebasestorage.googleapis.com/v0/b/accesscontrolsystem-4f265.appspot.com/o/src%2Foptions.png?alt=media">
      </label>
      <div class="featureContainer hidden" id="featureContainer">
        <button id="extend" class="featureButton">Gia hạn thẻ</button>
        <button id="delete" class="featureButton redBtn">Xóa thẻ</button>
        <button id="reset" class="featureButton">Về chế độ đọc</button>
        <div class="restartContainer">
          <button id="restart" class="restart"></button>
          <img class="restartLabel" src="https://firebasestorage.googleapis.com/v0/b/accesscontrolsystem-4f265.appspot.com/o/src%2Frestart.gif?alt=media"/>
        </div>
        <div class="selectionContainer hidden" id="selectionContainer">
          <button id="confirm" class="confirm redBtn">Đồng ý</button>
          <button id="cancel" class="cancel">Hủy</button>
        </div>
      </div>
    </div>
    <div class="flashContainer">
      <input type="checkbox" class="hidden" id="barCheckbox" onchange="showBar(this)">
      <label class="flashLabel" for="barCheckbox">
          <img src="https://firebasestorage.googleapis.com/v0/b/accesscontrolsystem-4f265.appspot.com/o/src%2Flight.jpg?alt=media">
      </label>
      <div class="barContainer hidden" id="barContainer">
          <input type="range" class="flashBar" id="flash" min="0" max="255" value="0" orient="vertical">
      </div>
    </div>
  </div>

  <div class="mainObj" id="mainObjContainer">
    <div class="highContainer">
      <div>
        <img id="stream" src="" />
      </div>
      <div class="highRightContainer" id="highRightContainer">
        <iframe id="result" style="border:none; width:320px; height:240px;"> </iframe>
        <div class="feature">
          <button id="startStream">Phát</button>
          <button id="recognizeNumber" class="hidden"></button>
          <button id="getStill" class="hidden"></button>
          <button id="saveImage" class="hidden">Lưu</button>
          <button id="stopStream">Dừng</button>
          <button id="showBoard">Hiển thị</button>
          <button id="notify" class="hidden"></button>
        </div>
      </div>
    </div>
    <div class="lowContainer">
      <div>
        <img id="stillImage" src="" />
      </div>
      <div>
        <iframe id="infoIframe" style="border:none; width:320px; height:288px;"></iframe>
      </div>
    </div>
  </div>
    <iframe class="hidden" id="ifr"></iframe>
</body>
<script>

document.addEventListener('DOMContentLoaded', function (event) {

  var baseHost = document.location.origin;
  var streamState = false;

  const streamContainer = document.getElementById('stream');
  const stillContainer = document.getElementById('stillImage');

  const stillBtn = document.getElementById('getStill');
  const streamBtn = document.getElementById('startStream');
  const recognizeBtn = document.getElementById('recognizeNumber');
  const stopBtn = document.getElementById('stopStream');
  const saveBtn = document.getElementById('saveImage');
  const showBoardBtn = document.getElementById('showBoard');
  const notifyBtn = document.getElementById('notify');

  const infoIfr = document.getElementById('infoIframe');
  const notifyIfr = document.getElementById('notification');
  const resultIfr = document.getElementById('result');
  const ifr = document.getElementById('ifr');

  const extendBtn = document.getElementById('extend');
  const deleteBtn = document.getElementById('delete');
  const resetBtn = document.getElementById('reset');
  const restartBtn = document.getElementById('restart');

  const barContainer = document.getElementById('barContainer');
  const barCheckbox = document.getElementById('barCheckbox');
  const flash = document.getElementById('flash');

  const optionsCheckbox = document.getElementById('optionsCheckbox');
  const featureContainer = document.getElementById('featureContainer');

  function showOptions(checkbox) {
    if (checkbox.checked) {
      featureContainer.classList.remove('hidden');
    } else {
        featureContainer.classList.add('hidden');
      }
  }
  
  function uncheckOptions() {
    optionsCheckbox.checked = false;
    showOptions(optionsCheckbox);
  }

  optionsCheckbox.onchange = () => showOptions(optionsCheckbox);

  const selectionContainer = document.getElementById('selectionContainer');
  const confirmBtn = document.getElementById('confirm');
  const cancelBtn = document.getElementById('cancel');

  function showSelection() {
    selectionContainer.classList.remove('hidden');
  }

  function hideSelection() {
    selectionContainer.classList.add('hidden');
  }

  barCheckbox.onchange = function () {
    if (barCheckbox.checked) {
      barContainer.classList.remove('hidden');
    } else {
        barContainer.classList.add('hidden');
      }
  };

  // stream bằng cách still liên tục
  streamBtn.onclick = function (event) {
    streamState = true;
    streamContainer.src = baseHost +'/?getstill=' + Math.random();
  };

  streamContainer.onload = function (event) {
    if (!streamState) return;
    streamBtn.click();
  };

  function updateValue(flash) {
    let value;
    value = flash.value;
    var query = `${baseHost}/?flash=${value}`;
    fetch(query)
  }

  flash.onchange = () => updateValue(flash);

  recognizeBtn.onclick = () => {
    ifr.src = `${baseHost}/?recognizenumber=${Date.now()}`;
  };

  showBoardBtn.onclick = function (event) {
    infoIfr.src = baseHost + '?showboard';
  };

  showBoardBtn.click();

  notifyBtn.onclick = function (event) {
    updateNotification(); // kiểm tra nội dung và cập nhật trạng thái cho iframe thông báo
    notifyIfr.src = baseHost + '?notify';
  };

  function checkAndClick() {
    if (notifyIfr.classList.contains('hidden')) {
      notifyBtn.click();
      setTimeout(checkAndClick, 500);
    } else {
        notifyBtn.click();
      }
  }

  extendBtn.onclick = function(event) {
    socket.send("ExtendExpirationDate");
    uncheckOptions();
  }

  deleteBtn.onclick = function(event) {
    socket.send("DeleteUser");
    uncheckOptions();
  }

  resetBtn.onclick = function(event) {
    socket.send("ResetSystem");
    uncheckOptions();
  }

  restartBtn.onclick = function(event) {
    showSelection();
    confirmBtn.onclick = function(event) {
      socket.send("RestartSystem");
      hideSelection();
      uncheckOptions();
    };
    cancelBtn.onclick = function(event) {
      hideSelection();
      uncheckOptions();
    }
  };

  function updateNotification() {
    const iframeDoc = notifyIfr.contentDocument;

    const h2Element = iframeDoc?.querySelector('h2#title');
    const h3Element = iframeDoc?.querySelector('h3#content');

    const h2Empty = !h2Element || h2Element.textContent.trim() === "";
    const h3Empty = !h3Element || h3Element.textContent.trim() === "";
    
    if (!h2Empty && !h3Empty) {
      notifyIfr.classList.remove('hidden');
    } else {
        notifyIfr.classList.add('hidden');
      }
  }

  let originalHostname = window.location.hostname;

  let ipParts = originalHostname.split('.');

  if (ipParts.length === 4) {
    ipParts[3] = '100';
    let esp8266LocalIP = ipParts.join('.');
    ipParts[3] = '50';
    let serverIP = ipParts.join('.');

    resultIfr.src = 'http://' + serverIP + ':3000/result';

    var socket = new WebSocket('ws://' + esp8266LocalIP + ':81');

    socket.onopen = function() {
      console.log('WebSocket connected successfully');
    };

    socket.onmessage = function(event) {
        console.log(event.data);
        if ((event.data.includes('Database'))||(event.data == 'ReceiveDatabaseSuccessfully')||(event.data == 'InvalidInformation')||(event.data == 'ErrorFound')||(event.data == 'ReadyToExtend')||(event.data.includes('FirstScanDone'))||(event.data == 'ReadyToDelete')) {
          checkAndClick();
        } else if (event.data == 'HideNotification') {
            notifyIfr.classList.add('hidden');
          } else if (event.data == 'ShowInformation') {
              showBoardBtn.click();
              setTimeout(() => {
                recognizeBtn.click();
              },1000);
            } else if (event.data == 'ResetSystem') {
                location.reload();
              }
    };
  }
    
  stillBtn.onclick = () => {
    stillContainer.src = `${baseHost}/?getstill=${Date.now()}`;
  };

  saveBtn.onclick = function (event) { 
    const now = new Date();
    const timestampTemp = (now.getFullYear() * 10000000000 + (now.getMonth() + 1) * 100000000 + now.getDate() * 1000000 + now.getHours() * 10000 + now.getMinutes() * 100 + now.getSeconds()).toString();
    sessionStorage.setItem('timestampValue', timestampTemp);
    
    stillBtn.click();

    ifr.src = baseHost + '?saveimage=' + timestampTemp;
  }; 

  stopBtn.onclick = function (event) { 
    streamState=false;    
    window.stop();
  };
});
  
</script>
</body>
</html>)rawliteral";

void mainpage() {
  String Data="";

  if (cmd!="")
    Data = Feedback;
  else
    Data = String((const char *)INDEX_HTML);
  
  // tách dữ liệu ra để in lần lượt
  for (int Index = 0; Index < Data.length(); Index = Index+1024) {
    client.print(Data.substring(Index, Index+1024));
  } 
  //--------------------------------
}

camera_fb_t * last_fb = NULL;

// giữ lại khung ảnh
void getStill() {
  
  if (last_fb != NULL) {
    esp_camera_fb_return(last_fb);
  }

  last_fb = esp_camera_fb_get();
  
  if (!last_fb) {
    Serial.println("Lỗi fb");
    error();
  }
  
  // in dữ liệu ảnh từ buffer lên webserver
  uint8_t *fbBuf = last_fb->buf;
  size_t fbLen = last_fb->len;

  for (size_t n = 0; n < fbLen; n += 1024) {
    if (n + 1024 < fbLen) {
      client.write(fbBuf, 1024);
      fbBuf += 1024;
    } else if (fbLen % 1024 > 0) {
      size_t remainder = fbLen % 1024;
      client.write(fbBuf, remainder);
    }
  }

  // cập nhật giá trị đèn flash
  ledcAttachChannel(4, 5000, 8, 4);
  ledcWrite(4, flashValue);  
}

// gửi ảnh lên server nhận diện biển số xe
void recognizeNumber() {
  if (last_fb != NULL) {
    esp_camera_fb_return(last_fb);
  }

  last_fb = esp_camera_fb_get();
  
  if (!last_fb) {
    Serial.println("Lỗi fb");
    error();
  }
  
  uint8_t *fbBuf = last_fb->buf;
  size_t fbLen = last_fb->len;
  String imageData = "";
  
  for (size_t n = 0; n < fbLen; n += 1024) {
    if (n + 1024 < fbLen) {
      imageData += String((char*)fbBuf, 1024);
      fbBuf += 1024;
    } else if (fbLen % 1024 > 0) {
      size_t remainder = fbLen % 1024;
      imageData += String((char*)fbBuf, remainder);
    }
  }

  String urlNode = "http://" + String(serverIP) + ":3000/upload";
  
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String head = 
    "--" + boundary + "\r\n" + 
    "Content-Disposition: form-data; name=\"image\"; filename=\"image.jpg\"\r\n" + 
    "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  int contentLength = head.length() + imageData.length() + tail.length();

  HTTPClient http;
  http.begin(urlNode);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Content-Length", String(contentLength));

  int httpResponseCode = http.sendRequest("POST", head + imageData + tail);

  if (httpResponseCode > 0) {
    Serial.println("Tải lên thành công");
    String response = http.getString();
  } else {
      Serial.println("Tải lên thất bại");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println(response);
    }

  http.end();

  // cập nhật giá trị đèn flash
  ledcAttachChannel(4, 5000, 8, 4);
  ledcWrite(4, flashValue);  
}

// tải ảnh lên Firebase Storage
void postImage(String filename) {

  String urlFirebase = "https://firebasestorage.googleapis.com/v0/b/" + FIREBASE_STORAGE_BUCKET + "/o?name=licenseplate/" + pointer + ".jpg&uploadType=media";

  HTTPClient http;
  http.begin(urlFirebase);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(last_fb->buf, last_fb->len);

  if (httpResponseCode > 0) {
    Serial.println("Tải lên thành công");
    String response = http.getString();
    Serial.println(response);
  } else {
      Serial.println("Tải lên thất bại");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println(response);
    }

  http.end();

  // cập nhật giá trị đèn flash
  ledcAttachChannel(4, 5000, 8, 4);
  ledcWrite(4, flashValue);  
}

// giải mã dữ liệu nhận từ webserver
String urlDecode(const String &str) {
  String decoded = "";
  for (int i = 0; i < str.length(); ++i) {
    if (str[i] == '%') {
      if (i + 2 < str.length()) {
        int value;
        sscanf(str.substring(i + 1, i + 3).c_str(), "%x", &value);
        decoded += (char)value;
        i += 2;
      }
    } else if (str[i] == '+') {
        decoded += ' ';
      } else {
          decoded += str[i];
        }
  }
  return decoded;
}

// gửi thông tin người dùng cho esp8266
void sendInfo(String encodeQuery) {
  String decodedQuery = urlDecode(encodeQuery);

  // tách chuỗi thông tin thành từng phần riêng biệt
  int start = 0;
  int end = decodedQuery.indexOf('&');
  while (end >= 0) {
    String pair = decodedQuery.substring(start, end);
    int pos = pair.indexOf('=');
    if (pos >= 0) {
      String key = pair.substring(0, pos);
      String value = pair.substring(pos + 1);
      if (key == "number") {
        numberSend = value;
      } else if (key == "owner") {
          ownerSend = value;
        } else if (key == "timestamp") {
            timestampSend = value;
          }
    }
    start = end + 1;
    end = decodedQuery.indexOf('&', start);
  }

  String pair = decodedQuery.substring(start);
    int pos = pair.indexOf('=');
    if (pos >= 0) {
      String key = pair.substring(0, pos);
      String value = pair.substring(pos + 1);
    if (key == "number") {
        numberSend = value;
      } else if (key == "owner") {
          ownerSend = value;
        } else if (key == "timestamp") {
            timestampSend = value;
          }
  }

  String message = "Database:" + numberSend + "," + ownerSend + "," + timestampSend;
  webSocket.sendTXT(message);

  Serial.println("Đã nhận thông tin:");
  Serial.println("Biển số xe: " + numberSend);
  Serial.println("Chủ sở hữu: " + ownerSend);
  Serial.println("Tên ảnh: " + timestampSend);
}

// bảng hiển thị thông tin
String showBoard() {
  String css =
    "<style>"
      "body {font-family: 'Montserrat', sans-serif; background-color: #fff; margin: 0; padding: 0; overflow: hidden;}"
      ".switch {position: relative; display: inline-block; width: 80px; height: 40px} "
      ".switch input {display: none}"       
      ".slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 25px}"
      ".slider:before {position: absolute; content: ''; height: 34px; width: 38.5px; left: 3px; bottom: 3px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 20px}"
      "input:checked+.slider {background-color: #45a049}"
      "input:checked+.slider:before {-webkit-transform: translateX(35.5px); -ms-transform: translateX(35.5px); transform: translateX(35.5px)}"
      ".showInfo {width: 100%;}"
      "h3 {font-weight: bold; margin: 8px 4px -12px 4px; font-size: 15px;}"
      "h2 {color: #4CAF50; font-weight: bolder; margin: 8px 4px -12px 4px; font-size: 25px; word-wrap: break-word; word-break: break-word;}"
      "hr {margin: 20px 0px 10px 0px;}"
      ".status {width: 100%; background-color: #fff; color:" + statusColor + "; text-align: center; font-size: 18px; font-weight: bolder;}"
      "input[type='text'] {width: 100%; padding: 10px; background-color: #fff; border: 1px solid #ccc; box-sizing: border-box; border-radius: 4px; cursor: pointer; font-size: 20px; font-weight: bolder;}"
      "input[type='text']:focus {outline: none; border-color: #4CAF50; box-shadow: 0 0 5px #4CAF50;}"
      "input::placeholder {color: #cccccc; opacity: 0.8;}"
      ".submit {width: 100%; padding: 10px; margin-top: 24px; background-color: #fff; border: 2px solid #4CAF50; color: #4CAF50; box-sizing: border-box; border-radius: 4px; cursor: pointer; font-size: 15px;}"
      ".submit:hover {background-color: #4CAF50; color: #fff;}"
      ".hidden {display: none;}"
    "</style>";

  String body =
    "<label class='switch'><input type='checkbox' id='toggleSwitch' onchange='changeMode(this)'>"
      "<span class='slider'></span>"
    "</label>"
    "<div class='showInfo' id='infoBoard'>"
      "<h3> Biển số: </h3>"
      "<br>"
      "<h2 id='showNumber'>" + numberReceive + "</h2>"
      "<br>"
      "<h3> Chủ sở hữu: </h3>"
      "<br>"
      "<h2 id='showOwner'>" + ownerReceive + "</h2>"
      "<hr>"
      "<div class='status'>" + status + "</div>"
    "</div>"
    "<form class='hidden' id='infoForm'>"
      "<h3> Biển số: </h3>"
      "<br>"
      "<input id='number' type='text' name='number' autocomplete='off' placeholder='VD: 62M1-12345'>"
      "<br>"
      "<h3> Chủ sở hữu: </h3>"
      "<br>"
      "<input id='owner' type='text' name='owner' autocomplete='off'>"
      "<br>"
      "<input class='submit' type='submit' value='Xác nhận'>"
    "</form>";

  String jvscript =
    "<script>"
      "document.addEventListener('DOMContentLoaded', function (event) {"
        "const plateImage = parent.document.getElementById('stillImage');"
        "plateImage.src = 'https://firebasestorage.googleapis.com/v0/b/" + FIREBASE_STORAGE_BUCKET + "/o/licenseplate%2F" + timestampReceive + ".jpg?alt=media';"
      "});"
      
      "const form = document.getElementById('infoForm');"

      "function changeMode(checkbox) {"
        "const form = document.getElementById('infoForm');"
        "const board = document.getElementById('infoBoard');"
        "if (checkbox.checked) {"
          "form.classList.remove('hidden');"
          "board.classList.add('hidden');"
        "} else {"
            "form.classList.add('hidden');"
            "board.classList.remove('hidden');"
          "}"
      "}"

      "form.addEventListener('submit', function(event) {"
        "event.preventDefault();"

        "const number = document.getElementById('number').value;"
        "const owner = document.getElementById('owner').value;"
        //---------------------------------

        "parent.document.getElementById('saveImage').click();"

       "setTimeout(() => {"
          "var timestamp = sessionStorage.getItem('timestampValue');"
          "parent.document.getElementById('ifr').src = parent.document.location.origin+'?getinfo=' + 'number=' + encodeURIComponent(number) + '&owner=' + encodeURIComponent(owner) + '&timestamp=' + encodeURIComponent(timestamp);"
          "location.reload();"
        "}, 2000);"
      "});"
    "</script>";

  String board = css + body + jvscript;

  return board;
}
//-----------------------

// lấy định dạng ngày tháng năm month day, year
void getCurrentTime() {
  timeClient.update();

  time_t epochTime = timeClient.getEpochTime();
  currentTime = timeClient.getFormattedTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  String currentMonthName = months[currentMonth-1];
  int currentYear = ptm->tm_year+1900;
  currentDate = currentMonthName + " " + String(monthDay) + ", " + String(currentYear);
}

// đọc chuỗi ký tự để phân tích ra câu lệnh
void getCommand(char c) {
  if (c == '?') receiveState = 1;
  if ((c == ' ')||(c == '\r')||(c == '\n')) receiveState = 0;
  
  if (receiveState == 1)
  {
    Command = Command + String(c);
    
    if (c == '=') cmdState = 0;
    if (c == ';') strState++;
  
    if ((cmdState == 1)&&((c != '?')||(questionstate == 1))) cmd = cmd + String(c);
    if ((cmdState == 0)&&(strState == 1)&&((c != '=')||(equalstate == 1))) pointer = pointer + String(c);
    
    if (c == '?') questionstate = 1;
    if (c == '=') equalstate = 1;
    if ((strState >= 9)&&(c == ';')) semicolonstate = 1;
  }
}

// xử lý các câu lệnh
void executeCommand() {
  if ((cmd != "getstill")&&((cmd != "notify"))) {
    Serial.println("cmd = " + cmd + ", pointer = " + pointer);
  }

  if (cmd == "saveimage") { 
    postImage(pointer);
  } else if (cmd == "getinfo") {
      sendInfo(pointer);
    } else if (cmd == "showboard") {
        Feedback = showBoard();
      } else if (cmd == "recognizenumber") {
          recognizeNumber();
        }  else if (cmd == "notify") {
            Feedback = showNotification();
          } else if (cmd == "flash") {
              ledcAttachChannel(4, 5000, 8, 4);
              flashValue = pointer.toInt();
              ledcWrite(4, flashValue);  
            } else Feedback = "Command is not defined.";
  if (Feedback == "") Feedback = Command;  
}

// lắng nghe câu lệnh thông qua HTTP
void listenConnection() {
  Feedback = ""; Command = ""; cmd = ""; pointer = "";
  receiveState = 0, cmdState = 1, strState = 1, questionstate = 0, equalstate = 0, semicolonstate = 0;
  
  client = server.available();

  if (client) { 
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();       
        getCommand(c);
                
        if (c == '\n') {
          if (currentLine.length() == 0) {    
            if (cmd == "getstill") {
              getStill();
            } else mainpage(); 
            Feedback = "";
            break;
          } else {
              currentLine = "";
            }
        } else if (c != '\r') {
            currentLine += c;
          }

        if ((currentLine.indexOf("/?") != -1)&&(currentLine.indexOf(" HTTP") != -1)) {
          if (Command.indexOf("stop") != -1) {
            client.println();
            client.stop();
          }
          currentLine = "";
          Feedback = "";
          executeCommand();
        }
      }
    }
    delay(1);
    client.stop();
  }
}

void loop() {  
  if (notificationActive && (millis() - notificationStartTime >= notificationTime)) {
    notificationMain = "";
    notificationSub = "";
        
    webSocket.sendTXT("HideNotification");

    notificationActive = false;
  }

  if (logActive && (millis() - logStartTime >= logTime)) {
    getCurrentTime();
    writeToSheet("Log", currentDate, currentTime, ownerReceive, numberReceive);
    logActive = false;
  }

  webSocket.loop();
  listenConnection();
}
