# ox-simulator

A simulator for the [ox](https://github.com/freebirdxr/ox) OpenXR runtime. This can help test and emulate different VR devices (Quest, Vive, Vive trackers, etc.) programmatically.

## Purpose

This simulator allows developers to test OpenXR applications without physical hardware by simulating popular VR devices. It provides two interface modes for controlling the simulated devices:
- Web server interface - locally-hosted API for programmatic control
- GUI interface - windowed application for manual control (TBD)
## Building

1. Clone this repository using `git clone https://github.com/freebirdxr/ox-simulator`
2. Download [ox_driver.h](https://github.com/freebirdxr/ox/releases/latest/download/driver.h) and place it inside this folder.
3. Compile using `cmake`:

```bash
cmake -B build
cmake --build build --config Release
```

4. Your driver will be built inside `build/ox_simulator`.

## Installation

Copy the driver (i.e. the `build/ox_simulator` folder) to the ox runtime's `drivers` folder:

For example:

```
ox-runtime/
├── bin/
│   ├── ox-service.exe (or ox-service on Linux)
│   └── drivers/
│       └── ox_simulator/
│           └── driver.dll (or libdriver.so)
```

## Configuration

Edit the `config.json` file in the deployed driver folder (`drivers/ox_simulator/config.json`):

```json
{
  "device": "oculus_quest_2",
  "mode": "api",
  "api_port": 8765,
  "comment": "Device options: oculus_quest_2, oculus_quest_3, htc_vive, valve_index. Mode options: api, gui"
}
```

**Configuration Options:**
- `device`: VR device to emulate (`oculus_quest_2`, `oculus_quest_3`, `htc_vive`, `valve_index`)
- `mode`: Interface mode (`api` for HTTP server, `gui` for graphical interface)
- `api_port`: Port for HTTP API server (default: 8765)

## Usage

### Starting the Simulator

The simulator starts automatically when the ox service is launched.

### HTTP API Reference

The simulator provides a REST API for controlling virtual devices:

#### Get HMD Pose
```bash
GET http://localhost:8765/hmd/pose
```

**Response:**
```json
{
  "position": {"x": 0.0, "y": 1.6, "z": 0.0},
  "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}
}
```

#### Set HMD Pose
```bash
POST http://localhost:8765/hmd/pose
Content-Type: application/json

{
  "position": {"x": 0.0, "y": 1.6, "z": -1.0},
  "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}
}
```

#### Set Controller Pose
```bash
POST http://localhost:8765/controller/<id>/pose
Content-Type: application/json

{
  "active": true,
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}
}
```

**Parameters:**
- `<id>`: Controller ID (0 = left, 1 = right)
- `active`: Whether the controller is active/connected (optional, default: true)

#### Set Controller Input
```bash
POST http://localhost:8765/controller/<id>/input
Content-Type: application/json

{
  "component": "trigger/value",
  "value": 0.8
}
```

**Parameters:**
- `<id>`: Controller ID (0 = left, 1 = right)
- `component`: Input component path (e.g., `trigger/value`, `squeeze/value`, `thumbstick/x`, `thumbstick/y`, `a/click`, `b/click`)
- `value`: Numeric value (0.0 to 1.0) or boolean

**Common Input Components:**
- `trigger/value` - Trigger analog value (0.0 - 1.0)
- `trigger/click` - Trigger button click (boolean)
- `squeeze/value` - Grip squeeze analog value (0.0 - 1.0)
- `squeeze/click` - Grip button click (boolean)
- `thumbstick/x` - Thumbstick X axis (-1.0 to 1.0)
- `thumbstick/y` - Thumbstick Y axis (-1.0 to 1.0)
- `thumbstick/click` - Thumbstick button click (boolean)
- `a/click` - A button click (boolean)
- `b/click` - B button click (boolean)
- `x/click` - X button click (boolean)
- `y/click` - Y button click (boolean)

## API Usage Examples

### Using cURL

**Move HMD forward:**
```bash
curl -X POST http://localhost:8765/hmd/pose -H "Content-Type: application/json" -d '{
  "position": {"x": 0, "y": 1.6, "z": -1},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}'
```

**Position left controller:**
```bash
curl -X POST http://localhost:8765/controller/0/pose -H "Content-Type: application/json" -d '{
  "active": true,
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}'
```

**Press the left trigger:**
```bash
curl -X POST http://localhost:8765/controller/0/input -H "Content-Type: application/json" -d '{
  "component": "trigger/value",
  "value": 1.0
}'
```

### Using Python

```python
import requests
import json

BASE_URL = "http://localhost:8765"

# Move HMD
hmd_pose = {
    "position": {"x": 0, "y": 1.6, "z": -1},
    "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}
response = requests.post(f"{BASE_URL}/hmd/pose", json=hmd_pose)
print(f"Set HMD pose: {response.status_code}")

# Activate and position left controller
left_controller = {
    "active": True,
    "position": {"x": -0.2, "y": 1.4, "z": -0.3},
    "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}
response = requests.post(f"{BASE_URL}/controller/0/pose", json=left_controller)
print(f"Set left controller: {response.status_code}")

# Press trigger on left controller
trigger_input = {
    "component": "trigger/value",
    "value": 0.8
}
response = requests.post(f"{BASE_URL}/controller/0/input", json=trigger_input)
print(f"Set trigger: {response.status_code}")

# Press A button
button_input = {
    "component": "a/click",
    "value": True
}
response = requests.post(f"{BASE_URL}/controller/0/input", json=button_input)
print(f"Press A button: {response.status_code}")
```

## Testing

After deploying and configuring the simulator, you can test it with OpenXR applications:

1. Start the ox service (which loads the simulator driver)
2. Set the OpenXR runtime environment variable:
   - **Windows:** `$env:XR_RUNTIME_JSON = "C:\path\to\ox\build\win-x64\bin\ox_openxr.json"`
   - **Linux:** `export XR_RUNTIME_JSON="/path/to/ox/build/linux-x64/bin/ox_openxr.json"`
3. Control the simulator via HTTP API
4. Run your OpenXR application

## Troubleshooting

**Port already in use:**
- Change the `api_port` in `config.json`
- Check for other processes using the port: `netstat -ano | findstr :8765` (Windows) or `lsof -i :8765` (Linux)

**Driver not loading:**
- Verify the driver folder is in the `drivers` inside ox runtime.
- Check that `ox-service` is running
- Ensure `config.json` exists and is valid JSON

**API not responding:**
- Confirm `mode` is set to `"api"` in `config.json`
- Check `ox-service` console output for startup messages
- Test the root endpoint: `curl http://localhost:8765/`
