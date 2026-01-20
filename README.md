# ox-simulator

A simulator for the [ox](https://github.com/ox-runtime/ox) OpenXR runtime. This can help test and emulate different VR devices (Quest, Vive, Vive trackers, etc.) programmatically.

## Purpose

This simulator allows developers to test OpenXR applications without physical hardware by simulating popular VR devices. It provides two interface modes for controlling the simulated devices:
- Web server interface - locally-hosted API for programmatic control
- GUI interface - windowed application for manual control (TBD)
## Building

1. Clone this repository using `git clone https://github.com/ox-runtime/ox-simulator`
2. Download [ox_driver.h](https://github.com/ox-runtime/ox/releases/latest/download/driver.h) and place it inside this folder.
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

#### Get Device Pose
```bash
GET http://localhost:8765/device/pose?user_path=/user/hand/left
```

**Query Parameters:**
- `user_path`: OpenXR user path for the device (e.g., `/user/hand/left`, `/user/hand/right`, `/user/vive_tracker/waist`)

**Response:**
```json
{
  "user_path": "/user/hand/left",
  "is_active": true,
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}
}
```

#### Set Device Pose
```bash
POST http://localhost:8765/device/pose
Content-Type: application/json

{
  "user_path": "/user/hand/left",
  "active": true,
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}
}
```

**Parameters:**
- `user_path`: OpenXR user path for the device (e.g., `/user/hand/left`, `/user/hand/right`, `/user/vive_tracker/waist`, `/user/vive_tracker/left_foot`)
- `active`: Whether the device is active/connected (optional, default: true)

#### Get Device Input
```bash
GET http://localhost:8765/device/input?user_path=/user/hand/left&component=/input/trigger/value
```

**Query Parameters:**
- `user_path`: OpenXR user path for the device (e.g., `/user/hand/left`, `/user/hand/right`)
- `component`: Input component path with `/input/` prefix (e.g., `/input/trigger/value`, `/input/squeeze/value`, `/input/thumbstick/x`)

**Response:**
```json
{
  "user_path": "/user/hand/left",
  "component": "/input/trigger/value",
  "boolean_value": false,
  "float_value": 0.8,
  "x": 0.0,
  "y": 0.0
}
```

#### Set Device Input
```bash
POST http://localhost:8765/device/input
Content-Type: application/json

{
  "user_path": "/user/hand/left",
  "component": "/input/trigger/value",
  "value": 0.8
}
```

**Parameters:**
- `user_path`: OpenXR user path for the device (e.g., `/user/hand/left`, `/user/hand/right`)
- `component`: Input component path with `/input/` prefix (e.g., `/input/trigger/value`, `/input/squeeze/value`, `/input/thumbstick/x`)
- `value`: Numeric value (0.0 to 1.0) or boolean

**Common Input Components:**
- `/input/trigger/value` - Trigger analog value (0.0 - 1.0)
- `/input/trigger/click` - Trigger button click (boolean)
- `/input/squeeze/value` - Grip squeeze analog value (0.0 - 1.0)
- `/input/squeeze/click` - Grip button click (boolean)
- `/input/thumbstick/x` - Thumbstick X axis (-1.0 to 1.0)
- `/input/thumbstick/y` - Thumbstick Y axis (-1.0 to 1.0)
- `/input/thumbstick/click` - Thumbstick button click (boolean)
- `/input/a/click` - A button click (boolean)
- `/input/b/click` - B button click (boolean)
- `/input/x/click` - X button click (boolean)
- `/input/y/click` - Y button click (boolean)

## API Usage Examples

### Using cURL

**Get current HMD pose:**
```bash
curl http://localhost:8765/hmd/pose
```

**Move HMD forward:**
```bash
curl -X POST http://localhost:8765/hmd/pose -H "Content-Type: application/json" -d '{
  "position": {"x": 0, "y": 1.6, "z": -1},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}'
```

**Get left controller pose:**
```bash
curl "http://localhost:8765/device/pose?user_path=/user/hand/left"
```

**Position left controller:**
```bash
curl -X POST http://localhost:8765/device/pose -H "Content-Type: application/json" -d '{
  "user_path": "/user/hand/left",
  "active": true,
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}'
```

**Get trigger value:**
```bash
curl "http://localhost:8765/device/input?user_path=/user/hand/left&component=/input/trigger/value"
```

**Press the left trigger:**
```bash
curl -X POST http://localhost:8765/device/input -H "Content-Type: application/json" -d '{
  "user_path": "/user/hand/left",
  "component": "/input/trigger/value",
  "value": 1.0
}'
```

**Position a Vive tracker on waist:**
```bash
curl -X POST http://localhost:8765/device/pose -H "Content-Type: application/json" -d '{
  "user_path": "/user/vive_tracker/waist",
  "active": true,
  "position": {"x": 0, "y": 1.0, "z": 0},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}'
```

### Using Python

```python
import requests
import json

BASE_URL = "http://localhost:8765"

# Get current HMD pose
response = requests.get(f"{BASE_URL}/hmd/pose")
if response.status_code == 200:
    hmd_pose = response.json()
    print(f"Current HMD pose: {hmd_pose}")

# Move HMD
hmd_pose = {
    "position": {"x": 0, "y": 1.6, "z": -1},
    "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}
response = requests.post(f"{BASE_URL}/hmd/pose", json=hmd_pose)
print(f"Set HMD pose: {response.status_code}")

# Get current left controller pose
response = requests.get(f"{BASE_URL}/device/pose?user_path=/user/hand/left")
if response.status_code == 200:
    left_pose = response.json()
    print(f"Left controller pose: {left_pose}")

# Activate and position left controller
left_controller = {
    "user_path": "/user/hand/left",
    "active": True,
    "position": {"x": -0.2, "y": 1.4, "z": -0.3},
    "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}
response = requests.post(f"{BASE_URL}/device/pose", json=left_controller)
print(f"Set left controller: {response.status_code}")

# Get current trigger value
response = requests.get(f"{BASE_URL}/device/input?user_path=/user/hand/left&component=/input/trigger/value")
if response.status_code == 200:
    trigger_state = response.json()
    print(f"Trigger value: {trigger_state['float_value']}")

# Press trigger on left controller
trigger_input = {
    "user_path": "/user/hand/left",
    "component": "/input/trigger/value",
    "value": 0.8
}
response = requests.post(f"{BASE_URL}/device/input", json=trigger_input)
print(f"Set trigger: {response.status_code}")

# Press A button
button_input = {
    "user_path": "/user/hand/left",
    "component": "/input/a/click",
    "value": True
}
response = requests.post(f"{BASE_URL}/device/input", json=button_input)
print(f"Press A button: {response.status_code}")

# Add Vive tracker on waist
tracker_pose = {
    "user_path": "/user/vive_tracker/waist",
    "active": True,
    "position": {"x": 0, "y": 1.0, "z": 0},
    "orientation": {"x": 0, "y": 0, "z": 0, "w": 1}
}
response = requests.post(f"{BASE_URL}/device/pose", json=tracker_pose)
print(f"Set waist tracker: {response.status_code}")
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
