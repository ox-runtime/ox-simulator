import requests
import time

# Base URL for the ox-simulator HTTP API
BASE_URL = "http://localhost:8765"

# Use a session for connection reuse
session = requests.Session()


def set_device_profile(device_name):
    """Switch the simulator to emulate a specific device."""
    url = f"{BASE_URL}/v1/profile"
    data = {"device": device_name}
    response = session.put(url, json=data)
    if response.status_code == 200:
        result = response.json()
        print(f"Switched to device: {result['device']}")
    else:
        print(f"Failed to switch device: {response.status_code} - {response.text}")


def set_headset_pose(position, orientation):
    """Set the pose of the headset."""
    url = f"{BASE_URL}/v1/devices/user/head"
    data = {"position": position, "orientation": orientation}
    response = session.put(url, json=data)
    if response.status_code != 200:
        print(f"Failed to set headset pose: {response.status_code} - {response.text}")


def set_controller_pose(hand, position, orientation, active=True):
    """Set the pose of a controller."""
    url = f"{BASE_URL}/v1/devices/user/hand/{hand}"
    data = {"position": position, "orientation": orientation, "active": active}
    response = session.put(url, json=data)
    if response.status_code != 200:
        print(f"Failed to set {hand} controller pose: {response.status_code} - {response.text}")


def set_input_value(hand, input_path, value):
    """Set the value of an input component."""
    url = f"{BASE_URL}/v1/inputs/user/hand/{hand}/input/{input_path}"
    data = {"value": value}
    response = session.put(url, json=data)
    if response.status_code != 200:
        print(f"Failed to set {hand} {input_path}: {response.status_code} - {response.text}")


def main():
    # Emulate Oculus Quest 2 controllers by default
    set_device_profile("oculus_quest_2")

    # Check http://localhost:8765/v1/profile to see the available inputs for the selected device profile
    # and adjust the input paths below accordingly.

    set_headset_pose({"x": 0.0, "y": 0.0, "z": 0.0}, {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0})

    print("Starting controller simulation loop. Press Ctrl+C to stop.")

    i = 0.0

    try:
        while True:
            for hand in ["left", "right"]:
                # Static pose values - replace with actual controller data
                # TODO: Fetch actual position from your DIY controller hardware
                # Example: position = get_controller_position(hand)

                i += 0.001  # Just to demonstrate movement

                position = {
                    "x": -0.4 + i if hand == "left" else 0.2,  # Left controller slightly left, right slightly right
                    "y": 1.4,  # At chest height
                    "z": -2,  # Forward
                }

                # Note: Blender transforms the position/rotation after applying tracking, so play
                # with the values here to place the controllers where you want in your scene.
                # Use 'Screencast VR view' and viewport gizmos in the VR Scene Inspector addon to visualize.

                # TODO: Fetch actual orientation from your DIY controller hardware
                # Example: orientation = get_controller_orientation(hand)
                orientation = {"x": 0.7071, "y": 0.0, "z": 0.0, "w": 0.7071}  # 90 degrees on x axis

                # Activate and set pose
                set_controller_pose(hand, position, orientation, active=True)

                # Set input values - replace with actual button/trigger states
                # TODO: Fetch actual trigger value from your DIY controller hardware
                # Example: trigger_value = get_trigger_value(hand)
                trigger_value = 0.0  # Not pressing trigger. Set to 1.0 for fully pressed
                set_input_value(hand, "trigger/value", trigger_value)

                # TODO: Fetch actual grip value from your DIY controller hardware
                grip_value = 0.0  # Not gripping
                set_input_value(hand, "squeeze/value", grip_value)

                # TODO: Fetch actual button states from your DIY controller hardware
                # For Quest controllers, common buttons: a, b, x, y, menu, system
                # For Vive/Index: trigger_click, grip_click, menu, etc.
                # Example: a_button_pressed = is_button_pressed(hand, 'a')
                a_button_pressed = False  # Not pressed
                set_input_value(hand, "a/click", a_button_pressed)

                b_button_pressed = False  # Not pressed
                set_input_value(hand, "b/click", b_button_pressed)

                # Add more inputs as needed for your device profile
                # Check GET /v1/profile to see available inputs for the current device

            # Update at 60 fps (adjust as needed)
            FPS = 60.0
            time.sleep(1.0 / FPS)

    except KeyboardInterrupt:
        print("\nStopping simulation.")


if __name__ == "__main__":
    main()
