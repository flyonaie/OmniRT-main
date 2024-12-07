# AimRT CMake Configuration Tool

This is a web-based configuration tool for managing AimRT CMake build options.

## Setup

1. Install Python requirements:
```bash
pip install -r requirements.txt
```

2. Run the application:
```bash
python app.py
```

3. Open your web browser and navigate to:
```
http://localhost:5000
```

## Features

- Web-based interface for managing CMake options
- Save configurations for later use
- Generate CMake commands automatically
- Group options by build and feature categories
- Tooltips with option descriptions

## Usage

1. Use the toggle switches to enable/disable options
2. Click "Save Configuration" to save your settings
3. Click "Apply Configuration" to generate a batch file with the CMake command
4. Run the generated `configure_cmake.bat` to apply the configuration
