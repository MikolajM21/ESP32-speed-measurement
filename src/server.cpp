#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#define LED 2
#define DETECTOR 25

uint8_t broadcastAddress[] = {0x**, 0x**, 0x**, 0x**, 0x**, 0x**};

AsyncWebServer server(80);
esp_now_peer_info_t peerInfo;

typedef struct sectionMessage {
  bool sectionInfo;
} sectionMessage;

sectionMessage serverInfo;
sectionMessage fromClient;

boolean isLaserInterrupted;
boolean isConnectionOk;

unsigned long startCounting = 0;
unsigned long measuredOutcome = 0;

String distance = "100", name = "driver1", reset = "no";

const char* ssid = "************";
const char* password = "**************";
const char* PARAM_DISTANCE = "distance";
const char* PARAM_NAME = "name";
const char* PARAM_BUTTON = "button";
const char* resource = "******************";
const char* tableServer = "maker.ifttt.com";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><style>
html {text-align: center;}
p2 {font-size: 1.5rem;}
.button {
  padding: 15px 50px;
  font-size: 24px;
  text-align: center;
  outline: none;
  color: #fff;
  background-color: #0f8b8d;
  border: none;
  border-radius: 5px;
}
.button:active {
     background-color: #0f8b8d;
     box-shadow: 2 2px #CDCDCD;
     transform: translateY(2px);
}
.state {
  font-size: 1.5rem;
  font-weight: bold;
}
</style>
<meta http-equiv="refresh" content="5">
<title>ESP Web Server</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<h1>Segmental speed measurement</h1>
<p>
<form action="/get">
    Distance (current - %distance%): <input type="number" name="distance" style="width: 3em" value="100" min="1" max="200">
    <label for="meters">m&nbsp;</label>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
<form action="/get">
    Name (current - %name%): <input type="text" name="name" style="width: 7em" value="">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form>
</p>
<p class="state">State: <span id="state">%state%</span></p>
<form action="/get">
<p><button id="button" class="button" name="button" value="yes" onclick="buttonMessage()">Reset</button></p>
</form>
<p2><a href="**************" target="_blank" rel="noopener noreferrer">Results page</a></p2>
<script>
    function submitMessage() {
      alert("Value saved");
      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
    function buttonMessage() {
      alert("Circuit resetted");
      setTimeout(function(){ document.location.reload(false); }, 500); 
      document.getElementById("state").innerHTML = "READY";
     } 
</script></body></html>)rawliteral";

void initWiFi() {
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.begin(ssid, password);
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Error 404: Page not found");
}

String processor(const String& var){
  if(var == "distance"){
    return distance;
  }
  else if(var == "name"){
    return name;
  }
  else if(var == "state"){
    if(serverInfo.sectionInfo){
      return "READY";
    }
    else {
      return "BLOCKED";
    }
  }
  return String();
}

void resetCircuit(){
  serverInfo.sectionInfo = 1;
  esp_now_send(broadcastAddress, (uint8_t *) &serverInfo, sizeof(serverInfo));
  reset = "no";
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&fromClient, incomingData, sizeof(fromClient));
  serverInfo.sectionInfo = fromClient.sectionInfo;
  startCounting = millis();
}

void makeIFTTTRequest(){
  WiFiClient client;
  client.connect(tableServer, 80);
  String jsonObject = String("{\"value1\":\"") + distance + "\",\"value2\":\"" + name
                      + "\",\"value3\":\"" + String(measuredOutcome/1000) + "," + String(measuredOutcome%1000) + "\"}";

  client.println(String("POST ") + resource + " HTTP/1.1");
  client.println(String("Host: ") + tableServer); 
  client.println("Connection: close\r\nContent-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonObject.length());
  client.println();
  client.println(jsonObject);

  while(client.available()){
    Serial.write(client.read());
  }
  client.stop(); 
}

void setup(){
  pinMode(LED,OUTPUT);
  pinMode(DETECTOR,INPUT);
  Serial.begin(115200);
  initWiFi();
  esp_now_init();
  esp_now_register_recv_cb(OnDataRecv);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.ifidx = ESP_IF_WIFI_STA;  
  peerInfo.encrypt = false;  
  esp_now_add_peer(&peerInfo); 
  serverInfo.sectionInfo = true;
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200,"text/html", index_html, processor);
  });
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(request->hasParam(PARAM_BUTTON)) {
      reset = request->getParam(PARAM_BUTTON)->value();
    }
    else if (request->hasParam(PARAM_DISTANCE)) {
      distance = request->getParam(PARAM_DISTANCE)->value();
    }
    else if (request->hasParam(PARAM_NAME)) {
      name = request->getParam(PARAM_NAME)->value();
    }
  });
  server.onNotFound(notFound);
  server.begin();
}
 
void loop(){
  if(reset!="yes"){
    digitalWrite(LED,!serverInfo.sectionInfo);
    if(!serverInfo.sectionInfo) {
      isLaserInterrupted = digitalRead(DETECTOR);
      if(isLaserInterrupted) {
        serverInfo.sectionInfo = true;
        measuredOutcome = millis() - startCounting;
        esp_now_send(broadcastAddress, (uint8_t *) &serverInfo, sizeof(serverInfo));
        makeIFTTTRequest();
      }
    }
  }
  else {
    resetCircuit();
  }
}