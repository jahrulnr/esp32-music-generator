# ESP32 Music Generator

## Overview
The ESP32 Music Generator is a project designed to create a lofi music generator using the ESP32 microcontroller. This project includes features such as audio synthesis, touch controls, weather integration, and web-based configuration. It is structured to be modular and extensible, making it easy to add new features or modify existing ones.

## Features
- **Audio Synthesis**: Generate lofi music using preloaded samples and synthesized sounds.
- **Touch Controls**: Interact with the device using touch sensors.
- **Weather Integration**: Display weather information and use it to influence music generation.
- **Web Interface**: Configure and control the device through a web-based interface.
- **Modular Design**: Easily extend functionality with additional components and services.

## Project Structure
```
boards/
data/
include/
lib/
    FileManager/
    I2CManager/
    SendTask/
    Sstring/
src/
    app/
    audio/
    display/
    repository/
    sensors/
    services/
    utils/
    web/
    main.cpp
tools/
4mb.csv
platformio.ini
```

### Key Directories
- **`boards/`**: Board configuration files.
- **`data/`**: Data files for the project.
- **`lib/`**: Custom libraries such as FileManager, I2CManager, and more.
- **`src/`**: Main source code, including application logic, audio synthesis, and web controllers.
- **`tools/`**: Utility scripts like `partition_manager.py`.

## Getting Started

### Prerequisites
- Install [PlatformIO](https://platformio.org/) in your development environment.
- ESP32 development board.

### Setup
1. Clone the repository:
   ```bash
   git clone https://github.com/jahrulnr/esp32-music-generator.git
   cd esp32-music-generator
   ```
2. Open the project in your preferred IDE (e.g., VS Code with PlatformIO extension).
3. Connect your ESP32 board to your computer.
4. Build and upload the project:
   ```bash
   pio run --target upload
   ```

## Usage
- Access the web interface to configure settings and control the device.
- Use touch sensors to interact with the music generator.
- View weather information on the display.

## Contributing
Contributions are welcome! Please follow these steps:
1. Fork the repository.
2. Create a new branch for your feature or bugfix.
3. Submit a pull request with a detailed description of your changes.

## License
This project is licensed under the MIT License. See the LICENSE file for details.

## Acknowledgments
- [PlatformIO](https://platformio.org/)
- [ESP32](https://www.espressif.com/en/products/socs/esp32)
- Open-source libraries and contributors.