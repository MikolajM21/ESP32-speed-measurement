#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#define LED 2
#define DETECTOR 25

uint8_t broadcastAddress[] = {0x**, 0x**, 0x**, 0x**, 0x**, 0x**};

esp_now_peer_info_t peerInfo;

typedef struct sectionMessage {
    bool sectionInfo;
} sectionMessage;

sectionMessage client;
sectionMessage fromServer;

boolean isLaserInterrupted;

constexpr char WIFI_SSID[] = "***********";

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i=0; i<n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i);
      }
    }
  }
  return 0;
}

void initWiFi() {
    WiFi.mode(WIFI_MODE_STA);
    int32_t channel = getWiFiChannel(WIFI_SSID);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
}

void initEspNow() {
    esp_now_init();
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.ifidx = ESP_IF_WIFI_STA;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&fromServer, incomingData, sizeof(fromServer));
  client.sectionInfo = fromServer.sectionInfo;
}

void setup(){
  pinMode(LED,OUTPUT);
  pinMode(DETECTOR, INPUT);
  Serial.begin(115200);
  initWiFi();
  initEspNow();
  esp_now_register_recv_cb(OnDataRecv);
  client.sectionInfo = true;
}
 
void loop(){
  digitalWrite(LED,client.sectionInfo);
  if(client.sectionInfo) {
    isLaserInterrupted = digitalRead(DETECTOR);
    if(isLaserInterrupted) {
      client.sectionInfo = false;
      esp_now_send(broadcastAddress, (uint8_t *) &client, sizeof(client));
    }
  }
}
