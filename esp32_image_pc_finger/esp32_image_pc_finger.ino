//#include <SoftwareSerial.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiUdp.h>
#include <fpm.h>
#include <HardwareSerial.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

HardwareSerial fserial(1);
FPM finger(&fserial);
FPMSystemParams params;

const char* ssid = "XXXXX";
const char* password = "XXXXX";

const char * udpServerIp = "192.168.1.6";
const int udpServerPort = 44444;
const int udpPort =88888;
//create UDP instance
WiFiUDP udpClient;

/* for convenience */
#define PRINTF_BUF_SZ   60
char printfBuf[PRINTF_BUF_SZ];

uint8_t buffer1[50];

void readMacAddress(){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}
void setup()
{
    Serial.begin(57600);
    //fserial.begin(57600);
    WiFi.mode(WIFI_STA);
    WiFi.STA.begin();
    Serial.println();
    Serial.println("[DEFAULT] ESP32 Board MAC Address: ");
    readMacAddress();
    WiFi.begin(ssid, password);  
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
    fserial.begin(57600, SERIAL_8N1, 25, 32);
    Serial.println("IMAGE-TO-PC example");
    
    udpClient.begin(udpServerPort);
    if (finger.begin()) {
        finger.readParams(&params);
        Serial.println("Found fingerprint sensor!");
        Serial.print("Capacity: "); 
        Serial.println(params.capacity);
        Serial.print("Packet length: "); 
        Serial.println(FPM::packetLengths[static_cast<uint8_t>(params.packetLen)]);

    } 
    else {
        Serial.println("Did not find fingerprint sensor :(");
        while (1) yield();
    }    
}

void loop()
{

  imageToUdp();
  yield();

}

uint32_t imageToUdp(void)
{
    FPMStatus status;
    
    /* Take a snapshot of the finger */
    Serial.println("\r\nPlace a finger.");
    
    do {
        status = finger.getImage();
        
        switch (status) 
        {
            case FPMStatus::OK:
                Serial.println("Image taken.");
                break;
                
            case FPMStatus::NOFINGER:
                Serial.println(".");
                break;
                
            default:
                /* allow retries even when an error happens */
                snprintf(printfBuf, PRINTF_BUF_SZ, "getImage(): error 0x%X", static_cast<uint16_t>(status));
                Serial.println(printfBuf);
                break;
        }
        
        yield();
    }
    while (status != FPMStatus::OK);
    
    /* Initiate the image transfer */
    status = finger.downloadImage();
    
    switch (status) 
    {
        case FPMStatus::OK:
            Serial.println("Starting image stream...");
            break;
            
        default:
            snprintf(printfBuf, PRINTF_BUF_SZ, "downloadImage(): error 0x%X", static_cast<uint16_t>(status));
            Serial.println(printfBuf);
            return 0;
    }
    uint8_t buffer[50] = "START";
    udpClient.beginPacket(udpServerIp, udpServerPort);
    udpClient.write(buffer, 5);
    udpClient.endPacket();
    uint32_t totalRead = 0;
    uint16_t readLen = 0;
    
    /* Now, the sensor will send us the image from its image buffer, one packet at a time. */
    bool readComplete = false;

    while (!readComplete) 
    {
        /* Start composing a packet to the remote server */
        //udpClient.beginPacket(udpServerIp, udpServerPort);
        udpClient.beginPacket(udpServerIp, udpServerPort);
        bool ret = finger.readDataPacket(NULL, &udpClient, &readLen, &readComplete);
        
        if (!ret)
        {
            snprintf_P(printfBuf, PRINTF_BUF_SZ, PSTR("readDataPacket(): failed after reading %u bytes"), totalRead);
            Serial.println(printfBuf);
            return 0;
        }
        
        /* Complete the packet and send it */
        if (!udpClient.endPacket())
        {
            snprintf_P(printfBuf, PRINTF_BUF_SZ, PSTR("imageToUdp(): failed to send packet, count = %u bytes"), totalRead);
            Serial.println(printfBuf);
            return 0;
        }
        
        totalRead += readLen;
        
        yield();
    }
    uint8_t buffer1[50] = "STOP";
    udpClient.beginPacket(udpServerIp, udpServerPort);
    udpClient.write(buffer1, 4);
    udpClient.endPacket();
    Serial.println();
    Serial.print(totalRead); 
    Serial.println(" bytes transferred.");
    return totalRead;
}

void macaddress_str(int a[], char *buf)
{
    int i;
    for (i = 0; i < 5; i++, buf += 3)
        sprintf(buf, "%02X:", a[i]);
    sprintf(buf, "%02X", a[i]);
}


