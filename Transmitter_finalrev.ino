#include <Wire.h>
#include <MPU6050.h>
#include <SoftwareSerial.h>

#define RX 11 // Arduino RX (Pin 11) -> HC-12 TX
#define TX 10 // Arduino TX (Pin 10) -> HC-12 RX

MPU6050 mpu;
SoftwareSerial hc12(RX, TX);

// NO LONGER A CONST. We will calculate this.
float ACCEL_SENSITIVITY = 16384.0; 

const float EARTHQUAKE_THRESHOLD = 0.014;
const uint8_t MPU_INTERRUPT_PIN = 2;
const uint8_t COMBI_PIN = 7;

volatile bool mpuInterrupt = false;
volatile float currentPGA = 0.0;

unsigned long lastAlertTime = 0;
const unsigned long alertCooldown = 2000;

float g_offsetX = 0.0;
float g_offsetY = 0.0;
float g_offsetZ = 0.0;

void setup() {
  Wire.begin();
  Serial.begin(9600);
  hc12.begin(9600);
  pinMode(MPU_INTERRUPT_PIN, INPUT);
  pinMode(COMBI_PIN, OUTPUT);
  digitalWrite(COMBI_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(MPU_INTERRUPT_PIN), dmpDataReady, RISING);
  Serial.println("Initializing MPU6050...");
  mpu.initialize();
  Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
  mpu.setClockSource(MPU6050_CLOCK_PLL_EXT32K);
  mpu.setDHPFMode(MPU6050_DHPF_5);

  findGravityOffsetsAndSensitivity(); //Calibration function

  mpu.setMotionDetectionThreshold(7);
  mpu.setMotionDetectionDuration(2);
  mpu.setIntMotionEnabled(true);
  mpu.setIntFreefallEnabled(false);
  mpu.setIntZeroMotionEnabled(false);
  Serial.println("\nEarthquake detection system ready.");
}

void dmpDataReady() {
  mpuInterrupt = true;
}

void findGravityOffsetsAndSensitivity() {
  Serial.println("Hard-resetting offsets to 0...");
  mpu.setXAccelOffset(0);
  mpu.setYAccelOffset(0);
  mpu.setZAccelOffset(0);

  Serial.println("--- IMPORTANT ---");
  Serial.println("Place device in its FINAL UPRIGHT position on the pillar.");
  Serial.println("Calibrating in 5 seconds... DO NOT MOVE.");
  delay(5000);

  Serial.println("Calibrating... taking 1000 readings...");

  long sumAX = 0, sumAY = 0, sumAZ = 0;
  int16_t ax, ay, az;
  for (int i = 0; i < 1000; i++) {
    mpu.getAcceleration(&ax, &ay, &az);
    sumAX += ax;
    sumAY += ay;
    sumAZ += az;
    delay(2);
  }

  // --- NEW SENSITIVITY CALCULATION ---
  
  // 1. Find the average RAW vector
  // use "double" for high precision
  double avgAX = sumAX / 1000.0;
  double avgAY = sumAY / 1000.0;
  double avgAZ = sumAZ / 1000.0;

  // 2. Calculate the magnitude of that raw vector
  // This magnitude, by definition, IS 1.0g of gravity.
  double rawMag = sqrt(sq(avgAX) + sq(avgAY) + sq(avgAZ));

  // 3. Set this as new sensitivity
  ACCEL_SENSITIVITY = (float)rawMag;

  // ---------------------------------

  // Now, calculate the offsets IN G's, using new sensitivity
  g_offsetX = avgAX / ACCEL_SENSITIVITY;
  g_offsetY = avgAY / ACCEL_SENSITIVITY;
  g_offsetZ = avgAZ / ACCEL_SENSITIVITY;

  Serial.println("Calibration complete.");
  Serial.print("NEW Self-Calibrated Sensitivity: ");
  Serial.println(ACCEL_SENSITIVITY);
  
  Serial.println("Resting offsets (in g):");
  Serial.print("X: "); Serial.println(g_offsetX, 4);
  Serial.print("Y: "); Serial.println(g_offsetY, 4);
  Serial.print("Z: "); Serial.println(g_offsetZ, 4);
  
  // The magnitude of these 3 offsets should now be 1.0
  Serial.print("Offset Magnitude (should be 1.0): ");
  Serial.println(sqrt(sq(g_offsetX) + sq(g_offsetY) + sq(g_offsetZ)));
}

void calculatePGA(int16_t ax, int16_t ay, int16_t az) {
  float xAccel = ax / ACCEL_SENSITIVITY;
  float yAccel = ay / ACCEL_SENSITIVITY;
  float zAccel = az / ACCEL_SENSITIVITY;
  float dynamicX = xAccel - g_offsetX;
  float dynamicY = yAccel - g_offsetY;
  float dynamicZ = zAccel - g_offsetZ;
  currentPGA = sqrt(sq(dynamicX) + sq(dynamicY) + sq(dynamicZ));
}

void earthquakeIntensity() {
  const char* intensityLevels[] = {"IV", "V", "VI", "VII", "VIII", "IX", "X"};
  const float thresholds[] = {0.014, 0.04, 0.093, 0.19, 0.35, 1.01, 1.25};
  uint8_t intensity = 0;
  for (int i = 0; i < 7; i++) {
    if (currentPGA < thresholds[i]) {
      break;
    }
    intensity = i;
  }
  Serial.print("Intensity: ");
  Serial.println(intensityLevels[intensity]);
  Serial.print("PGA: ");
  Serial.println(currentPGA, 4);
  hc12.println(currentPGA, 4);
}

void accelerometerOutput(int16_t ax, int16_t ay, int16_t az) {
  Serial.println("--- Accelerometer Output ---");
  Serial.print("Raw: X="); Serial.print(ax);
  Serial.print(", Y="); Serial.print(ay);
  Serial.print(", Z="); Serial.println(az);
  float xAccel = ax / ACCEL_SENSITIVITY;
  float yAccel = ay / ACCEL_SENSITIVITY;
  float zAccel = az / ACCEL_SENSITIVITY;
  Serial.print("g (Raw): X="); Serial.print(xAccel, 4);
  Serial.print(", Y="); Serial.print(yAccel, 4);
  Serial.print(", Z="); Serial.println(zAccel, 4);
  Serial.print("PGA (Dynamic): ");
  Serial.println(currentPGA, 4);
  Serial.println();
}

void processMotionInterrupt() {
  mpu.getIntStatus();
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  calculatePGA(ax, ay, az);
  if (currentPGA > EARTHQUAKE_THRESHOLD) {
    if (millis() - lastAlertTime > alertCooldown) {
      lastAlertTime = millis();
      digitalWrite(COMBI_PIN, LOW);
      Serial.println("!!! EARTHQUAKE DETECTED !!!");
      earthquakeIntensity();
      accelerometerOutput(ax, ay, az);
    }
  }
  else {
    digitalWrite(COMBI_PIN, HIGH);
  }
}

void loop() {
  if (mpuInterrupt) {
    mpuInterrupt = false;
    processMotionInterrupt();
  }
}
