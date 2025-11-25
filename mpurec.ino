#include <SoftwareSerial.h>

#define RX 11 //TX of HC-12 to Pin 11
#define TX 10 //RX of HC-12 to Pin 10

float g_receivedPGA = 0.0;
const uint8_t hc12_setPin = 6;

SoftwareSerial hc12(RX, TX);

void setup() {
  Serial.begin(9600);    // For local debugging
  hc12.begin(9600);      // HC12 communication
  Serial.println("Receiver Ready");
  hc12.println("Receiver Ready");
  pinMode(hc12_setPin, OUTPUT);
  digitalWrite(hc12_setPin, HIGH);
}

void loop() {
  // Check if data is available from the HC-12 module
  
  // if (hc12.available()) {
  //   char buffer[32];
  //   int index = 0;
    
  //   // Read the incoming data into the buffer
  //   while (hc12.available() && index < 31) {
  //     buffer[index++] = hc12.read();
  //     delay(1);  // Small delay to allow buffer to fill
  //   }
  //   buffer[index] = '\0';  // Null-terminate the string
    
  //   // Convert the received string to float
  //   g_receivedPGA = atof(buffer);
    
  //   // Print the received value
  //   earthquakeIntensity();
  // }

  if (hc12.available()) {
    // Read the entire line until the newline character
     String line = hc12.readStringUntil('\n');

    // Convert the String to a float
    g_receivedPGA = line.toFloat();

    // Print what we received (for debugging)
    Serial.print("Received PGA: ");
    Serial.println(g_receivedPGA);

    // Now process it
    earthquakeIntensity();
  }
}

void earthquakeIntensity() {
  const char* intensityLevels[] = {"IV", "V", "VI", "VII", "VIII", "IX", "X"};
  const float thresholds[] = {0.014, 0.04, 0.093, 0.19, 0.35, 1.01, 1.25};
  uint8_t intensity = 0;

  for (int i = 0; i < 7; i++) {
    if (g_receivedPGA < thresholds[i]) {
      break;
    }
    intensity = i;
  }

  Serial.print("Intensity: ");
  Serial.println(intensityLevels[intensity]);
  Serial.print("PGA: ");
  Serial.println(PGA, 4);
  
}