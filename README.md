
# Laundry Machine MQTT Integration

ESP32-based system for integrating laundry machines with a central console using **MQTT**.

This project enables a laundromat console application to:

- Accept coin payments
- Select washing programs
- Start machines remotely
- Monitor machine status
- Track machine usage

The system uses **ESP32 controllers**, **Mosquitto MQTT broker**, and **Node-RED for testing and automation**.

---

# System Architecture

Customer  
↓  
Laundry Console (Tablet / Web App)  
↓  
Node-RED  
↓  
MQTT Broker (Mosquitto)  
↓  
ESP32 Coin Collector  
↓  
ESP32 Machine Controller  
↓  
Laundry Machine  

---

# Repository Structure

```
laundry-mqtt-integration/
│
├── README.md
│
├── esp32-machine-controller/
│   ├── src/
│   │   └── main.cpp
│   └── platformio.ini
│
├── esp32-coin-collector/
│   ├── src/
│   │   └── main.cpp
│   └── platformio.ini
│
└── nodered-test-flow/
    └── laundry_test_flow.json
```

---

# Components

## ESP32 Machine Controller

Controls the washing machine through optocouplers that emulate button presses.

Functions:

- Select washing program
- Start machine
- Reset machine
- Detect door status
- Detect machine running state
- Send status updates via MQTT

---

## ESP32 Coin Collector

Handles coin acceptor pulses and tracks customer credit.

Functions:

- Detect coin pulses
- Convert pulses to currency (MAD)
- Track inserted credit
- Display credit on OLED
- Send payment requests to console

---

## Node-RED Test Flow

Used for development and testing.

Allows:

- Sending machine commands
- Monitoring machine state
- Simulating console integration

Import the flow using:

Node-RED → Menu → Import → `laundry_test_flow.json`

---

# MQTT Broker

The system uses **Mosquitto** as the MQTT broker.

Default configuration:

Port: **1883**  
Protocol: **MQTT v3.1.1**

---

# MQTT Topic Structure

All communication follows this structure:

```
laundry/machine/<machine_id>/<category>
```

Example machine ID:

```
2
```

---

# Machine Status Topics

Published by the **ESP32 Machine Controller**

### Machine Online Status

Topic

```
laundry/machine/<id>/status
```

Payload

```
online
```

Example

```
laundry/machine/2/status
online
```

---

### Current Program Mode

Topic

```
laundry/machine/<id>/mode
```

Payload

```
1-8
```

Example

```
laundry/machine/2/mode
3
```

---

### Machine Running State

Topic

```
laundry/machine/<id>/running
```

Payload

```
0 = stopped
1 = running
```

Example

```
laundry/machine/2/running
1
```

---

### Door Status

Topic

```
laundry/machine/<id>/door
```

Payload

```
open
closed
```

Example

```
laundry/machine/2/door
closed
```

---

### Program Finished Event

Topic

```
laundry/machine/<id>/event/finished
```

Payload

```
1
```

Example

```
laundry/machine/2/event/finished
1
```

---

# Coin Collector Topics

Published by the **ESP32 Coin Collector**

### Coin Inserted

Topic

```
laundry/machine/<id>/coin/inserted
```

Payload

```
0.5
1
2
5
10
```

Example

```
laundry/machine/2/coin/inserted
5
```

---

### Payment Request

When the user presses **confirm** after inserting coins.

Topic

```
laundry/machine/<id>/payment/request
```

Payload

```
credit value
```

Example

```
laundry/machine/2/payment/request
15
```

---

# Console Commands

Sent from **console application → machine controller**

### Select Program

Topic

```
laundry/machine/<id>/cmd/setmode
```

Payload

```
1-8
```

Example

```
laundry/machine/2/cmd/setmode
3
```

---

### Start Machine

Topic

```
laundry/machine/<id>/cmd/start
```

Payload

```
1
```

Example

```
laundry/machine/2/cmd/start
1
```

---

### Reset Machine

Topic

```
laundry/machine/<id>/cmd/reset
```

Payload

```
1
```

Example

```
laundry/machine/2/cmd/reset
1
```

---

# Example MQTT Messages

Machine started

Topic

```
laundry/machine/2/running
```

Payload

```
1
```

Door closed

Topic

```
laundry/machine/2/door
```

Payload

```
closed
```

---

# Development Environment

Firmware is built using **PlatformIO**.

Example configuration:

```
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps =
    knolleary/PubSubClient
```

---

# Required Infrastructure

The system requires:

- MQTT Broker (Mosquitto)
- ESP32 Machine Controller
- ESP32 Coin Collector
- Laundry Console Application
- Node-RED (for testing and automation)

---

# Testing

Machine commands can be tested using **Node-RED** or any MQTT client.

Example test command:

Topic

```
laundry/machine/2/cmd/setmode
```

Payload

```
3
```

The machine should respond with:

```
laundry/machine/2/mode
3
```

---

# License

Internal project – laundry machine automation system.
