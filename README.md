# Laundry MQTT Integration

This repository contains firmware and test flows used to integrate
laundry machines with the console software via MQTT.

System architecture:

ESP32 Coin Acceptor
        ↓
MQTT (Mosquitto Broker)
        ↓
Node-RED
        ↓
Laundry Console Application

ESP32 Machine Controller
        ↑
        MQTT Commands

Main MQTT Topics:

laundry/machine/<id>/coin/inserted
laundry/machine/<id>/payment/request
laundry/machine/<id>/cmd/setmode
laundry/machine/<id>/cmd/start
laundry/machine/<id>/cmd/reset
laundry/machine/<id>/status
laundry/machine/<id>/running
laundry/machine/<id>/door
laundry/machine/<id>/event/finished
