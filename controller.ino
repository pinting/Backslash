#include <SPI.h>
#include <Ethernet.h>
#include <limits.h>

#define SECRET "12345678"
#define BUFFER_SIZE 256
#define BEEP_LOG_SIZE 64
#define BEEP_MIN_DIFF 2000
#define DYN_INTERVAL 1000L * 60L * 60L
#define DYN_HOST "example.com"
#define DYN_AUTH "dXNlcm5hbWU6cGFzc3dvcmQ="
#define POWER_PIN 6
#define BEEPER_PIN 2
#define STATUS_PIN 4

unsigned char mac[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
IPAddress ip(192, 168, 0, 100);
EthernetServer server(80);

unsigned int beepLog[BEEP_LOG_SIZE] = { 0 };
unsigned int beepIndex = 0;

void pressKey(unsigned char mod, unsigned char key)
{
    static unsigned char keyboard[8] = { 0 };

    keyboard[0] = mod;
    keyboard[2] = key;

    Serial.write(keyboard, 8);
}

void releaseKey(void)
{
    pressKey(0, 0);
}

String readUrl(EthernetClient &client)
{
    String result;

    bool urlReached = false;
    bool newLine = true;

    // Read the first N chars of the request and parse it
    for (int i = 0; client.connected(); i++)
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

        // Found "GET " part
        if (i >= 3 && c == ' ')
        {
            urlReached = true;
        }
        else if (c == ' ')
        {
            urlReached = false;
        }
        else if (urlReached)
        {
            result += c;
        }
    }

    return result;
}

void checkBeeper(void)
{
    static unsigned long beepStarted = 0;
    static unsigned long beepEnded = 0;

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

String readStatus()
{
    return digitalRead(STATUS_PIN) == LOW ? "1" : "0";
}

String sendForcePower()
{
    digitalWrite(POWER_PIN, HIGH);
    delay(2000);
    digitalWrite(POWER_PIN, LOW);

    return "1";
}

String sendPower()
{
    digitalWrite(POWER_PIN, HIGH);
    delay(200);
    digitalWrite(POWER_PIN, LOW);
    
    return "1";
}

String sendKeyboard(String url)
{
    const int s = url.length();
    const char *b = url.c_str();

    if(b[s - 6] >= '0' && b[s - 6] <= '9' && b[s - 5] >= '0' && b[s - 5] <= '9' && 
        b[s - 4] >= '0' && b[s - 4] <= '9' && b[s - 3] >= '0' && b[s - 3] <= '9' && 
        b[s - 2] >= '0' && b[s - 2] <= '9' && b[s - 1] >= '0' && b[s - 1] <= '9')
    {
        unsigned char mod = (b[s - 6] - '0') * 100 + (b[s - 5] - '0') * 10 + (b[s - 4] - '0');
        unsigned char key = (b[s - 3] - '0') * 100 + (b[s - 2] - '0') * 10 + (b[s - 1] - '0');
    
        pressKey(mod, key);
        releaseKey();

        return "1";
    }

    return "0";
}

String readBeeper()
{
    String result;

    for (unsigned int i = 0; i < BEEP_LOG_SIZE; i++)
    {
        result += beepLog[i];
        result += beepIndex == i ? "<\n" : "\n";
    }

    return result;
}

void registerDyn()
{
    EthernetClient client;

    if (!client.connect("dynupdate.no-ip.com", 80))
    {
        return;
    }

    client.println("GET /nic/update?myip=&hostname=" DYN_HOST " HTTP/1.1");
    client.println("Host: dynupdate.no-ip.com");
    client.println("Authorization: Basic " DYN_AUTH);
    client.println("User-Agent: Backslash github.com/pinting/Backslash");
    client.println();
    client.println();

    delay(1);
    client.stop();
}

void loop(void)
{
    static unsigned long lastDyn = LONG_MAX;

    if(labs(millis() - lastDyn) > DYN_INTERVAL)
    {
        registerDyn();
        lastDyn = millis();
    }

    EthernetClient client = server.available();

    if(!client)
    {
        return;
    }
    
    String url = readUrl(client);

    // Check if SECRET is present
    if (url.indexOf(SECRET) == -1)
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
    if (url.indexOf("status") != -1)
    {
        client.print(readStatus());
    }

    // Force power
    else if (url.indexOf("force") != -1)
    {
        client.print(sendForcePower());
    }

    // Power
    else if (url.indexOf("power") != -1)
    {
        client.print(sendPower());
    }

    // Keyboard
    else if (url.indexOf("keyboard") != -1)
    {
        client.print(sendKeyboard(url));
    }

    // Beeper
    else if (url.indexOf("beeper") != -1)
    {
        client.print(readBeeper());
    }

    delay(1);
    client.stop();
}

void setup(void)
{
    delay(1000);

    pinMode(POWER_PIN, OUTPUT);
    pinMode(BEEPER_PIN, INPUT_PULLUP);
    pinMode(STATUS_PIN, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(BEEPER_PIN), checkBeeper, CHANGE);

    Ethernet.begin(mac, ip);
    server.begin();

    Serial.begin(9600);
    delay(200);
}