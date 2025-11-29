#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <Process.h>

// Relay pins (active LOW modules are common; adjust RELAY_ON/OFF if your board differs)
const uint8_t RELAY_PINS[4] = {2, 3, 4, 5};
const uint8_t RELAY_COUNT = sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]);
const uint8_t RELAY_ON = LOW;
const uint8_t RELAY_OFF = HIGH;

// Status and feedback indicators
const uint8_t LED_STATUS = 13;   // On-board LED for heartbeat and success
const uint8_t LED_ERROR = 12;    // External LED for error signalling (optional)
const uint8_t BUZZER = 8;        // Piezo buzzer for audible feedback (optional)

// Network and API configuration
const char* STATIC_IP = "192.168.0.220";  // Documented target IP (set on Linux side)
const uint16_t SERVER_PORT = 5555;         // Accessible via http://<IP>:5555/relay?channel=1&action=on

BridgeServer server(SERVER_PORT);
unsigned long lastHeartbeat = 0;

struct RelayCommand {
  int channel;           // 1-4
  String action;         // on, off, pulse
  unsigned long pulseMs; // optional; default 500 ms
};

void setup() {
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED_STATUS, LOW);
  digitalWrite(LED_ERROR, LOW);
  digitalWrite(BUZZER, LOW);

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_OFF);
  }

  Bridge.begin();
  server.noListenOnLocalhost();   // Accept connections from LAN
  server.begin();

  notifyStartup();
}

void loop() {
  heartbeat();
  BridgeClient client = server.accept();
  if (client) {
    handleClient(client);
    client.stop();
  }
}

void notifyStartup() {
  // Quick startup pattern for LEDs and buzzer
  digitalWrite(LED_STATUS, HIGH);
  tone(BUZZER, 2000, 150);
  delay(150);
  digitalWrite(LED_STATUS, LOW);
  delay(150);
  digitalWrite(LED_STATUS, HIGH);
  tone(BUZZER, 1200, 150);
  delay(150);
  digitalWrite(LED_STATUS, LOW);
}

void heartbeat() {
  unsigned long now = millis();
  if (now - lastHeartbeat >= 1000) {
    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    lastHeartbeat = now;
  }
}

void handleClient(BridgeClient& client) {
  String request = client.readStringUntil('\r');
  client.flush();

  if (request.length() == 0) {
    sendError(client, 400, "Empty request");
    return;
  }

  int reqIndex = request.indexOf("/relay");
  if (reqIndex < 0) {
    sendError(client, 404, "Unknown endpoint");
    return;
  }

  RelayCommand cmd;
  if (!parseCommand(request, cmd)) {
    sendError(client, 400, "Invalid parameters");
    return;
  }

  bool result = executeCommand(cmd);
  if (!result) {
    sendError(client, 500, "Relay action failed");
    return;
  }

  sendOk(client, cmd);
}

bool parseCommand(const String& request, RelayCommand& cmd) {
  int qIndex = request.indexOf('?');
  if (qIndex < 0) return false;
  String query = request.substring(qIndex + 1);

  cmd.channel = -1;
  cmd.action = "";
  cmd.pulseMs = 500;

  int start = 0;
  while (start < query.length()) {
    int equalIdx = query.indexOf('=', start);
    if (equalIdx < 0) break;
    String key = query.substring(start, equalIdx);
    int ampIdx = query.indexOf('&', equalIdx + 1);
    if (ampIdx < 0) ampIdx = query.length();
    String value = query.substring(equalIdx + 1, ampIdx);

    key.toLowerCase();
    value.toLowerCase();

    if (key == "channel") {
      cmd.channel = value.toInt();
    } else if (key == "action") {
      cmd.action = value;
    } else if (key == "pulse") {
      cmd.pulseMs = value.toInt();
    }
    start = ampIdx + 1;
  }

  if (cmd.channel < 1 || cmd.channel > RELAY_COUNT) return false;
  if (cmd.action != "on" && cmd.action != "off" && cmd.action != "pulse") return false;
  if (cmd.action == "pulse" && cmd.pulseMs == 0) cmd.pulseMs = 500;

  return true;
}

bool executeCommand(const RelayCommand& cmd) {
  uint8_t pin = RELAY_PINS[cmd.channel - 1];
  bool ok = true;

  if (cmd.action == "on") {
    ok = setRelay(pin, RELAY_ON);
  } else if (cmd.action == "off") {
    ok = setRelay(pin, RELAY_OFF);
  } else if (cmd.action == "pulse") {
    ok = pulseRelay(pin, cmd.pulseMs);
  }

  if (ok) {
    feedbackSuccess();
  } else {
    feedbackError();
  }
  return ok;
}

bool setRelay(uint8_t pin, uint8_t state) {
  if (!isValidPin(pin)) return false;
  digitalWrite(pin, state);
  delay(10); // settle
  return digitalRead(pin) == state;
}

bool pulseRelay(uint8_t pin, unsigned long durationMs) {
  if (!isValidPin(pin)) return false;
  if (durationMs > 5000) durationMs = 5000; // safety limit

  digitalWrite(pin, RELAY_ON);
  delay(durationMs);
  digitalWrite(pin, RELAY_OFF);
  return true;
}

bool isValidPin(uint8_t pin) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (RELAY_PINS[i] == pin) return true;
  }
  return false;
}

void feedbackSuccess() {
  digitalWrite(LED_STATUS, HIGH);
  tone(BUZZER, 1800, 80);
  delay(100);
  digitalWrite(LED_STATUS, LOW);
}

void feedbackError() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_ERROR, HIGH);
    tone(BUZZER, 400, 120);
    delay(150);
    digitalWrite(LED_ERROR, LOW);
    delay(120);
  }
}

void sendOk(BridgeClient& client, const RelayCommand& cmd) {
  client.println("Status: 200 OK");
  client.println("Content-Type: application/json");
  client.println();
  client.print("{\"channel\":");
  client.print(cmd.channel);
  client.print(",\"action\":\"");
  client.print(cmd.action);
  client.print("\"");
  if (cmd.action == "pulse") {
    client.print(",\"pulseMs\":");
    client.print(cmd.pulseMs);
  }
  client.println("}");
}

void sendError(BridgeClient& client, int code, const char* message) {
  client.print("Status: ");
  client.print(code);
  client.println(" ERROR");
  client.println("Content-Type: application/json");
  client.println();
  client.print("{\"error\":\"");
  client.print(message);
  client.println("\"}");
  feedbackError();
}
