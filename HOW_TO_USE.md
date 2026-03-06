# How to Run the MQTT Backend

This guide explains how to start the **MQTT broker (Mosquitto)** and **Node-RED** using Docker and how to import the provided Node-RED test flow.

---

# 1. Install Docker

## Ubuntu / Debian

```bash
sudo apt update
sudo apt install docker.io
```

Enable Docker:

```bash
sudo systemctl enable docker
sudo systemctl start docker
```

Verify installation:

```bash
docker --version
```

---

# 2. Create Docker Network

Create a shared network so Mosquitto and Node-RED can communicate.

```bash
docker network create laundry-net
```

---

# 3. Run Mosquitto MQTT Broker

Start the MQTT broker container.

```bash
docker run -d \
--name mosquitto \
--network laundry-net \
-p 1883:1883 \
eclipse-mosquitto
```

Verify it is running:

```bash
docker ps
```

You should see:

```
mosquitto
```

---

# 4. Run Node-RED

Start Node-RED on the same network.

```bash
docker run -d \
--name nodered \
--network laundry-net \
-p 1880:1880 \
nodered/node-red
```

Verify:

```bash
docker ps
```

You should see:

```
mosquitto
nodered
```

---

# 5. Open Node-RED

Open Node-RED in a browser:

```
http://localhost:1880
```

---

# 6. Import the Node-RED Flow

Inside Node-RED:

1. Click the **menu icon (top right)**  
2. Click **Import**  
3. Select **File**  
4. Choose the file:

```
nodered/laundry_test_flow.json
```

5. Click **Import**
6. Click **Deploy**

---

# 7. Configure MQTT in Node-RED

When configuring MQTT nodes:

Broker:

```
mosquitto
```

Port:

```
1883
```

Do **NOT** use `localhost`, because Node-RED runs inside Docker.

---

# 8. Restarting the System

Stop containers:

```bash
docker stop mosquitto
docker stop nodered
```

Start containers again:

```bash
docker start mosquitto
docker start nodered
```

---

# 9. Debug MQTT Messages

To monitor all MQTT messages:

```bash
docker exec -it mosquitto mosquitto_sub -h localhost -t "#" -v
```

This will show all MQTT traffic in real time.
