#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"
#include <LittleFS.h>
#include "motorController.h"
#include <string>
#include <thread>

WebServer server(80);
MotorController mc(18, 19, 5);

struct volume_args va;
struct csv_args ca;

char serialBuffer[10];
int serialIndex = 0;

void setup() {
  Serial.begin(230400);
  Serial.println("Beginning setup");
  Serial2.begin(941176, SERIAL_8N1, 16, 17);
  Serial2.print("0");
  Serial2.flush();
  delay(5000);
  //Begin setup
  Serial.println("Doin stuff");
  WiFi.mode(WIFI_AP);
  IPAddress apIP(192, 168, 0, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Puff3rd 2.0");
  server.begin();
  server.on("/", root);
  server.on("/main.html", root);
  server.on("/main.css", mainCss);
  server.on("/main.js", mainJs);
  server.on("/bootstrap.css", bootCss);
  server.on("/bootstrap.js", bootJs);
  server.on("/favicon.ico", favicon);
  server.on("/csvRun", csvRun);
  server.on("/volumeRun", volumeRun);
  server.on("/startRun", startRun);
  server.on("/endRun", endRun);
  server.on("/killRuns", killRuns);
  server.on("/getPrefs", getPrefs);
  Serial.println("Set up server");
  LittleFS.begin();
  Serial.println("Started LittleFS");
  delay(2000);
  initialCalibration();
  Serial.println("Did calibration");
  xTaskCreate(serverLoop, "serverLoop", 10000, NULL, 0, NULL);
  Serial.println("Spawned server thread");
  Serial.println("Done with setup");
  for (int i = 0; i < 10; i++) {
    serialBuffer[i] = 0;
  }
}


void loop() {
  delay(100);
  if (Serial2.available() > 0) {
    char readChar = Serial2.read();
    Serial.println(readChar);
    if (readChar == '\n') {
      mc.lengthInPulses = ((String) serialBuffer).toInt();
      for (int i = 0; i < 10; i++) {
        serialBuffer[i] = 0;
      }
      serialIndex = 0;
    } else {
      serialBuffer[serialIndex] = readChar;
      serialIndex++;
    }
  }
}

void serverLoop(void* params) {
  Serial.print("Starting server loop on core #");
  Serial.println(xPortGetCoreID());
  while (true) {
    server.handleClient();
    std::this_thread::sleep_for (std::chrono::milliseconds(10));
  }
}

void initialCalibration() {
  Serial2.print("|");
  Serial2.flush();
  Serial.println("Just sent the thing");
}

void root() {
  File file = LittleFS.open("/main.html", "r");
  server.streamFile(file, "text/html");
  file.close();
}

void mainCss() {
  File file = LittleFS.open("/main.css", "r");
  server.streamFile(file, "text/css");
  file.close();
}

void mainJs() {
  File file = LittleFS.open("/main.js", "r");
  server.streamFile(file, "text/javascript");
  file.close();
}

void bootCss() {
  File file = LittleFS.open("/bootstrap.css", "r");
  server.streamFile(file, "text/css");
  file.close();
}

void bootJs() {
  File file = LittleFS.open("/bootstrap.js", "r");
  server.streamFile(file, "text/javascript");
  file.close();
}

void favicon() {
  File file = LittleFS.open("/favicon.ico", "r");
  server.streamFile(file, "text/javascript");
  file.close();
}

void csvRun() {
  mc.setVolumeOfPipe(std::stof(server.arg("volume").c_str()));
  int result = mc.verifyCSV(server.arg("data").c_str());
  Serial.println(server.arg("data"));
  if (result == 3) {
    server.send(200, "text/plain", "Time error");
  } else if (result == 2) {
    server.send(200, "text/plain", "Too fast");
  } else if (result == 1) {
    server.send(200, "text/plain", "Invalid format");
  } else if (result == 4) {
    server.send(200, "text/plain", "Goes over the end");
  } else {
    server.send(200, "text/plain", "Doing it");
    ca.mc = &mc;
    strcpy(ca.csvData, server.arg("data").c_str());
    xTaskCreate(startCSV, "CSVTask", 10000, (void*) &ca, 0, &ca.functionHandle);
  }
}

void volumeRun() {
  Serial.println("Got here 1");
  mc.setVolumeOfPipe(std::stof(server.arg("volume").c_str()));
  va.mc = &mc;
  Serial.println("Got here 2");
  va.count = std::stoi(server.arg("count").c_str());
  va.volume = std::stof(server.arg("runVolume").c_str());
  va.timeToDo = std::stof(server.arg("timeToDo").c_str());
  va.timeBetween = std::stoul(server.arg("timeBetween").c_str());
  Serial.println("Got here 3");
  int fudgeMilliseconds = std::stoi(server.arg("fudge").c_str());
  mc.setPreDelay(fudgeMilliseconds);
  Serial.println("Got here 4");
  float fudgeFactor = std::stof(server.arg("factor").c_str());
  mc.setCalibrationFactor(fudgeFactor);
  Serial.println("Got here 5");
  int holdMilliseconds = std::stoi(server.arg("peakHold").c_str());
  mc.setPeakHold(holdMilliseconds);
  Serial.println("Got here 6");
  
  int result = mc.verifyVolume(va.count, va.volume, va.timeToDo, va.timeBetween);

  if (result == 2) {
    server.send(200, "text/plain", "More time");
  } else if (result == 1) {
    server.send(200, "text/plain", "Too fast");
  } else {
    server.send(200, "text/plain", "Doing it");
    if (mc.state == 0) {
      xTaskCreate(startVolume, "VolumeTask", 10000, (void*) &va, 0, &va.functionHandle);
    }
  }
}

void killRuns() {
  if (va.functionHandle) {
    vTaskDelete(va.functionHandle);
  }
  if (ca.functionHandle) {
    vTaskDelete(ca.functionHandle);
  }
  Serial2.print(0);
  Serial2.print("\n");
  mc.state = 0;
  server.send(200, "text/plain", "Doing it");
}

void startRun() {
  mc.goToStart();
  server.send(200, "text/plain", "Doing it");
}

void endRun() {
  mc.goToEnd();
  server.send(200, "text/plain", "Doing it");
}

void getPrefs() {
  char preferenceData[200];
  Serial.println(preferenceData);
  mc.getPreferences(preferenceData);
  Serial.println(preferenceData);
  server.send(200, "text/plain", preferenceData);
}

void startVolume(void* pvParams) {
  volume_args* params = (volume_args*) pvParams;
  params->mc->runVolume(params->count, params->volume, params->timeToDo, params->timeBetween);
  vTaskDelete(params->functionHandle);
}

void startCSV(void* pvParams) {
  csv_args* params = (csv_args*) pvParams;
  params->mc->runCSV(params->csvData);
  vTaskDelete(params->functionHandle);
}
