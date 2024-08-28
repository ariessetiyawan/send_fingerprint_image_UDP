#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <FPM.h>
#include <HardwareSerial.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <SPI.h>

HardwareSerial fserial(1);
FPM finger(&fserial);
FPM_System_Params params;

const char * udpServerIp = "192.168.1.7";
const int udpServerPort = 44444;
const int udpPort =88888;
//create UDP instance


/* for convenience */
#define PRINTF_BUF_SZ   60
char printfBuf[PRINTF_BUF_SZ];

byte mac[] = { 0x2C, 0xF7, 0xF1, 0x08, 0x00, 0x9A };

IPAddress local_ip(192, 168, 1, 33);
IPAddress remote_ip(192, 168, 1, 7);

unsigned int local_port = 8888;
unsigned int remote_port = 44444;

EthernetUDP client;
IPAddress ip(192, 168, 1, 45);

// Set the static IP address to use if the DHCP fails to assign
#define MYIPADDR 192,168,1,28
#define MYIPMASK 255,255,255,0
#define MYDNS 192,168,1,1
#define MYGW 192,168,1,1

void setup()
{
  Serial.begin(57600);
  fserial.begin(57600, SERIAL_8N1, 25, 32);
  Serial.println();
  Serial.println("Initialize Ethernet with DHCP :");
  delay(10);
  byte mac_address[6] ={0,};
  w5500.getMACAddress(mac_address);
  for(int i = 0; i < 6; i++) {
    Serial.write(mac_address[i]);
  }

  Ethernet.init(5);

  delay(10);
  int i = 0;
  int DHCP = 0;
  DHCP =Ethernet.begin(mac);
  if (DHCP == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    for (;;)
      ;
    while( DHCP == 0 && i < 30){
      delay(1000);
      DHCP = Ethernet.begin(mac);
      i++;
    }
    if(!DHCP){
      Serial.println("DHCP FAILED");
      for(;;); //Infinite loop because DHCP Failed
    }
    Serial.println("DHCP Success");
    // no point in carrying on, so do nothing forevermore:
  }
  else {
      // print your local IP address:
      Serial.print("IP assigned Ethernet ");
      Serial.println("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
      Serial.print("IP Address        : ");
      Serial.println(Ethernet.localIP());
      Serial.print("Subnet Mask       : ");
      Serial.println(Ethernet.subnetMask());
      Serial.print("Default Gateway IP: ");
      Serial.println(Ethernet.gatewayIP());
      Serial.print("DNS Server IP     : ");
      Serial.println(Ethernet.dnsServerIP());
      Serial.println();
      String imgfilenm=(Ethernet.localIP().toString());
      imgfilenm.replace(".","");
      imgfilenm.replace(".","");
      imgfilenm.replace(".","");
      imgfilenm=imgfilenm+'_';
      Serial.print("Nama file : ");
      Serial.println(imgfilenm);
      delay(500);
      if (finger.begin()) {
        finger.readParams(&params);
        Serial.println("Found fingerprint sensor!");
        Serial.print("Capacity: "); Serial.println(params.capacity);
        Serial.print("Packet length: "); Serial.println(FPM::packet_lengths[params.packet_len]);
      } 
      else {
          Serial.println("Did not find fingerprint sensor :(");
          while (1) yield();
      }    
    
    }

  // print your local IP address:

  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  Serial.println("IMAGE-TO-PC example");
    
    client.begin(remote_port);
    if (finger.begin()) {
        finger.readParams(&params);
        Serial.println("Found fingerprint sensor!");
        Serial.print("Capacity: "); 
        Serial.println(params.capacity);
        Serial.print("Packet length: "); 
        Serial.println(FPM::packet_lengths[params.packet_len]);
    }
    else {
        Serial.println("Did not find fingerprint sensor :(");
        while (1) yield();
    }
}

void loop()
{

  stream_image();
  yield();

}

/* set to the current sensor packet length, 128 by default */
#define TRANSFER_SZ    128
uint8_t buffer[TRANSFER_SZ];

void stream_image(void) {
      if (!set_packet_len_128()) {
        Serial.println("Could not set packet length");
        return;
    }

    delay(100);
    
    int16_t p = -1;
    Serial.println("Waiting for a finger...");
    while (p != FPM_OK) {
        p = finger.getImage();
        switch (p) {
            case FPM_OK:
                Serial.println("Image taken");
                break;
            case FPM_NOFINGER:
                break;
            case FPM_PACKETRECIEVEERR:
                Serial.println("Communication error");
                break;
            case FPM_IMAGEFAIL:
                Serial.println("Imaging error");
                break;
            default:
                Serial.println("Unknown error");
                break;
        }
        yield();
    }

    p = finger.downImage();
    switch (p) {
        case FPM_OK:
            Serial.println("Starting image stream...");
            break;
        case FPM_PACKETRECIEVEERR:
            Serial.println("Communication error");
            return;
        case FPM_UPLOADFAIL:
            Serial.println("Cannot transfer the image");
            return;
    }
    
    /* flag to know when we're done */
    bool read_finished;
    /* indicate the max size to read, and also returns how much was actually read */
	  uint16_t readlen = TRANSFER_SZ;
    uint16_t count = 0;
    
    uint8_t buffer2[50] = "START";
    client.beginPacket(remote_ip, remote_port);
    client.write(buffer2, 5);
    client.endPacket();
    delay(5);
    
    while (true) {
        client.beginPacket(remote_ip, remote_port);
        bool ret = finger.readRaw(FPM_OUTPUT_TO_BUFFER, buffer, &read_finished, &readlen);
        if (ret) {
            count++;
            // we now have a complete packet, so send it 
			      client.write(buffer, readlen);
            client.endPacket();
            
            // indicate the length to be read next time like before 
			      readlen = TRANSFER_SZ;
            if (read_finished)
                break;
        }
        else {
            Serial.print("\r\nError receiving packet ");
            Serial.println(count);
            return;
        }
        yield();
    }
    
    Serial.println();
    int jmlpacket=(count * FPM::packet_lengths[params.packet_len]);
    Serial.print(count * FPM::packet_lengths[params.packet_len]); 
    Serial.println(" bytes read.");
    Serial.println("Image stream complete.");
    if (jmlpacket==36864){
      delay(5);
      uint8_t buffer1[50] = "STOP";
      client.beginPacket(remote_ip, remote_port);
      client.write(buffer1, 4);
      client.endPacket();
    }
}

/* set packet length to 128 bytes,
   no need to call this for R308 sensor */
bool set_packet_len_128(void) {
    uint8_t param = FPM_SETPARAM_PACKET_LEN; // Example
    uint8_t value = FPM_PLEN_128;
    int16_t p = finger.setParam(param, value);
    switch (p) {
        case FPM_OK:
            Serial.println("Packet length set to 128 bytes");
            break;
        case FPM_PACKETRECIEVEERR:
            Serial.println("Comms error");
            break;
        case FPM_INVALIDREG:
            Serial.println("Invalid settings!");
            break;
        default:
            Serial.println("Unknown error");
    }

    return (p == FPM_OK);
}


