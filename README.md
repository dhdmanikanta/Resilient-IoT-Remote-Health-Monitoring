# Resilient IoT for Remote Health Monitoring

An edge-based ESP32 patient monitor that keeps working on weak or intermittent networks. The ESP32 does local acquisition and preprocessing, publishes readings over MQTT, and a Node-RED dashboard shows them live.

> Academic prototype (inter-departmental project, Vignan's Foundation for Science, Technology & Research, Aug–Dec 2025). It is not a clinically validated medical device.

## What it measures

| Signal | Sensor | Rate |
|---|---|---|
| ECG | AD8232 | 250 Hz |
| Heart rate and SpO2 | MAX30102 | 100 samples/s |
| Body temperature | DS18B20 | periodic |
| Motion | PIR sensor | event-based |

## How it works

1. The ESP32 acquires the sensor signals and preprocesses them locally (edge processing).
2. Readings are published over Wi-Fi with MQTT, one topic per signal: `heartrate`, `spo2`, `temperature`, `motion`, `ecg` under `/vignan/patient01/`.
3. If the network drops, readings are buffered locally and the device reconnects automatically.
4. A Node-RED dashboard subscribes to the topics and shows SpO2, temperature, motion status, the ECG waveform and heart rate.

## Screenshots and diagrams

| | |
|---|---|
| ![Node-RED dashboard](images/iot-dashboard.webp) | ![Node-RED flow](images/iot-flow.webp) |
| Node-RED dashboard | Node-RED flow, one MQTT topic per signal |

![Wiring diagram](images/iot-wiring.webp)

Wiring diagram drawn in Cirkit Designer.

## My role

Team lead in a team of 3: system design, sensor integration, MQTT communication and the Node-RED dashboard.

## Limitations

- Prototype only. No clinical validation, and not for diagnosis or treatment.
- The dashboard screenshot is a demonstration view.
- Firmware source and the Node-RED flow export are not yet in this repository and will be added.

## Security note

Never commit Wi-Fi passwords, MQTT broker credentials or tokens. Keep them in a local file that is listed in `.gitignore`.
