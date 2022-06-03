#define bufferSize 800
#define pulsePin 23
#define directionPin 21
//Note: Enable should be HIGH to disable the motor
#define enablePin 22
#define pulsesLeftPin 5
#define lowLimitPin 18
#define highLimitPin 19
int smootherIndex = 0;
int pulsesPerSecondSmooth[bufferSize];
int pulsesPerSecond = 0;
long actualPulsesPerSecond = 0;
char serialBuffer[10];
char serialIndex = 0;

int calibrationState = -1;
int pulsesLength;
int pulsesLeft;

unsigned long pulseMicros;
unsigned long updateMicros;

void setup() {
  // put your setup code here, to run once:
  pinMode(pulsePin, OUTPUT);
  pinMode(directionPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(pulsesLeftPin, OUTPUT);
  pinMode(lowLimitPin, INPUT_PULLUP);
  pinMode(highLimitPin, INPUT_PULLUP);
  digitalWrite(directionPin, LOW);
  digitalWrite(enablePin, LOW);
  digitalWrite(pulsesLeftPin, LOW);
  pulseMicros = micros();
  updateMicros = micros();
  for (int i = 0; i < bufferSize; i++) {
    pulsesPerSecondSmooth[i] = 0;
  }
  for (int i = 0; i < 10; i++) {
    serialBuffer[i] = 0;
  }

  Serial.begin(230400);
  Serial.println("Restarting");
  Serial2.begin(941176, SERIAL_8N1, 16, 17);
}

void loop() {
  calibrationLoop();
  if (Serial2.available() > 0) {
    char readChar = Serial2.read();
    Serial.print(readChar);
    if (readChar == '\n') {
      pulsesPerSecond = ((String) serialBuffer).toInt();
      Serial.println("Got new speed imperative");
      Serial.println(pulsesPerSecond);
      for (int i = 0; i < 10; i++) {
        serialBuffer[i] = 0;
      }
      serialIndex = 0;
      pulsesLeft = -1;
    } else if (readChar == '`') {
      pulsesLeft = ((String) serialBuffer).toInt();
      Serial.println("Got new pulse imperative");
      Serial.println(pulsesLeft);
      for (int i = 0; i < 10; i++) {
        serialBuffer[i] = 0;
      }
      serialIndex = 0;
    } else if (readChar == '|') {
      calibrationState = 0;
      for (int i = 0; i < 10; i++) {
        serialBuffer[i] = 0;
      }
      serialIndex = 0;
    } else {
      serialBuffer[serialIndex] = readChar;
      serialIndex++;
      serialIndex = serialIndex % 10;
    }
  }
  unsigned long timeNow = micros();
  if (timeNow - updateMicros > 1000) {
    //Serial.println(pulsesPerSecond);
    updateMicros += 1000;
    pulsesPerSecondSmooth[smootherIndex] = pulsesPerSecond;
    smootherIndex++;
    smootherIndex = smootherIndex % bufferSize;
    actualPulsesPerSecond = 0;
    for (int i = 0; i < bufferSize; i++) {
      actualPulsesPerSecond += pulsesPerSecondSmooth[i];
    }
    actualPulsesPerSecond /= bufferSize;
    if (actualPulsesPerSecond < 0) {
      digitalWrite(directionPin, LOW);
      actualPulsesPerSecond *= -1;
    } else if (actualPulsesPerSecond > 0) {
      digitalWrite(directionPin, HIGH);
    }
    if (actualPulsesPerSecond == 0) {
      digitalWrite(enablePin, HIGH);
      delayMicroseconds(100);
    } else {
      digitalWrite(enablePin, LOW);
      delayMicroseconds(100);
    }
  }
  if (pulsesLeft != 0
      && actualPulsesPerSecond != 0
      && timeNow - pulseMicros > 1000000 / actualPulsesPerSecond) {
    pulseMicros += 1000000 / actualPulsesPerSecond;
    delayMicroseconds(20);
    digitalWrite(pulsePin, HIGH);
    delayMicroseconds(20);
    digitalWrite(pulsePin, LOW);
    pulsesLeft--;
    pulsesLength++;
  }
  if (pulsesLeft == 0) {
    digitalWrite(pulsesLeftPin, LOW);
  } else {
    digitalWrite(pulsesLeftPin, HIGH);
  }
  if (digitalRead(lowLimitPin) == HIGH && pulsesPerSecond <= 0 && pulsesLeft != 0) {
    Serial.println("Hit the low limit!");
    pulsesLeft = 0;
    pulsesPerSecond = 0;
    for (int i = 0; i < bufferSize; i++) {
      pulsesPerSecondSmooth[i] = 0;
    }
    if (calibrationState == 0) {
      calibrationState = 1;
    } else if (calibrationState == 3) {
      calibrationState = -1;
    }
    //Back off the limit switch a bit, see if that does anything
    digitalWrite(directionPin, HIGH);
    delayMicroseconds(100);
    Serial.println("Backing off");
    for (int i = 0; i < 100; i++) {
      delayMicroseconds(2000);
      digitalWrite(pulsePin, HIGH);
      delayMicroseconds(20);
      digitalWrite(pulsePin, LOW);
    }
    delay(2);
    Serial.println("Backed off");
  }
  if (digitalRead(highLimitPin) == HIGH && pulsesPerSecond >= 0 && pulsesLeft != 0) {
    Serial.println("Hit the high limit!");
    pulsesLeft = 0;
    pulsesPerSecond = 0;
    for (int i = 0; i < bufferSize; i++) {
      pulsesPerSecondSmooth[i] = 0;
    }
    if (calibrationState == 2) {
      calibrationState = 3;
      Serial2.println(pulsesLength - 10);
    }
    //Back off the limit switch a bit, see if that does anything
    digitalWrite(directionPin, LOW);
    delayMicroseconds(100);
    Serial.println("Backing off");
    for (int i = 0; i < 100; i++) {
      delayMicroseconds(2000);
      digitalWrite(pulsePin, HIGH);
      delayMicroseconds(20);
      digitalWrite(pulsePin, LOW);
    }
    delay(2);
    Serial.println("Backed off");
  }
}

void calibrationLoop() {
  if (calibrationState == -1) return;
  if (calibrationState == 0) {
    pulsesLeft = -1;
    pulsesPerSecond = -800;
  } else if (calibrationState == 1) {
    pulsesLength = 0;
    pulsesLeft = -1;
    pulsesPerSecond = 800;
    calibrationState = 2;
  } else if (calibrationState == 3) {
    pulsesLeft = -1;
    pulsesPerSecond = -800;
  }
}
