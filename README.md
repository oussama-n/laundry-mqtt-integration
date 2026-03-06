# Laundry MQTT Integration

This repository contains the firmware and test environment for integrating **laundry machines with a tablet console application using MQTT**.

The system consists of:

- ESP32 **Machine Controller** (controls the washing machine)
- ESP32 **Coin Collector** (handles coin payments and user interface)
- **Node-RED test environment**
- **Mosquitto MQTT broker**

The goal is to allow the **tablet console application** to communicate with laundry machines through MQTT.

---

# System Architecture

```
Customer
   │
   ▼
Tablet Console App
   │
   │ HTTP API
   ▼
Node-RED
   │
   │ MQTT
   ▼
MQTT Broker (Mosquitto)
   │
   ├──────────────► ESP32 Machine Controller
   │                 (controls machine)
   │
   └──────────────► ESP32 Coin Collector
                     (handles coins & credit)
```

---

# Repository Structure

```
laundry-mqtt-integration
│
├── README.md
│
├── esp32-machine-controller
│   └── main.cpp
│
├── esp32-coin-collector
│   └── main.cpp
│
└── nodered
    └── laundry_test_flow.json
```

---

# Components

## 1️⃣ ESP32 Machine Controller

Controls the washing machine by simulating button presses through optocouplers.

### Functions

- Select machine mode
- Start machine
- Detect machine running state
- Detect door lock state
- Publish machine status via MQTT

### Hardware Pins

| Pin | Function |
|----|----|
| GPIO16 | DEC_OPTO (reset program) |
| GPIO17 | INC_OPTO (increment program) |
| GPIO5 | START_OPTO |
| GPIO15 | INC LED |
| GPIO2 | DEC LED |
| GPIO4 | START LED |
| GPIO18 | Door lock sensor |

---

## 2️⃣ ESP32 Coin Collector

Handles coin input and user interaction.

### Functions

- Detect coin pulses
- Convert pulses to currency
- Display credit on OLED
- Send payment request
- Handle reset commands

### Hardware Pins

| Pin | Function |
|----|----|
| GPIO26 | Left button |
| GPIO25 | Confirm button |
| GPIO33 | Right button |
| GPIO27 | Coin pulse input |
| I2C | OLED Display |

---

# MQTT Topics

Machine ID is dynamic.

```
<MACHINE_ID>
```

Example:

```
laundry/machine/2/status
```

---

# Machine Controller Topics

## Published

Machine → MQTT

```
laundry/machine/<id>/status
laundry/machine/<id>/mode
laundry/machine/<id>/running
laundry/machine/<id>/door
laundry/machine/<id>/event/started
laundry/machine/<id>/event/finished
```

Example

```
laundry/machine/2/running
payload: 1
```

---

## Subscribed

MQTT → Machine

```
laundry/machine/<id>/cmd/setmode
laundry/machine/<id>/cmd/start
laundry/machine/<id>/cmd/reset
```

Example

```
Topic:
laundry/machine/2/cmd/setmode

Payload:
3
```

---

# Coin Collector Topics

## Published

```
laundry/machine/<id>/coin/inserted
laundry/machine/<id>/credit
laundry/machine/<id>/payment/request
laundry/machine/<id>/stats/total
```

Example

```
laundry/machine/1/coin/inserted
payload: 2
```

---

## Subscribed

```
laundry/machine/<id>/cmd/reset
laundry/machine/<id>/cmd/start
```

---

# Node-RED Flow

Node-RED acts as the bridge between:

- MQTT devices
- Tablet console API

The flow performs:

1. Receives coin events
2. Sends payment request to console API
3. Waits for approval
4. Sends machine start command

---

# API Integration

Node-RED sends HTTP requests to the console application.

Example endpoints:

```
POST /api/coin-inserted
POST /api/payment-request
POST /api/machine-finished
```

Payload example:

```
{
  "machineId": "2",
  "amount": 5,
  "timestamp": "2026-02-22T16:30:00Z"
}
```

---

# Quick Test

### 1 Start Mosquitto

```
docker run -d -p 1883:1883 eclipse-mosquitto
```

### 2 Start Node-RED

```
docker run -d -p 1880:1880 nodered/node-red
```

Open

```
http://localhost:1880
```

---

### 3 Import Node-RED Flow

Import:

```
nodered/laundry_test_flow.json
```

---

### 4 Power ESP32 Devices

Both ESP32 boards will automatically connect to:

```
MQTT broker
```

---

### 5 Monitor Topics

Use Node-RED debug panel or:

```
mosquitto_sub -h localhost -t "#" -v
```

Example output

```
laundry/machine/2/status online
laundry/machine/2/mode 1
laundry/machine/2/running 0
laundry/machine/2/door open
```

---

# Requirements

### Hardware

- ESP32 DevKit
- Optocouplers
- Coin acceptor
- OLED display
- Washing machine interface

### Software

- Mosquitto MQTT
- Node-RED
- Arduino / PlatformIO
- Docker (optional)

---

# Notes

- Machine IDs must be unique.
- MQTT broker must be reachable by ESP32 devices.
- Node-RED acts only as a middleware bridge.

---

# Author

Oussama Nordine  
Electrical Engineering Student  
Embedded Systems / IoT Development
