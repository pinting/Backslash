#include <SPI.h>
#include <Ethernet.h>

#define SECRET "12345678"
#define REQUEST_SIZE 64
#define BEEP_LOG_SIZE 64
#define BEEP_MIN_DIFF 2000

#define POWER_PIN 6
#define BEEPER_PIN 2
#define STATUS_PIN 4

IPAddress ip(192, 168, 1, 100);
EthernetServer server(80);
EthernetClient client;

unsigned char mac[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
unsigned char keyboard[8] = { 0 };

unsigned int beepLog[BEEP_LOG_SIZE] = { 0 };
unsigned int beepIndex = 0;
unsigned long beepStarted;
unsigned long beepEnded;

void(*reset) (void) = 0;

void pressKey(unsigned char mod, unsigned char key)
{
    keyboard[0] = mod;
    keyboard[2] = key;
    Serial.write(keyboard, 8);
}

void releaseKey(void)
{
    pressKey(0, 0);
}

void checkBeeper(void)
{
    int passed = abs(micros() - beepStarted);

    // Low is needed, because the optocoupler inverts the signal
    if (digitalRead(BEEPER_PIN) == LOW)
    {
        beepStarted = micros();
    }
    else if (beepIndex > 0 && abs(beepStarted - beepEnded) < BEEP_MIN_DIFF)
    {
        beepLog[beepIndex - 1] += passed;
        beepEnded = micros();
    }
    else
    {
        beepLog[beepIndex] = passed;
        beepIndex = (beepIndex + 1 < BEEP_LOG_SIZE) ? beepIndex + 1 : 0;
        beepEnded = micros();
    }
}

void loop(void)
{
    client = server.available();

    if (!client)
    {
        return;
    }

    bool urlReached = false;
    bool newLine = true;

    char url[REQUEST_SIZE] = { 0 };
    unsigned short len = 0;

    // Read the first REQUEST_SIZE part of the request and parse it
    for (unsigned short i = 0; client.connected() && i < REQUEST_SIZE; i++)
    {
        if (!client.available())
        {
            continue;
        }

        char c = client.read();

        if (c == '\n' && newLine)
        {
            break;
        }

        if (c == '\n')
        {
            newLine = true;
        }
        else if (c != '\r')
        {
            newLine = false;
        }

        if (i == 3 && c == ' ')
        {
            urlReached = true;
        }
        else if (c == ' ')
        {
            urlReached = false;
        }
        else if (urlReached)
        {
            url[len++] = c;
        }
    }

    // Check if SECRET is present
    if (strstr(url, SECRET) == NULL)
    {
        client.stop();
        return;
    }

    // Fabricate HTTP header
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println("Access-Control-Allow-Origin: *");
    client.println();
    
    // Status
    if (strstr(url, "status") != NULL)
    {
        client.println(digitalRead(STATUS_PIN) == LOW ? "on" : "off");
    }

    // Force power
    else if (strstr(url, "force") != NULL)
    {
        digitalWrite(POWER_PIN, HIGH);
        delay(2000);
        digitalWrite(POWER_PIN, LOW);
        client.println("done");
    }

    // Power
    else if (strstr(url, "power") != NULL)
    {
        digitalWrite(POWER_PIN, HIGH);
        delay(200);
        digitalWrite(POWER_PIN, LOW);
        client.println("done");
    }

    // Keyboard
    else if (strstr(url, "keyboard") != NULL)
    {
        unsigned char mod = (url[len - 6] - '0') * 100 + (url[len - 5] - '0') * 10 + (url[len - 4] - '0');
        unsigned char key = (url[len - 3] - '0') * 100 + (url[len - 2] - '0') * 10 + (url[len - 1] - '0');

        pressKey(mod, key);
        releaseKey();

        client.println(mod);
        client.println(key);
    }

    // Beeper
    else if (strstr(url, "beeper") != NULL)
    {
        for (unsigned int i = 0; i < BEEP_LOG_SIZE; i++)
        {
            client.print(beepLog[i]);
            client.println(beepIndex == i ? "<" : "");
        }
    }

    // Reset (the Arduino)
    else if (strstr(url, "reset") != NULL)
    {
        reset();
    }

    delay(1);
    client.stop();
}

void setup(void)
{
    pinMode(POWER_PIN, OUTPUT);
    pinMode(BEEPER_PIN, INPUT_PULLUP);
    pinMode(STATUS_PIN, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(BEEPER_PIN), checkBeeper, CHANGE);

    Ethernet.begin(mac, ip);
    server.begin();

    Serial.begin(9600);
    delay(200);
}