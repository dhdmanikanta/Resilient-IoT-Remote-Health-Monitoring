/*
  Resilient IoT Remote Health Monitoring - ESP32 firmware
  Sensors : MAX30102 (HR, SpO2), AD8232 (ECG), DS18B20 (temperature), PIR (motion)
  Broker  : public HiveMQ broker over plain MQTT (port 1883) - prototype only
  Notes   : SpO2 uses a simple linear approximation of the red/IR ratio and is
            NOT calibrated; do not use for clinical decisions.
  Replace the Wi-Fi placeholders below before flashing.
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <OneWire.h>
#include <DallasTemperature.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

#define ECG_PIN       36
#define ECG_LO_PLUS   32
#define ECG_LO_MINUS  33
#define PIR_PIN       27
#define TEMP_PIN      4

const char* hr_topic     = "/vignan/patient01/heartrate";
const char* spo2_topic   = "/vignan/patient01/spo2";
const char* temp_topic   = "/vignan/patient01/temperature";
const char* ecg_topic    = "/vignan/patient01/ecg";
const char* motion_topic = "/vignan/patient01/motion";
const char* alert_topic  = "/vignan/patient01/alert";

MAX30105 sensor;

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

long lastBeat = 0;

float beatsPerMinute = 0;
int beatAvg = 0;

const byte RATE_SIZE = 4;

byte rates[RATE_SIZE];
byte rateSpot = 0;

float spo2 = 0.0;

bool fingerPresent = false;

float currentTemp = 0.0;
int currentMotion = 0;
int currentECG = 0;

unsigned long lastPublish = 0;

const unsigned long PIR_LOCKOUT = 60000;

void setup_wifi()
{
    Serial.print("Connecting Wi-Fi");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi Connected");
}

void reconnect()
{
    if (!client.connected())
    {
        Serial.print("Connecting MQTT...");

        if (client.connect("ESP32_Medical_Master_Test123"))
        {
            Serial.println("CONNECTED");
        }
        else
        {
            Serial.print("FAILED : ");
            Serial.println(client.state());
        }
    }
}

void setup()
{
    Serial.begin(115200);

    setup_wifi();

    client.setServer(mqtt_server, mqtt_port);

    pinMode(PIR_PIN, INPUT);

    pinMode(TEMP_PIN, INPUT_PULLUP);

    pinMode(ECG_LO_PLUS, INPUT);
    pinMode(ECG_LO_MINUS, INPUT);

    Wire.begin(21, 22);

    if (!sensor.begin(Wire, I2C_SPEED_FAST))
    {
        Serial.println("MAX30102 NOT DETECTED");

        while (1);
    }

    sensor.setup();

    sensor.setPulseAmplitudeRed(0x1F);
    sensor.setPulseAmplitudeIR(0x1F);

    tempSensor.begin();

    tempSensor.setWaitForConversion(false);

    tempSensor.requestTemperatures();

    Serial.println("System Ready");
}

void loop()
{
    static unsigned long lastReconnectAttempt = 0;

    if (!client.connected())
    {
        if (millis() - lastReconnectAttempt > 5000)
        {
            lastReconnectAttempt = millis();

            reconnect();
        }
    }
    else
    {
        client.loop();
    }

    unsigned long now = millis();

    long irValue = sensor.getIR();

    long redValue = sensor.getRed();

    fingerPresent = (irValue > 50000);

    if (fingerPresent)
    {
        if (checkForBeat(irValue))
        {
            long delta = now - lastBeat;

            lastBeat = now;

            beatsPerMinute = 60 / (delta / 1000.0);

            if (beatsPerMinute > 30 && beatsPerMinute < 220)
            {
                rates[rateSpot++] = (byte)beatsPerMinute;

                rateSpot %= RATE_SIZE;

                beatAvg = 0;

                for (byte x = 0; x < RATE_SIZE; x++)
                {
                    beatAvg += rates[x];
                }

                beatAvg /= RATE_SIZE;
            }
        }

        if (irValue > 0 && redValue > 0)
        {
            float ratio = (float)redValue / (float)irValue;

            spo2 = 104.0 - (17.0 * ratio);

            if (spo2 > 100.0)
            {
                spo2 = 100.0;
            }
        }
    }
    else
    {
        beatAvg = 0;

        spo2 = 0.0;
    }

    if (now - lastPublish > 2000)
    {
        lastPublish = now;

        if (digitalRead(ECG_LO_PLUS) || digitalRead(ECG_LO_MINUS))
        {
            currentECG = 0;
        }
        else
        {
            currentECG = analogRead(ECG_PIN);
        }

        if (now < PIR_LOCKOUT)
        {
            currentMotion = 0;
        }
        else
        {
            currentMotion = digitalRead(PIR_PIN);
        }

        currentTemp = tempSensor.getTempCByIndex(0);

        tempSensor.requestTemperatures();

        if (client.connected())
        {
            client.publish(hr_topic, String(beatAvg).c_str());

            client.publish(spo2_topic, String(spo2, 1).c_str());

            client.publish(temp_topic, String(currentTemp, 1).c_str());

            client.publish(
                motion_topic,
                currentMotion ? "DETECTED" : "CLEAR"
            );

            client.publish(ecg_topic, String(currentECG).c_str());

            if (beatAvg < 50 && beatAvg > 10)
            {
                client.publish(alert_topic, "BRADYCARDIA");
            }
            else if (beatAvg > 120)
            {
                client.publish(alert_topic, "TACHYCARDIA");
            }

            Serial.println("---------------");

            Serial.print("Heart Rate : ");
            Serial.println(beatAvg);

            Serial.print("SpO2 : ");
            Serial.println(spo2);

            Serial.print("Temperature : ");
            Serial.println(currentTemp);

            Serial.print("Motion : ");
            Serial.println(currentMotion ? "DETECTED" : "CLEAR");

            Serial.print("ECG : ");
            Serial.println(currentECG);

            Serial.println("Published");
        }
    }
}
