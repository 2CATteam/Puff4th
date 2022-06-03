#include "Arduino.h"
#include "motorController.h"
#include <thread>
#include <chrono>
#include <string>
#include <mutex>

MotorController::MotorController(int low, int high, int done) {
  lowLimitPin = low;
  highLimitPin = high;
  donePin = done;
  pinMode(lowLimitPin, INPUT_PULLUP);
  pinMode(highLimitPin, INPUT_PULLUP);
  pinMode(donePin, INPUT);
  state = 0;
  lengthInPulses = 4000;
}

int MotorController::verifyVolume(int count, float volume, float timeToDo, unsigned long timeBetween) {
  Serial.println("Got here 1");
  
  //Do math to see how many pulses per second it will take
  int pulsesPerPuff = volumeToPulses(volume);
  if (timeToDo == 0) {
    Serial.println("timeToDo was 0");
    return 1;
  }
  int localPulsesPerSecond = static_cast<int>(pulsesPerPuff / timeToDo);

  Serial.println("Got here 2");

  //Check if the operation is given enough time
  if (localPulsesPerSecond > maxSpeed) {
    Serial.println("Request didn't give enough time to run this puff");
    return 1;
  }
  //Each puff consists of running for fudge, running one direction, waiting getPeakHold() milliseconds, running for fudge, running one direction.
  if (timeBetween < timeToDo * 2 + getPreDelay() * 2 + getPeakHold()) {
    Serial.println("Request didn't give enough time between");
    return 2;
  }
  Serial.println("Got here 3");
  return 0;
}

void MotorController::runVolume(int count, float volume, float timeToDo, unsigned long timeBetween) {
  Serial.println("Got here 1");
  //Set the semaphore
  state = 1;
  
  //Do math to see how many pulses per second it will take
  int pulsesPerPuff = volumeToPulses(volume);
  Serial.println(pulsesPerPuff);
  int localPulsesPerSecond = static_cast<int>(pulsesPerPuff / timeToDo);
  Serial.println(localPulsesPerSecond);

  //Check if the operation is given enough time
  if (localPulsesPerSecond > maxSpeed) {
    Serial.println("Request didn't give enough time to run this puff");
    Serial2.print("0");
    Serial2.print("\n");
    state = 0;
    return;
  }
  //Each puff consists of running for fudge, running one direction, waiting getPeakHold() milliseconds, running for fudge, running one direction.
  if (timeBetween < timeToDo * 2 + getPreDelay() * 2 + getPeakHold()) {
    Serial.println("Request didn't give enough time between");
    Serial2.print("0");
    Serial2.print("\n");
    state = 0;
    return;
  }

  Serial.println("Got here 2");

  //Fully exhaust by going down until we hit the limit switch
  Serial2.print("-400");
  Serial2.print("\n");

  delay(20);

  //Wait until done with the intake
  while (digitalRead(donePin) == HIGH) {
    std::this_thread::yield();
  }

  //For each puff
  for (int i = 0; i < count; i++) {
    Serial.print("Got here ");
    Serial.println(i);
    //Set the speed (which starts the puff) and wait for the amount of fudge time
    unsigned long start = millis();
    Serial2.print("\n");
    Serial2.print(static_cast<int>(localPulsesPerSecond *= getCalibrationFactor()));
    Serial2.print("\n");
    Serial2.flush();
    Serial.println("Starting motor for intake");
    std::this_thread::sleep_for (std::chrono::milliseconds(getPreDelay()));
    //Now, set the pulsesLeft, because we actually want to count how many we go this time
    Serial.println("Starting to count intake pulses");
    Serial2.print(static_cast<int>(pulsesPerPuff /= getCalibrationFactor()));
    Serial2.print("`");
    Serial2.flush();
    delay(1000);

    //Wait until done with the intake
    while (digitalRead(donePin) == HIGH) {
      std::this_thread::yield();
    }

    //Wait some number of milliseconds at the peak
    Serial2.print("0");
    Serial2.print("\n");
    std::this_thread::sleep_for (std::chrono::milliseconds(getPeakHold()));

    //Now do the same for the exhaust
    Serial2.print(static_cast<int>(volumeToPulses(-1)));
    Serial2.print("\n");
    Serial2.flush();
    Serial.println("Starting motor for exhaust");
    std::this_thread::sleep_for (std::chrono::milliseconds(getPreDelay()));
    //Now, wait until we hit the limit switch at the base (which set pulsesLeft to 0) (so errors don't propagate)
    while (digitalRead(donePin) == HIGH) {
      std::this_thread::yield();
    }

    Serial2.print("0");
    Serial2.print("\n");

    //Wait until it's time to start the next one
    while (millis() - start < timeBetween) {
      std::this_thread::yield();
    }
  }
  state = 0;
}

//Verify that a CSV file is valid. Return 0 if valid, 1 if invalid format, 2 if too slow, 3 if invalid time progression
int MotorController::verifyCSV(const char* values) {
  char* valuesCopy;
  //Wrap it in a try-catch block to catch format errors
  try {
    //Declare variables for keeping track of delta-time and delta-position
    int currentTime, currentPosition;
    int lastPosition = 0;
    int lastTime = 0;

    //Copy the string
    valuesCopy = strdup(values);
    
    //Declare variable for the time and read in the first value
    char* timeToken = strtok(valuesCopy, ",");
    char* speedToken = strtok(NULL, "_");
    float speedRequested;
    lastTime = static_cast<int>(std::stof(timeToken) * 1000);
    timeToken = strtok(valuesCopy, ",");
    
    //Go until we get to the end
    while (timeToken != NULL) {
      //Interpret the current time as millis
      currentTime = static_cast<int>(std::stof(timeToken) * 1000);
      
      //Convert L/min to L/millisecond to L to pulses
      speedRequested = std::stof(speedToken);
      speedRequested *= getCalibrationFactor();
      if (speedRequested == 0) {
        speedRequested = -40;
      }
      currentPosition = volumeToPulses(speedRequested / 60 / 1000 * (currentTime - lastTime));
      if (currentPosition < 0) {
        currentPosition = 0;
      }
      
      //If the new time goes back in time, throw an error
      if (currentTime - lastTime < 0) {
        Serial.println("Time moves forward");
        free(valuesCopy);
        return 3;
      }

      if (currentTime == lastTime && currentPosition != lastPosition) {
        Serial.println("Too fast");
        free(valuesCopy);
        return 2;
      }
      
      //If it wants us to go too fast throw an error
      if (currentTime != lastTime
          && (currentPosition - lastPosition) / (currentTime - lastTime) * (currentPosition > lastPosition ? 1 : -1)> maxSpeed) {
        Serial.println("Too fast");
        free(valuesCopy);
        return 2;
      }

      if (currentPosition > lengthInPulses) {
        Serial.println("Went too far!");
        free(valuesCopy);
        return 4;
      }
      
      //Prime for next loop
      lastPosition = currentPosition;
      lastTime = currentTime;
      speedToken = strtok(NULL, "_");
      timeToken = strtok(NULL, ",");
    }
    Serial.println("Finished verifying CSV file");
    free(valuesCopy);
    return 0;
  } catch(...) {
    Serial.println("Invalid format");
    free(valuesCopy);
    return 1;
  }
}

//Sets up a run from a CSV file
void MotorController::runCSV(char* valuesCopy) {
  
  //Set the semaphore
  state = 0;
  //Catch errors
  try {
    Serial.println(valuesCopy);
    //Declare variables
    int currentTime;
    int lastTime = 0;
    
    //Get the first set of values
    char* timeToken = strtok(valuesCopy, ",");
    lastTime = static_cast<int>(std::stof(timeToken) * 1000);
    char* speedToken = strtok(NULL, "_");
    timeToken = strtok(NULL, ",");
    float requestedSpeed;
    
    //Go to zero SLOWLY
    
    //Go to the starting position, and note that we're there
    Serial2.print(-200);
    Serial2.print("\n");
    delay(10);
    //Wait until the operation completes
    while (digitalRead(donePin) == HIGH) {
      delay(10);
    }

    //Until we run out of values
    while (timeToken != NULL) {
      //Note when it wants us to be where
      currentTime = static_cast<int>(std::stof(timeToken) * 1000);
      requestedSpeed = std::stof(speedToken);
      requestedSpeed *= getCalibrationFactor();
      if (requestedSpeed == 0) {
        requestedSpeed = -40;
      }

      //Set the proper number of pulses per second
      //Convert to pulses per second
      Serial2.print(static_cast<int>(volumeToPulses(requestedSpeed / 60)));
      Serial2.print("\n");
      Serial.println(static_cast<int>(volumeToPulses(requestedSpeed / 60)));
      Serial.print("Waiting for ");
      Serial.println(currentTime - lastTime);

      //Chill until it's time to go again
      delay(currentTime - lastTime);
      //Don't pulse while we're thinking
      Serial2.print("0");
      Serial2.print("\n");

      //Go for the next round
      lastTime = currentTime;
      speedToken = strtok(NULL, "_");
      timeToken = strtok(NULL, ",");
    }
    state = 0;
    Serial2.print(0);
    Serial2.print("\n");
    Serial.println("Finished running CSV file");
  } catch(...) {
    state = 0;
    Serial2.print(0);
    Serial2.print("\n");
    Serial.println("Invalid format and we didn't catch it");
  }
}

float MotorController::getVolumeOfPipe() {
  pref.begin("mc", false);
  float toReturn = pref.getFloat("volume", 3.0);
  pref.end();
  return toReturn;
}

void MotorController::setVolumeOfPipe(float volume) {
  pref.begin("mc", false);
  pref.putFloat("volume", volume);
  pref.end();
}

float MotorController::getCalibrationFactor() {
  pref.begin("mc", false);
  float toReturn = pref.getFloat("factor", 1.0);
  pref.end();
  return toReturn;
}

void MotorController::setCalibrationFactor(float factor) {
  pref.begin("mc", false);
  pref.putFloat("factor", factor);
  pref.end();
}

int MotorController::getPreDelay() {
  pref.begin("mc", false);
  int toReturn = pref.getInt("preDelay", 100);
  pref.end();
  return toReturn;
}

void MotorController::setPreDelay(int preDelay) {
  pref.begin("mc", false);
  pref.putInt("preDelay", preDelay);
  pref.end();
}

int MotorController::getPeakHold() {
  pref.begin("mc", false);
  int toReturn = pref.getInt("peakHold", 100);
  pref.end();
  return toReturn;
}

void MotorController::setPeakHold(int peakHold) {
  pref.begin("mc", false);
  pref.putInt("peakHold", peakHold);
  pref.end();
}

void MotorController::getPreferences(char* out) {
  Serial.println("Getting preferences:");
  Serial.println(out);
  strcpy(out, "");
  strcat(out, "volume=");
  strcat(out, std::to_string(getVolumeOfPipe()).c_str());
  strcat(out, "&factor=");
  strcat(out, std::to_string(getCalibrationFactor()).c_str());
  strcat(out, "&delay=");
  strcat(out, std::to_string(getPreDelay()).c_str());
  strcat(out, "&peak=");
  strcat(out, std::to_string(getPeakHold()).c_str());
  Serial.println(out);
}

void MotorController::goToStart() {
  Serial2.print(-400);
  Serial2.print("\n");
  Serial2.print(lengthInPulses);
  Serial2.print("`");
}

void MotorController::goToEnd() {
  Serial2.print(400);
  Serial2.print("\n");
  Serial2.print(lengthInPulses);
  Serial2.print("`");
}

int MotorController::volumeToPulses(float volume) {
  return static_cast<int>(volume / getVolumeOfPipe() * lengthInPulses);
}
