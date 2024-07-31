## README.md for esplock
- Author: lucasodra
- Last Updated: 2024 July 31

# esplock

`esplock` is a secure system designed to control access using ESP32 microcontrollers. The system utilizes a client-server architecture with WebSockets for communication, AES-256 encryption for secure command transmission, and Over-the-Air (OTA) updates for firmware management. This README provides an overview of the architecture, security measures, and implementation details.

## Architecture Overview

The system architecture involves three main components:

1. **Client**: Sends API requests to the OpenLock server to control ESP32 devices. The client encrypts commands using the ESP32's public key, which is obtained securely from the OpenLock server.

2. **OpenLock Server**: Handles API requests from the client, authenticates clients, and manages secure communication with the ESP32 devices. The server performs critical checks, such as validating the request against replay attacks and checking rate limits, before forwarding the encrypted command to the ESP32 via WebSocket Secure.

3. **ESP32 Device (esplock)**: An IoT device that connects to the OpenLock server via WebSocket Secure. It receives encrypted commands, decrypts them using a private key, and executes the specified actions, such as unlocking a door or updating its configuration.

### Key Updates and Security Enhancements

- **Encryption**: The system now uses AES-256 for encrypting commands from the client, ensuring robust security for sensitive data. The commands include actions like unlocking doors, updating configuration settings, and are always transmitted securely.

- **Public Key Management**: The ESP32 generates a public-private key pair upon startup, which is tied to the Door ID and stored securely. The public key is sent to the OpenLock server over HTTPS, minimizing the risk of exposure. Clients retrieve this public key via a secure API request to OpenLock.

- **Replay Attack Prevention**: The OpenLock server validates each request by checking if the hash of the encrypted data has been used before and if the request timestamp is within an acceptable range (1 minute). This ensures that any replayed commands are rejected.

- **Configuration Updates**: The system allows updating critical configuration settings like WiFi SSID, WiFi Password, OpenLock Server Address, Door ID, and Door Password. These updates are protected by validating the preset password and are securely persisted in the ESP32's storage.

### Architecture Diagram

User Settings for new ESP:
- WIFI SSID
- WIFI Password
- OpenLock Server Address
- Door ID
- Door Password

```mermaid
graph TD
    subgraph Client-Side Process
        A[User Website] -->|API Request<br>with Encrypted Data<br>Command+Timestamp+Password<br>& Door ID| B[OpenLock Server]
    end
    subgraph Server-Side Process
        B -->|Check if Hash has been used,<br>Public Key is banned,<br>or request limit exceeded| C[Redis Cache]
        C -->|Return Status| B
        B -->|If Valid, Forward Encrypted Data<br>to ESP32 via WebSocket Secure| D[ESP32]
    end
    subgraph ESP32-Side Process
        D -->|Decrypt Command<br>with Private Key<br>Verify Timestamp and Password| F[ESP32]
        F -->|If Valid, Execute Command:<br>Unlock/Lock Door, Update Settings| G[Door]
        F -->|Save Configurations<br>to Persistent Storage| H[Persistent Storage]
        F -->|Restart if Configurations<br>are Updated| H
        D -->|Send Keep-Alive| B
        D -->|Receive OTA Updates<br>from OpenLock Server| F
        D -->|Report Status| B
    end

    classDef client fill:#ffcc00,stroke:#333,stroke-width:2px;
    classDef server fill:#33cc33,stroke:#333,stroke-width:2px;
    classDef cache fill:#3399ff,stroke:#333,stroke-width:2px;
    classDef device fill:#ff6666,stroke:#333,stroke-width:2px;
    classDef storage fill:#ffd700,stroke:#333,stroke-width:2px;

    class A client;
    class B,D server;
    class C cache;
    class E,F,G,H device;
    class H storage;
```

## System Workflow

1. **Client Authentication**: Users authenticate with the OpenLock server using their email and website URL instead of a traditional username and password. This approach enhances security and integrates better with web applications. The OpenLock server then adds the authenticated website URL to its CORS policy, ensuring that only registered and verified URLs can make API requests. Upon successful authentication, users receive an API access token, which is required for all subsequent API interactions. This token verifies the identity and authorization of the client, ensuring that only authenticated users can interact with the system.

2. **Command Request**: Users send a command request (e.g., unlock door) to the OpenLock server using the API access token. The request includes the Door ID and encrypted data, which consists of the command, a timestamp, and the door password.

3. **Public Key Retrieval**: The OpenLock server retrieves the ESP32 device's public key associated with the specified Door ID from the Redis cache. If the public key is not present in the cache, the server denies the request and informs the client.

4. **Command Encryption**: The client encrypts the command, timestamp, and door password using AES-256 encryption with the ESP32's public key. This encrypted data, along with the unencrypted Door ID, is sent to the OpenLock server.

5. **Command Validation and Forwarding**: Upon receiving the command, the OpenLock server performs several checks:
   - **Token Validation**: Verifies the API access token.
   - **Security Checks**: Ensures that the Door ID and the requester's IP address are not banned, and that the request has not exceeded rate limits.
   - **Replay Attack Prevention**: Checks if the hash of the encrypted data has been used before and validates that the timestamp is within the 1-minute window. If all checks pass, the server forwards the encrypted command to the specified ESP32 device via WebSocket Secure.

6. **Command Reception and Decryption**: The ESP32 device receives the encrypted command through the secure WebSocket connection. It decrypts the command using its private key and verifies the timestamp to ensure the command is fresh and valid. The ESP32 also checks the door password for correctness.

7. **Action Execution**: If the command is valid and authorized, the ESP32 device executes the specified action, which could be unlocking/locking the door or updating critical configuration settings like the WiFi SSID, WiFi Password, OpenLock Server Address, Door ID, or Door Password. If the command involves updating configuration settings, the ESP32 saves the changes to persistent storage and restarts to apply the new settings.

8. **Keep-Alive and Reconnection**: To maintain a continuous and reliable connection, the ESP32 devices periodically send keep-alive messages to the OpenLock server. If the connection is lost, the devices automatically attempt to reconnect.

9. **OTA Updates**: The system supports Over-the-Air (OTA) updates, allowing the OpenLock server to push firmware updates to the ESP32 devices. This feature ensures that devices can receive the latest security patches and functionality improvements without requiring physical access.

10. **Logging and Monitoring**: The system logs all actions, including successful and failed authentication attempts, command executions, and system errors. These logs are monitored for auditing and security purposes, helping to detect and respond to anomalies or potential security threats.

Apologies for the oversight. Here's the updated README section with the inclusion of the latest security features, including the CORS policy:

## Security Measures

### 1. **TLS Encryption**
- All communications between clients, the OpenLock server, and ESP32 devices are secured using Transport Layer Security (TLS). This protocol ensures that data is encrypted during transmission, protecting against eavesdropping and tampering. TLS also provides data integrity, ensuring that messages cannot be altered without detection. This encryption is crucial for maintaining the confidentiality and integrity of data exchanges.

### 2. **Public-Private Key Encryption**
- Commands are encrypted with the ESP32's public key and can only be decrypted by the corresponding private key stored securely on the device. This asymmetrical encryption ensures that sensitive data, such as commands to unlock a door, cannot be intercepted or modified during transit. The private key never leaves the ESP32 device, providing an additional layer of security. The public key is transmitted to the OpenLock server via HTTPS, minimizing the risk of exposure.

### 3. **Mutual Authentication**
- Mutual authentication is enforced, requiring both the OpenLock server and the ESP32 devices to authenticate each other before establishing a connection. This two-way verification process ensures that only authorized devices and servers can communicate, preventing unauthorized devices from accessing the system and ensuring secure communication.

### 4. **Token-Based Authentication**
- Clients must obtain a JSON Web Token (JWT) to authenticate API requests to the OpenLock server. JWTs are issued after successful authentication, using the user's email and website URL. The website URL is registered with the server and added to the CORS policy, restricting API access to trusted domains only. This ensures that only registered and verified websites can make API requests, enhancing security. JWTs have expiration times to limit their validity period, and the server can refresh or revoke tokens as necessary.

### 5. **CORS Policy Enforcement**
- The OpenLock server enforces a strict Cross-Origin Resource Sharing (CORS) policy, allowing API requests only from registered and verified website URLs. This is part of the authentication process where users provide their email and website URL. The CORS policy helps prevent unauthorized websites from accessing the API, mitigating risks from cross-origin attacks and ensuring that only trusted clients can interact with the system.

### 6. **Rate Limiting and IP Blocking**
- The OpenLock server implements rate limiting to control the number of requests each token can make per second. This measure prevents abuse and protects the server from Denial of Service (DoS) attacks. Additionally, IP addresses showing suspicious behavior, such as exceeding rate limits or attempting unauthorized access, are temporarily blocked to protect against potential threats.

### 7. **Unique Device Identifiers**
- Each ESP32 device is assigned a unique identifier (UID) and a corresponding RSA key pair. The UID is used by the server to target specific devices when issuing commands, ensuring that each device can be uniquely addressed and authenticated. This prevents unauthorized devices from executing commands and ensures secure device management.

### 8. **Over-The-Air (OTA) Updates**
- The system supports OTA updates, allowing secure firmware upgrades without physical access to the ESP32 devices. This capability is crucial for deploying security patches and new features promptly, ensuring that all devices in the network run the latest and most secure firmware. OTA updates are securely transmitted and verified before application to ensure integrity and authenticity.

### 9. **Fail-Safe Mechanisms**
- In the event of a network failure or inability to contact the OpenLock server, ESP32 devices operate based on pre-configured fail-safe settings. These settings ensure that the system remains secure, maintaining the current state or reverting to a default secure state. This ensures continuous protection even if the system goes offline.

## Industry Safety Features

- **Replay Attack Prevention**: Each command sent to an ESP32 device includes a unique timestamp and nonce. The OpenLock server checks if the hash of the encrypted data has been transmitted before and validates that the timestamp is within a 1-minute window. This prevents replay attacks, where an attacker could potentially re-send captured messages to replay commands, ensuring that each command is unique and timely.

- **Data Integrity**: All data transmitted between the server and devices is verified for integrity using cryptographic checksums. This ensures that any tampering with the data can be detected and mitigated. The system's use of cryptographic checksums protects against unauthorized data manipulation, ensuring that all data remains accurate and trustworthy.

- **Access Control**: The system employs role-based access control (RBAC), defining specific permissions for different types of users and devices. This ensures that only authorized users can issue sensitive commands, such as unlocking a door. RBAC helps manage access rights and enhances overall security by limiting actions to specific roles, reducing the risk of unauthorized access.

- **Logging and Monitoring**: The OpenLock system logs all actions, including successful and unsuccessful authentication attempts, command executions, and system errors. This data is monitored to detect suspicious activity and respond quickly to potential security threats, providing a robust audit trail for forensic analysis. Logging and monitoring are crucial for identifying and mitigating potential security issues, ensuring the system remains secure and reliable.

## Getting Started

### Prerequisites

- **ESP32 Microcontroller**: Ensure the device has WiFi capability for network connectivity.
- **OpenLock Server**: The server must be set up with TLS for secure communication and WebSocket support to enable real-time interactions.
- **RSA Key Pair**: The ESP32 generates a public-private RSA key pair for secure command encryption and decryption. The public key is shared with the OpenLock server via HTTPS to ensure secure communication.

### Installation

1. **Clone the Repository**
   Clone the `esplock` repository to your local development environment:
   ```bash
   git clone https://github.com/lucasodra/esplock.git
   cd esplock
   ```

2. **Configure WiFi and Server Details**
   Open the `firmware.cpp` file and update the following details:
   - **WIFI_SSID**: Your WiFi network name.
   - **WIFI_PASSWORD**: Your WiFi password.
   - **SERVER_ADDRESS**: The domain or IP address of the OpenLock server.
   - **DOOR_ID**: A unique identifier for the door controlled by the ESP32.
   - **DOOR_PASSWORD**: A secure password for additional authentication.

3. **Upload the Firmware**
   Use the Arduino IDE or PlatformIO to compile and upload the firmware to your ESP32 device:
   - **Arduino IDE**: Open the `firmware.cpp` file, select your ESP32 board and port, and click "Upload."
   - **PlatformIO**: Navigate to the project directory and run the upload command.

4. **Monitor the Serial Output**
   Open the Serial Monitor in the Arduino IDE or PlatformIO to observe the ESP32's activity, such as key generation, connection attempts, and command execution. This is useful for debugging and ensuring the system operates as expected.

### Key Points to Note

- **CORS Policy**: The OpenLock server enforces a CORS policy to allow API requests only from registered website URLs. Ensure that the website URL used to access the API is registered with the OpenLock server.
- **Encryption & Security**: The ESP32 communicates with the OpenLock server using encrypted channels, and commands are encrypted with the device's public key to prevent unauthorized access.
- **Fail-Safe Mechanisms**: The ESP32 devices have pre-configured fail-safe settings to maintain security even during network failures or loss of communication with the server.

### Tech Stack

The `esplock` system utilizes the following technologies:

- **ESP32 Microcontroller**: A versatile microcontroller with WiFi and Bluetooth capabilities, ideal for controlling physical door locks.
- **WebSocket Protocol**: Facilitates real-time, bidirectional communication between the OpenLock server and ESP32 devices, ensuring efficient command delivery.
- **RSA Encryption**: Secures commands by encrypting them with the ESP32's public key and decrypting them on the device, ensuring robust data protection.
- **TLS/SSL**: Ensures secure data transmission over the network, preventing interception and maintaining data integrity.
- **Arduino Framework**: A comprehensive development platform for programming the ESP32, supported by extensive libraries.
- **mbedTLS**: A lightweight cryptographic library providing RSA encryption and secure communication protocols.
- **OTA Updates**: Allows remote firmware updates, facilitating the deployment of security patches and new features without physical access to devices.

### Dependencies

The following libraries and versions are required for the ESP32 firmware:

| Dependency              | Version   | Description                                              |
|-------------------------|-----------|----------------------------------------------------------|
| **Arduino Core for ESP32** | 1.0.6 or later | Core library for ESP32 development, including WiFi and networking capabilities. |
| **WiFiClientSecure**    | Built-in  | Provides secure communication over SSL/TLS for WiFi clients. |
| **WebSocketsClient**    | 2.3.5     | Enables WebSocket communication for real-time interactions with the server. |
| **ArduinoJson**         | 6.18.5    | A flexible JSON library used for parsing and creating JSON data structures. |
| **Adafruit NeoPixel**   | 1.7.0     | Controls RGB LEDs for visual status indication and feedback. |
| **mbedTLS**             | 2.16.3    | Provides cryptographic functionalities, including RSA and TLS/SSL. |
| **ArduinoOTA**          | Built-in  | Facilitates OTA updates, enabling remote firmware upgrades. |
| **Ticker**              | Built-in  | A library for creating software timers, useful for periodic tasks like keep-alive signals. |

### Installing Dependencies

To install these dependencies, follow the steps below using either the Arduino IDE or PlatformIO.

**Arduino IDE:**

1. Open the Arduino IDE.
2. Go to `Sketch` > `Include Library` > `Manage Libraries...`.
3. Search for and install the required libraries.

**PlatformIO:**

Add the following to your `platformio.ini` file:

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    arduino-libraries/ArduinoJson@^6.18.5
    knolleary/WebSockets@^2.3.5
    adafruit/Adafruit NeoPixel@^1.7.0
    espressif/arduino-esp32
```

## Interpreting the Blinking Lights on ESP32

The ESP32 uses a NeoPixel LED to visually indicate different states and actions. Below is a table explaining the meaning of each blinking pattern.

| Color           | Blinking Pattern                 | Description                                                     |
|-----------------|----------------------------------|-----------------------------------------------------------------|
| **Blue**        | 1 blink every 500ms              | Connecting to WiFi                                              |
| **Green**       | 3 blinks, 100ms delay            | Successfully connected to WiFi                                  |
| **Orange**      | 1 blink every 500ms              | Attempting to connect to WebSocket server                       |
| **Green**       | 5 blinks, 100ms delay            | Successfully connected to WebSocket server                      |
| **White**       | 1 blink, 100ms delay             | Received a new command from the server                          |
| **Red**         | 5 blinks, 100ms delay            | Error in processing, such as failed decryption or parsing       |
| **Blue**        | 1 blink, 100ms delay             | Sending a keep-alive message to the WebSocket server            |
| **Green**       | 5 blinks, 100ms delay            | Door unlocked                                                   |
| **Red**         | 5 blinks, 100ms delay            | Door locked                                                     |
| **Yellow**      | 5 blinks, 100ms delay            | Updating WiFi credentials, server address, or preset password   |
| **Purple**      | 5 blinks, 100ms delay            | OTA update in progress                                          |
| **Cyan**        | 5 blinks, 100ms delay            | Ping response sent (public key or status requested)             |

### Note
- The LED will turn off after completing the blinking pattern for each event.
- For any critical errors, the device may repeat the red blinking pattern until the issue is resolved.

## Contributing

We welcome contributions to enhance the security and functionality of the esplock system. Please fork the repository and create a pull request with your changes.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.