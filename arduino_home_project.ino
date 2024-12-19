#include <FastLED.h>

#define START_BYTE 0x7E
#define CMD_AMB_LIGHT_ON_OFF 0x00
#define CMD_PLANT_LIGHT_ON_OFF 0x01
#define MAX_CMD_CNT 2
#define MAX_PAYLOAD_LEN 1


#define NUM_LEDS    600         // Adjust this to the number of LEDs in your strip
#define BRIGHTNESS  100        // Set brightness level (0-255)
#define LED_TYPE    WS2812B    // Type of LED strip
#define COLOR_ORDER GRB        // Color order of the LED strip
//#define NUM_LED_COLORS 3
#define LIGHT_STRIP_COLOR CRGB(238, 175, 97)

#define AMB_POWER_PIN 2
#define PLANT_LIGHT_PIN 3
#define AMB_CTRL_PIN 4

// Abstract base class for commands
class Command {
public:
  virtual ~Command() {}
  virtual void initExecute() = 0;
  virtual void commandExecute(uint8_t payloadLength, uint8_t* payloadData) = 0;
};

// Registry of commands
Command* commandRegistry[MAX_CMD_CNT];

// Concrete command for plant light on/off
class PlantLightOnOffCommand : public Command {
public:
  void initExecute() override {
    pinMode(PLANT_LIGHT_PIN, OUTPUT);
    digitalWrite(PLANT_LIGHT_PIN, LOW);
  }

  void commandExecute(uint8_t payloadLength, uint8_t* payloadData) override {
    if (payloadLength == 1) {
      digitalWrite(PLANT_LIGHT_PIN, payloadData[0] ? HIGH : LOW);
    }
  }
};

struct AmbLightData {
  uint8_t leds_on;
  CRGB leds[NUM_LEDS];
  // Define an array of warm colors
  uint8_t colorIndex;
  uint8_t nextColorIndex;
  uint8_t blendAmount;
};

// Concrete command for ambient light on/off
class AmbLightOnOffCommand : public Command {
protected:
  AmbLightData* data;

public:
  AmbLightOnOffCommand() {
    data = new AmbLightData;
    data->colorIndex = 0;
    data->nextColorIndex = 1;
    data->blendAmount = 0;
  }

  void initExecute() override {
    FastLED.addLeds<LED_TYPE, AMB_CTRL_PIN, COLOR_ORDER>(data->leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    
    pinMode(AMB_POWER_PIN, OUTPUT);
    digitalWrite(AMB_POWER_PIN, HIGH);
  }

  void commandExecute(uint8_t payloadLength, uint8_t* payloadData) {
    if (payloadLength == 1) {
      data->leds_on = payloadData[0] ? 1 : 0;
      digitalWrite(AMB_POWER_PIN, payloadData[0] ? HIGH : LOW);
      delay(300);
      fill_solid(data->leds, NUM_LEDS, LIGHT_STRIP_COLOR);
      FastLED.show();
    }
  }
};

void setup() {
  uint8_t i;

  Serial.begin(9600);

  for (i = 0; i < MAX_CMD_CNT; i++) {
    commandRegistry[i] = nullptr;
  }

  // Register our commands
  commandRegistry[CMD_AMB_LIGHT_ON_OFF]   = new AmbLightOnOffCommand();
  commandRegistry[CMD_PLANT_LIGHT_ON_OFF] = new PlantLightOnOffCommand();

  // Call initExecute on all commands that support it
  for (i = 0; i < MAX_CMD_CNT; i++) {
    if (commandRegistry[i]) {
      commandRegistry[i]->initExecute();
    }
  }
}

void loop() {
  static uint8_t state = 0;
  static uint8_t command = 0;
  static uint8_t payloadLength = 0;
  static uint8_t payloadData[MAX_PAYLOAD_LEN] = {0};
  static uint8_t payloadIndex = 0;
  static uint8_t checksum = 0;
  uint8_t incomingByte;

  if (Serial.available() > 0) {
    incomingByte = Serial.read();

    switch (state) {
      case 0: // Waiting for start byte
        if (incomingByte == START_BYTE) {
          state = 1;
          checksum = incomingByte;
        }
        break;

      case 1: // Reading command byte
        command = incomingByte;
        checksum ^= incomingByte;
        state = 2;
        break;

      case 2: // Reading payload length
        payloadLength = incomingByte;
        checksum ^= incomingByte;
        if (payloadLength <= MAX_PAYLOAD_LEN) {
          payloadIndex = 0;
          state = (payloadLength == 0) ? 4 : 3;
        } else {
          state = 0;
        }
        break;

      case 3: // Reading payload data
        payloadData[payloadIndex++] = incomingByte;
        checksum ^= incomingByte;
        if (payloadIndex == payloadLength) {
          state = 4;
        }
        break;

      case 4: // Verifying checksum
        //if (incomingByte == checksum) {
          // Check if we have a registered command
          commandRegistry[command]->commandExecute(payloadLength, payloadData);
        //}
        state = 0; // Reset state machine
        break;

      default:
        state = 0;  // Reset to initial state
        break;
    }
  }
}