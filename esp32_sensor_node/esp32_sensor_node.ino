#define BLYNK_TEMPLATE_ID "TMPL6N48AugjW"
#define BLYNK_TEMPLATE_NAME "Teeiihaiturtangjai"
#define BLYNK_DEVICE_NAME "ESP32"
#define BLYNK_AUTH_TOKEN "qr__xOLsc6w98AaKNetr94gvXAek0Eac"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <TaskScheduler.h>

#define V_REF 3.3            // Reference voltage (3.3V for ESP32)
#define DB_THRESHOLD 40      // Decibel threshold (you can adjust this value)
#define DHTPIN 4
#define DHTTYPE DHT22
#define hcsrPin 34
#define buzzerPin 5
#define soundSensorPin 32

DHT dht(DHTPIN, DHTTYPE);
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "whanwhan";
char pass[] = "whanwhanjubjub";

float temp = 0;
float lastTemp = 0;
const int tempVirtualPin = V2;

float humidity = 0;
float lastHumidity = 0;
const int humidityVirtualPin = V3;

int motion = 0;                 // variable for reading the pin status
int lastMotion = 0;
const int motionVirtualPin = V1;

float db = 0;
float lastDb = 0;
const int dbVirtualPin = V4;

float airQuality = 0;
float lastAirQuality = 0;
const int airQualityVirtualPin = V7;

String airQualityLabel = "";
String lastAirQualityLabel = "";
const int airQualityLabelVirtualPin = V8;

int pinValue = 0;
int dur = 1000;
const int sampleWindow = 50;                              // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;

const int MQ135_SENSOR_PIN = 35;
int sensitivity = 200;  // Adjust this value based on your calibration

// Tasks using Scheduler
Scheduler runner;

// Tasks
void readSensors();
void soundSensor();
void checkMotion();
void turnOffBuzzer();
void checkAirQuality();

Task taskReadSensors(3000, TASK_FOREVER, &readSensors); // Every 2 seconds
Task taskCheckMotion(500, TASK_FOREVER, &checkMotion);  // Every 100 ms
Task taskTurnOffBuzzer(1000, TASK_ONCE, &turnOffBuzzer); // 1-second buzzer task
Task taskSoundSensor(500, TASK_FOREVER, &soundSensor);
Task taskCheckAirQuality(500, TASK_FOREVER, &checkAirQuality);

void setup() {
  Serial.begin(115200);

  Serial.println("Starting!");
  dht.begin();
  Serial.println("Initialized DHT!");

  Blynk.begin(auth, ssid, pass);
  if(Blynk.connected()) {
    Serial.println("Blynk connected!");
    Serial.flush();
  }

  pinMode(hcsrPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(MQ135_SENSOR_PIN, INPUT);

  digitalWrite(buzzerPin, HIGH);  // Active low -> HIGH means OFF

  // Initialize tasks
  runner.addTask(taskReadSensors);
  runner.addTask(taskCheckMotion);
  runner.addTask(taskTurnOffBuzzer);
  runner.addTask(taskSoundSensor);
  runner.addTask(taskCheckAirQuality);

  // Enable tasks
  taskReadSensors.enable();
  taskCheckMotion.enable();
  taskSoundSensor.enable();
  taskCheckAirQuality.enable();
}

BLYNK_WRITE(V5) // this command is listening when something is written to V1
{
  pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  Serial.print("V5 button value is: "); // printing value to serial monitor
  Serial.println(pinValue);
}

void loop() {
  runner.execute();  // Run the scheduler
  Blynk.run();       // Keep Blynk running
}

void soundSensor() {
  unsigned long startMillis= millis();                   // Start of sample window
  float peakToPeak = 0;                                  // peak-to-peak level
 
  unsigned int signalMax = 0;                            //minimum value
  unsigned int signalMin = 1024;                         //maximum value
                                                          // collect data for 50 mS
  while (millis() - startMillis < sampleWindow)
  {
     sample = analogRead(soundSensorPin);                    //get reading from microphone
     if (sample < 1024)                                  // toss out spurious readings
     {
        if (sample > signalMax)
        {
           signalMax = sample;                           // save just the max levels
        }
        else if (sample < signalMin)
        {
           signalMin = sample;                           // save just the min levels
        }
     }
  }
 
     peakToPeak = signalMax - signalMin;
   int db;

   if(peakToPeak == 4294966272.00){
    db = 0;
   } else {
    db = map(peakToPeak,20,900,49.5,90);                  //calibrate for deciBels
   }

  if(db != lastDb){
    Blynk.virtualWrite(dbVirtualPin, db);

    lastDb = db;
    
//    Serial.print("Loudness: ");
//    Serial.print(db);
//    Serial.println("dB");
  }
}

void readSensors() {
  temp = dht.readTemperature();
  humidity = dht.readHumidity();

  if(temp != lastTemp){
    Blynk.virtualWrite(tempVirtualPin, temp);     // Send temperature
    
    lastTemp = temp;
    
//    Serial.print("Temp: ");
//    Serial.print(temp);
  }
  
  if(humidity != lastHumidity){
    Blynk.virtualWrite(humidityVirtualPin, humidity); // Send humidity
    
    lastHumidity = humidity;
    
//    Serial.print(" Celsius | Humidity: ");
//    Serial.print(humidity);
//    Serial.println(" %");
  }
}

void checkMotion() {
  motion = digitalRead(hcsrPin);  // Read motion sensor
  
   if (motion != lastMotion) {
    Blynk.virtualWrite(motionVirtualPin, motion);   // Send motion state
    
    if (motion == HIGH) {
      if (lastMotion == LOW) { // Motion detected, change pirState if it was LOW
//        Serial.println("Motion detected!");
        if (pinValue == 1){
          // do something when button is pressed;
          digitalWrite(buzzerPin, LOW); // Turn on buzzer (active low)
          taskTurnOffBuzzer.restartDelayed(0); // Start turn-off task
          Serial.print("V5 button value is: "); // printing value to serial monitor
          Serial.println(pinValue);
        }
       
        lastMotion = HIGH; // Update state to indicate motion
      }
    } else {
      if (lastMotion == HIGH) { // Motion ended, change pirState if it was HIGH
//        Serial.println("Motion ended!");
        lastMotion = LOW; // Update state to indicate motion ended
      }
    }
   }
}

void turnOffBuzzer() {
  digitalWrite(buzzerPin, HIGH); // Turn off buzzer
//  Serial.println("Buzzer turned off.");
}

String interpretAirQuality(int sensor_value) {
  if (sensor_value < 50) {
    return String("Excellent");
  } else if (sensor_value < 100) {
    return String("Good");
  } else if (sensor_value < 150) {
    return String("Moderate");
  } else if (sensor_value < 200) {
    return String("Poor");
  } else {
    return String("Dangerous");
  }
}

void checkAirQuality() {
  int sensorValue = analogRead(MQ135_SENSOR_PIN);
  airQuality = sensorValue * sensitivity / 1023;
  airQualityLabel = interpretAirQuality(airQuality);
  
//  Serial.print("Sensor Value: ");
//  Serial.println(sensorValue);

  if(airQuality != lastAirQuality){
    Blynk.virtualWrite(airQualityVirtualPin, airQuality);     // Send temperature
    
    lastAirQuality = airQuality;
    
//    Serial.print("Air Quality Index (Calibrated): ");
//    Serial.println(airQuality);
  }
  
  if(airQualityLabel != lastAirQualityLabel){
    Blynk.virtualWrite(airQualityLabelVirtualPin, airQualityLabel); // Send humidity
    
    lastAirQualityLabel = airQualityLabel;
   
//    Serial.print("Air Quality: ");
//    Serial.println(airQualityLabel);
  }
}
