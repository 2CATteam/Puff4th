#ifndef taskSize
#define taskSize 10
#endif
#ifndef maxSpeed
#define maxSpeed 40000
#endif
#ifndef MotorController_h
#define MotorController_h
#include <Preferences.h>
class MotorController {
  public:
    //Constructor
    MotorController(int pulse, int dir, int done);
    //Runs on repeat, and does the actual sending pulses and stuff
    //void loopStep();
    //Verifies that a runVolume call wouldwork
    int verifyVolume(int count, float volume, float timeToDo, unsigned long timeBetween);
    //Sets up a run to do a certain number of puffs, at a certain speed, of a certain volume
    void runVolume(int count, float volume, float timeToDo, unsigned long timeBetween);
    //Checks whether a CSV command file can be executed
    int verifyCSV(const char* values);
    //Sets up a run from a CSV file
    void runCSV(char* values);
    //Go to start or end
    void goToStart();
    void goToEnd();
    //Setters and getters
    void setVolumeOfPipe(float volume);
    float getVolumeOfPipe();
    void setCalibrationFactor(float factor);
    float getCalibrationFactor();
    void setPreDelay(int preDelay);
    int getPreDelay();
    void setPeakHold(int peakHold);
    int getPeakHold();
    void getPreferences(char* out);
    //Variables
    //When this is 0, the motor is not running a function. Otherwise, it is running a function.
    volatile int state;
    //The total length of the ball screw
    volatile int lengthInPulses;
    Preferences pref;
  private:
    int volumeToPulses(float volume);
    int donePin;
    int lowLimitPin;
    int highLimitPin;
    //volatile int pulsesPerSecond;
    //volatile int pulsesLeft;
    unsigned long lastPulse;
};

struct volume_args {
  int count;
  float volume;
  float timeToDo;
  unsigned long timeBetween;
  MotorController* mc;
  TaskHandle_t functionHandle;
};

struct csv_args {
  char csvData[1000];
  MotorController* mc;
  TaskHandle_t functionHandle;
};
#endif
