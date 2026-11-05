
# ⚡ Industrial Embedded Data Acquisition System

### An embedded solution for industrial signal acquisition, storage, and communication

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Keil](https://img.shields.io/badge/Keil-394049?style=for-the-badge&logo=arm&logoColor=00C1DE)
![Modbus](https://img.shields.io/badge/Protocol-Modbus-1F6FEB?style=for-the-badge)
![SPI](https://img.shields.io/badge/Interface-SPI-orange?style=for-the-badge)
![TF Card](https://img.shields.io/badge/Storage-TF%20Card-success?style=for-the-badge)

</div>

---

## 📖 Overview

This project is an embedded industrial data-acquisition system developed for a competition based on practical industrial requirements.

The system collects voltage and current signals, processes and stores the acquired data locally, and exchanges information with external devices through industrial communication protocols.

The software adopts a modular architecture, separating peripheral drivers, data acquisition, file storage, protocol communication, and application logic for easier debugging and future development.

---

## ✨ Key Features

- ⚡ Voltage and current signal acquisition
- 🔌 SPI-based peripheral communication
- 💾 TF card file reading and writing
- 📊 Local measurement-data storage
- 🔄 Modbus communication
- 📡 Custom communication protocol
- 🧩 Modular software architecture
- 🛠️ On-site acquisition and debugging support

---

## 🏗️ System Workflow

```text
Industrial Signal Input
          ↓
Signal Conditioning
          ↓
Data Acquisition
          ↓
Data Processing
       ↙      ↘
TF Card Storage   Modbus / Custom Communication
