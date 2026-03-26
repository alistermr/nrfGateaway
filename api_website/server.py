from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import requests
import re
from datetime import datetime, timezone

UUID_PATTERN = re.compile(r'(?:uuid=)?([0-9a-fA-F]{32})(?:\s|$)')
uuid_cache = []
scan_start_time = None

app = Flask(__name__, static_folder="static", static_url_path="/static")
CORS(app)

# NRF Cloud API configuration
API_KEY = "648758f0b3e4bcbc39a82c8516e68ab2662248af"
BASE_URL = "https://api.nrfcloud.com/v1"
#DEVICE_ID = "5034474b-3731-4738-80d4-0c0ffd414431" #2
DEVICE_ID = "50344654-3037-4bdd-8004-2314d6fc32b9"

DELAY = 0

@app.route("/")
def index():
    return send_from_directory(".", "index.html")


@app.route("/api/send", methods=["POST"])
def send_message():
    """Send a message to the nRF Cloud board and return the response."""
    data = request.get_json()
    message = data.get("message", "")

    if not message:
        return jsonify({"error": "Message is required"}), 400

    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json",
    }

    payload = {
        "topic": f"d/{DEVICE_ID}/c2d",
        "message": {
            "appId": "uart",
            "data": message,
        },
    }

    url = f"{BASE_URL}/devices/{DEVICE_ID}/messages"

    try:
        response = requests.post(url, headers=headers, json=payload)
        status_code = response.status_code

        try:
            body = response.json()
        except ValueError:
            body = {"raw": response.text}

        return jsonify({"status": status_code, "response": body}), status_code

    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/get_messages", methods=["GET"])
def get_messages():
    """Fetch stored device messages from nRF Cloud."""
    headers = {
        "Authorization": f"Bearer {API_KEY}",
    }

    params = {
        "deviceId": DEVICE_ID,
        "pageLimit": 20,
        "pageSort": "desc",
    }
    if appId := request.args.get("appId"):
        params["appId"] = appId

    url = f"{BASE_URL}/messages"

    try:
        response = requests.get(url, headers=headers, params=params)
        status_code = response.status_code

        try:
            body = response.json()
        except ValueError:
            body = {"raw": response.text}

        return jsonify({"status": status_code, "response": body}), 200

    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/device", methods=["GET"])
def get_device_state():
    """Fetch the current device state from nRF Cloud."""
    headers = {
        "Authorization": f"Bearer {API_KEY}",
    }

    url = f"{BASE_URL}/devices/{DEVICE_ID}"

    try:
        response = requests.get(url, headers=headers)
        status_code = response.status_code

        try:
            body = response.json()
        except ValueError:
            body = {"raw": response.text}

        return jsonify({"status": status_code, "response": body}), 200

    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/nonprov", methods=["GET"])
def get_nonprov():
    """Poll nRF Cloud for UART messages and extract UUIDs into a cache."""
    headers = {"Authorization": f"Bearer {API_KEY}"}
    try:
        response = requests.get(
            f"{BASE_URL}/messages",
            headers=headers,
            params={"deviceId": DEVICE_ID, "pageLimit": 20, "pageSort": "desc"},
        )
        if response.status_code == 200:
            items = response.json().get("items", [])
            newest = items[0].get("receivedAt") if items else "none"
            print(f"nonprov: {len(items)} items, newest={newest}, scan_start={scan_start_time}")
            for item in items:
                received_at = item.get("receivedAt", "")
                appid = item.get("message", {}).get("appId", "")
                msg_data = item.get("message", {}).get("data", "")
                if scan_start_time and received_at:
                    msg_time = datetime.fromisoformat(received_at.replace("Z", "+00:00"))
                    if msg_time < scan_start_time:
                        continue
                print(f"  -> appId='{appid}' data='{msg_data}' receivedAt={received_at}")
                for uuid in UUID_PATTERN.findall(msg_data):
                    if uuid not in uuid_cache:
                        uuid_cache.append(uuid)
    except requests.exceptions.RequestException:
        pass

    return jsonify({"uuids": uuid_cache})


@app.route("/api/nonprov/clear", methods=["POST"])
def clear_nonprov():
    """Clear the cached UUID list and record scan start time."""
    global scan_start_time
    uuid_cache.clear()
    scan_start_time = datetime.now(timezone.utc)
    return jsonify({"status": "cleared"})


if __name__ == "__main__":
    print("Server running at http://localhost:5000")
    app.run(debug=True, port=5000)
