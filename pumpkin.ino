#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#elif defined(SPARK)
#include "application.h"
#endif

SYSTEM_MODE(MANUAL);

#define RED     A4
#define GREEN   A5
#define BLUE    A6

enum WifiState { WIFI_STATE_NOT_CONNECTED, WIFI_STATE_CONNECTING, WIFI_STATE_CONNECTED, WIFI_STATE_RUNNING };
WifiState wifiState = WIFI_STATE_NOT_CONNECTED;

const size_t UDP_BUFFER_SIZE = 1024;
unsigned int listenPort = 777;
UDP udp;
uint8_t udp_buffer[UDP_BUFFER_SIZE];

void setup() {
    pinMode(RED, OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(BLUE, OUTPUT);
    analogWrite(RED, 0x00);
    analogWrite(GREEN, 0x00);
    analogWrite(BLUE, 0x00);

    WiFi.on();
    WiFi.clearCredentials();
    WiFi.setCredentials("MicaliWireless", "internets", WPA2);

    RGB.control(true);

}

void protocol() {
    int count = udp.receivePacket(udp_buffer, UDP_BUFFER_SIZE - 1);
    if (count > 0) {
        switch(udp_buffer[0]) {
            case 'r':
                analogWrite(RED, udp_buffer[1]);
                break;
            case 'g':
                analogWrite(GREEN, udp_buffer[1]);
                break;
            case 'b':
                analogWrite(BLUE, udp_buffer[1]);
                break;
            case 't':
                analogWrite(RED, udp_buffer[1]);
                analogWrite(GREEN, udp_buffer[2]);
                analogWrite(BLUE, udp_buffer[3]);
                break;
        }
    }
}

void loop() {

    switch(wifiState) {
        case WIFI_STATE_NOT_CONNECTED:
            Serial.println("connecting");
            RGB.color(0, 0, 0xff);
            WiFi.connect();
            wifiState = WIFI_STATE_CONNECTING;
            break;
        case WIFI_STATE_CONNECTING:
            if (WiFi.ready()) {
                wifiState = WIFI_STATE_CONNECTED;
            }
            break;
        case WIFI_STATE_CONNECTED:
            RGB.color(0, 0, 0);
            udp.begin(listenPort);
            wifiState = WIFI_STATE_RUNNING;
            break;
        case WIFI_STATE_RUNNING:
            if (!WiFi.ready()) {
                Serial.println("disconnected during connected state");
                wifiState = WIFI_STATE_CONNECTING;
                break;
            }
            protocol();
            break;
    }
}
