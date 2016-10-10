#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#elif defined(SPARK)
#include "application.h"
#endif

SYSTEM_MODE(MANUAL);

#define RED     A4
#define GREEN   A5
#define BLUE    A6

#define FADE_MS     400
#define LISTEN_PORT 777

#define WIFI_SSID           "MicaliWireless"
#define WIFI_PASSPHRASE     "internets"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RGB

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};


class RGBFader {

public:

    RGBFader() {
        color = {0, 0, 0};
        current_color = {0, 0, 0};
        target_color = {0, 0,0 };
    }

    virtual void loop();

    void fadeToColor(uint8_t r, uint8_t g, uint8_t b, uint32_t msec) {
        if (isFading()) {
            color.r = current_color.r;
            color.g = current_color.g;
            color.b = current_color.b;
        }
        else {
            current_color.r = color.r;
            current_color.g = color.g;
            current_color.b = color.b;
        }
        target_color.r = r;
        target_color.g = g;
        target_color.b = b;
        fade_start_time_ms = millis();
        fade_time_ms = msec;
    }

    virtual void setColor(uint8_t r, uint8_t g, uint8_t b);

    int isFading() {
        return (color.r != target_color.r) || (color.g != target_color.g) || (color.b != target_color.b);
    }

protected:

    long fade_start_time_ms, fade_time_ms;
    Color color, current_color, target_color;

    void handleLoop() {
        if (isFading()) {
            long pos = millis() - fade_start_time_ms;

            uint8_t new_r = clamp(color.r + pos * (target_color.r - color.r) / fade_time_ms);
            uint8_t new_g = clamp(color.g + pos * (target_color.g - color.g) / fade_time_ms);
            uint8_t new_b = clamp(color.b + pos * (target_color.b - color.b) / fade_time_ms);

            if (new_r != current_color.r ||
                new_g != current_color.g ||
                new_b != current_color.b) {
                current_color.r = new_r;
                current_color.g = new_g;
                current_color.b = new_b;
                setColor(new_r, new_g, new_b);
            }
            if (current_color.r == target_color.r &&
                current_color.g == target_color.g &&
                current_color.b == target_color.b) {
                color.r = target_color.r;
                color.g = target_color.g;
                color.b = target_color.b;
            }
        }
    }

    long clamp(long value, long min, long max) {
        if (value < min) value = min;
        if (value > max) value = max;
        return value;
    }

    uint8_t clamp(long value) {
        return (uint8_t)clamp(value, 0, 255);
    }

};

void RGBFader::loop() {
    handleLoop();
}
void RGBFader::setColor(uint8_t r, uint8_t g, uint8_t b) {}

class RGBLed : public RGBFader {
public:

    RGBLed(uint8_t red_pin, uint8_t green_pin, uint8_t blue_pin) : RGBFader() {
        pinR = red_pin;
        pinG = green_pin;
        pinB = blue_pin;
    }

    void setup() {
        pinMode(pinR, OUTPUT);
        pinMode(pinG, OUTPUT);
        pinMode(pinB, OUTPUT);
        setColor(0, 0, 0);
    }

    void loop() {
        if (mode == 0) {
            handleLoop();
        }
        else {
            long e = millis() - start_time_ms;
            if (e > random(80)+70) {
                start_time_ms = millis();
                byte r = random(60)+935;
                byte g = r*3/2;
                byte b = r*3/2;
                setColor(r, g, b);
            }
        }
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        analogWrite(pinR, r);
        analogWrite(pinG, g);
        analogWrite(pinB, b);
    }

    void switchMode() {
        if (mode) {
            mode = 0;
        }
        else {
            mode = 1;
        }
    }

protected:
    uint8_t pinR, pinG, pinB;
    long start_time_ms = 0;
    int mode = 0;
    int count = 0;

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Multicast Announce

class MulticastAnnouncer {

public:

    MulticastAnnouncer(UDP *udp) {
        this->udp = udp;
    }

    void setup() {
        convertDeviceIDToBinary();
    }

    void announce() {
        unsigned char buffer[20];
        int size = presence_announcement_coap(buffer, binaryDeviceID);

        // Serial.println("Announcing");
        udp->beginPacket({ 224, 0, 1, 187 }, 5683);
        udp->write(buffer, size);
        udp->endPacket();

    }

protected:

    String deviceId() {
        return System.deviceID();
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
        String stringID = deviceId();
        for (unsigned int i = 0; i < 12; i++) {
            highNibble = asciiToNibble(stringID.charAt(2 * i));
            lowNibble = asciiToNibble(stringID.charAt(2 * i + 1));
            binaryDeviceID[i] = highNibble << 4 | lowNibble;
        }
    }

    int presence_announcement_coap(unsigned char *buf, const char *id)  {
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

    char binaryDeviceID[12];
    const char SKIP_FROM_9_TO_A = 'a' - '9' - 1;
    UDP *udp;

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// WIFI Handler

class WiFiManager {

public:

    enum WifiState {
        WIFI_STATE_NOT_CONNECTED,
        WIFI_STATE_CONNECTING,
        WIFI_STATE_AWAITING_PING,
        WIFI_STATE_CONNECTED,
        WIFI_STATE_RUNNING
    };

    WiFiManager(String ssid, String passphrase) {
        current_wifi_state = WIFI_STATE_NOT_CONNECTED;
        this->ssid = ssid;
        this->passphrase = passphrase;
    }

    void setup() {
        // Serial.println("Connecting WiFi");
        WiFi.on();
        WiFi.clearCredentials();
        WiFi.setCredentials(ssid, passphrase, WPA2);
        onSetup();
    }

    void loop() {
        switch(current_wifi_state) {
            case WIFI_STATE_NOT_CONNECTED:
                // Serial.println("connecting");
                onConnecting();
                WiFi.connect();
                current_wifi_state = WIFI_STATE_CONNECTING;
                break;
            case WIFI_STATE_CONNECTING:
                if (WiFi.ready()) {
                    current_wifi_state = WIFI_STATE_AWAITING_PING;
                }
                break;
            case WIFI_STATE_AWAITING_PING:
                WiFi.ping(WiFi.gatewayIP());
                current_wifi_state = WIFI_STATE_CONNECTED;
                break;
            case WIFI_STATE_CONNECTED:
                onConnected();
                current_wifi_state = WIFI_STATE_RUNNING;
                break;
            case WIFI_STATE_RUNNING:
                if (!WiFi.ready()) {
                    // Serial.println("disconnected during connected state");
                    current_wifi_state = WIFI_STATE_NOT_CONNECTED;
                    break;
                }
                onRunning();
                break;
        }
        onLoop();
    }

    int ready() {
        return current_wifi_state == WIFI_STATE_RUNNING;
    }

protected:

    virtual void onSetup();
    virtual void onLoop();
    virtual void onConnecting();
    virtual void onConnected();
    virtual void onRunning();

    WifiState current_wifi_state;
    String ssid, passphrase;

};

void WiFiManager::onSetup() {}
void WiFiManager::onConnecting() {}
void WiFiManager::onConnected() {}
void WiFiManager::onRunning() {}
void WiFiManager::onLoop() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Listening protocol

class Server : public WiFiManager {

public:

    Server(String ssid, String passphrase, unsigned int listen_port) : WiFiManager(ssid, passphrase) {
        udp = new UDP();
        multicastAnnouncer = new MulticastAnnouncer(udp);
        this->listen_port = listen_port;
    }

    void onSetup() {
        // Serial.println("Server starting");
        RGB.control(true);
        multicastAnnouncer->setup();
        led.setup();
    }

    void onConnecting() {
        RGB.color(0x80, 0x80, 0x80);
    }

    void onConnected() {
        RGB.color(0, 0, 0);
        // Serial.println("Server connected");
        udp->begin(listen_port);

    }

    void onLoop() {
        led.loop();
    }

    void onRunning() {
        if (millis() - last_broadcast > 60000) {
            multicastAnnouncer->announce();
            last_broadcast = millis();
        }
        int count = udp->receivePacket(udp_buffer, UDP_BUFFER_SIZE - 1);
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
                    led.fadeToColor(udp_buffer[1], udp_buffer[2], udp_buffer[3], FADE_MS);
                    break;
                case 'f':
                    led.switchMode();
                    break;
            }
        }
    }

protected:
    UDP *udp;
    MulticastAnnouncer *multicastAnnouncer;

    RGBLed led = RGBLed(RED, GREEN, BLUE);

    unsigned int listen_port;
    const size_t UDP_BUFFER_SIZE = 128;
    uint8_t udp_buffer[128];
    system_tick_t last_broadcast = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main program

Server server = Server(WIFI_SSID, WIFI_PASSPHRASE, LISTEN_PORT);

void setup() {
    server.setup();
}

void loop() {
    server.loop();
}
