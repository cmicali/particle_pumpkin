#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#elif defined(SPARK)
#include "application.h"
#endif

SYSTEM_MODE(MANUAL);

#define RED     A4
#define GREEN   A5
#define BLUE    A6

enum WifiState { WIFI_STATE_NOT_CONNECTED, WIFI_STATE_CONNECTING, WIFI_STATE_AWAITING_PING, WIFI_STATE_CONNECTED, WIFI_STATE_RUNNING };
WifiState wifiState = WIFI_STATE_NOT_CONNECTED;

const size_t UDP_BUFFER_SIZE = 1024;
unsigned int listenPort = 777;
UDP udp;
uint8_t udp_buffer[UDP_BUFFER_SIZE];

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RGB

class RGBLed {
public:

    RGBLed(int redPin, int greenPin, int bluePin) {
        pinR = redPin;
        pinG = greenPin;
        pinB = bluePin;
        r = g = b = 0;
        tr = tg = tb = 0;
        xr = xg = xb = 0;
    }

    void loop() {
        if (isFading()) {
            long pos = millis() - fadeStartTime;
            uint8_t nr = (uint8_t) (r + pos * (tr - r) / fadeTimeMs);
            uint8_t ng = (uint8_t) (g + pos * (tg - g) / fadeTimeMs);
            uint8_t nb = (uint8_t) (b + pos * (tb - b) / fadeTimeMs);
            if (nr != xr || ng != xg || nb != xb) {
                xr = nr;
                xg = ng;
                xb = nb;
                setColor(xr, xg, xb);
            }
            if (xr == tr && xg == tg && xb == tb) {
                r = tr;
                g = tg;
                b = tb;
            }
        }
    }

    void fadeToColor(uint8_t r, uint8_t g, uint8_t b, uint32_t msec) {
        tr = r;
        tg = g;
        tb = b;
        if (!isFading()) {
            xr = this->r;
            xg = this->g;
            xb = this->b;
        }
        else {
            this->r = xr;
            this->g = xg;
            this->b = xb;
        }
        fadeStartTime = millis();
        fadeTimeMs = msec;
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        analogWrite(pinR, r);
        analogWrite(pinG, g);
        analogWrite(pinB, b);
    }

    int isFading() {
        return (r != tr) || (g != tg) || (b != tb);
    }

private:
    long fadeStartTime, fadeTimeMs;
    uint8_t r, g, b;
    uint8_t xr, xg, xb, tr, tg, tb;
    int pinR, pinG, pinB;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Multicast Announce

char binaryDeviceID[12];
const char SKIP_FROM_9_TO_A = 'a' - '9' - 1;

int presence_announcement_coap(unsigned char *buf, const char *id)
{
    buf[0] = 0x50; // Confirmable, no token
    buf[1] = 0x02; // Code POST
    buf[2] = 0x00; // message id ignorable in this context
    buf[3] = 0x00;
    buf[4] = 0xb1; // Uri-Path option of length 1
    buf[5] = 'h';
    buf[6] = 0xff; // payload marker

    memcpy(buf + 7, id, 12);

    return 19;
}

void Multicast_Presence_Announcement2() {
    unsigned char buffer[20];
    int size = presence_announcement_coap(buffer, binaryDeviceID);
    udp.beginPacket({ 224, 0, 1, 187 }, 5683);
    udp.write(buffer, size);
    udp.endPacket();
}


char asciiToNibble(char c) {
    char n = c - '0';
    if (n > 9) {
        n -= SKIP_FROM_9_TO_A;
    }
    return n;
}

void convertDeviceIDToBinary() {
    char highNibble, lowNibble;
    String stringID = System.deviceID();
    Serial.println(stringID);

    for (int i = 0; i < 12; i++) {
        highNibble = asciiToNibble(stringID.charAt(2 * i));
        lowNibble = asciiToNibble(stringID.charAt(2 * i + 1));
        binaryDeviceID[i] = highNibble << 4 | lowNibble;
    }
}

RGBLed led = RGBLed(RED, GREEN, BLUE);

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

    convertDeviceIDToBinary();

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
                led.fadeToColor(udp_buffer[1], udp_buffer[2], udp_buffer[3], 300);
                break;
        }
    }
}

static system_tick_t last_broadcast = 0;

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
                wifiState = WIFI_STATE_AWAITING_PING;
            }
            break;
        case WIFI_STATE_AWAITING_PING:
            WiFi.ping(WiFi.gatewayIP());
            wifiState = WIFI_STATE_CONNECTED;
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
            if (millis() - last_broadcast > 60000) {
                Multicast_Presence_Announcement2();
                last_broadcast = millis();
            }
            protocol();
            break;
    }

    led.loop();

}
