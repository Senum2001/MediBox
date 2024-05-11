// 210150V Dodangoda D.K.S.J.

// Libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h> // Newly added
#include <ESP32Servo.h> //Newly added
#include <NTPClient.h>
#include <WiFiUdp.h>


//OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0X3C

//Input and output pins
#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12
#define LDR1 A0
#define LDR2 A3
#define SERVMO 18

//Parameters related to light intensity -  Assignment 2
//------------------------------------------------------------------------

float GAMMA = 0.75;
const float RL10 = 50;
float MIN_ANGLE = 30;
float lux = 0;
String luxName;
//------------------------------------------------------------------------

//time initialization
#define NTP_SERVER     "pool.ntp.org"
int UTC_OFFSET = 5*60*60 + 30*60;;
#define UTC_OFFSET_DST 0
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//------------------------------------------------------------------------

//Wifi and mqtt clients -  Assignment 2
WiFiClient espClient;
PubSubClient mqttClient(espClient);


// to store light and temperature data to send through mqtt -  Assignment 2
char tempAr[10];
char lightAr[10];
//------------------------------------------------------------------------

DHTesp dhtSensor;

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool isScheduledON = false;
unsigned long scheduledOnTime;

bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {0, 1, 2};
int alarm_minutes[] = {1, 10, 25};
bool alarm_triggered[] = {false, false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3","5 - Disable Alarms"};

//servo moter initializing position -  Assignment 2
int pos = 0;
Servo servo; 

void setup() {
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(LDR1, INPUT);
  pinMode(LDR2, INPUT);
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  servo.attach(SERVMO, 500, 2400);

  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.display();
  delay(2000);


  setupWifi();
  setupMqtt(); // New added  -  Assignment 2

  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600);
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();

  print_line("Welcome", 25, 0, 2);
  print_line("to", 50, 20, 2);
  print_line("MediBox!", 20, 40, 2);
  display.clearDisplay();
}

void loop() {
  //connect with mqtt broker -  Assignment 2
  //--------------------------------------------------------------------------
  if(!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();
  //publish topics with data
  mqttClient.publish("Light_Control", lightAr); //Light Intensity - Highest among LDR1 and LDR2
  //mqttClient.publish("Light_Name", luxName);
  mqttClient.publish("Temp_Control", tempAr); // Teemperature 
  //--------------------------------------------------------------------------
  update_time_with_check_alarm();
  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }
  check_temp();
  read_ldr() ;
  serv_mo();
  check_schedule();
}
//------------------------------------------------------------------------------
void setupMqtt() {
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(recieveCallback);
}
//------------------------------------------------------------------------------
// helps to receive the message from the MQTT broker
void recieveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for (int i = 0; i < length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.print("\n");
 
  //receive minimum angle of thee Servo
  if (strcmp(topic, "minAngle") == 0){
    MIN_ANGLE = atoi(payloadCharAr); //ASCII to integer
  }
  //receive control factor
  if (strcmp(topic, "controllerFactor") == 0) {
    GAMMA = atof(payloadCharAr); //ASCII to float
  }
  //receive main switch status
  if (strcmp(topic, "MainSwitch_Status") == 0) {
    buzzerOn(payloadCharAr[0] == 't');
  }
  //receive scheduled time
  if(strcmp(topic, "Sch_Time_RX") == 0){
    if(payloadCharAr[0] =='N'){
      isScheduledON = false;
    }else{
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  }


}
// Function helps to connect wiht the broker
//------------------------------------------------------------------------
void connectToBroker() {
  while(!mqttClient.connected()){
    Serial.println("Attempting MQTT connection");
    if(mqttClient.connect("ESP32-12345645454")){
      Serial.println("connected");
      mqttClient.subscribe("minAngle");
      mqttClient.subscribe("controllerFactor");
      mqttClient.subscribe("MainSwitch_Status");
      mqttClient.subscribe("Sch_Time_RX");
    }else{
      Serial.println("failed");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}
//------------------------------------------------------------------------


void setupWifi() {
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WiFi", 0, 0, 2);
  }

  display.clearDisplay();
  print_line("Connected to WiFi", 0, 0, 2);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
}

void buzzerOn(bool on){
  if(on){
    tone(BUZZER, C);
  }
  else{
    noTone(BUZZER);
  }
}

unsigned long getTime(){
  timeClient.update();
  return timeClient.getEpochTime();
}
// Getting the highest LI among 2 LDRs
//-------------------------------------------------------------------------------
void read_ldr() {
  int analogValue1 = analogRead(LDR1); //INDOOR LDR
  int analogValue2 = analogRead(LDR2); //OUTDOOR LDR

  // Calculate light intensity for LDR1
  float voltage1 = analogValue1 / 1024.0 * 5.0;
  float resistance1 = 2000.0 * voltage1 / (1.0 - voltage1 / 5.0);
  float maxlux1 = pow(RL10 * 1e3 * pow(10, GAMMA) / 322.58, (1 / GAMMA));
  float lux1 = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance1, (1 / GAMMA)) / maxlux1;

  // Calculate light intensity for LDR2
  float voltage2 = analogValue2 / 1024.0 * 5.0;
  float resistance2 = 2000.0 * voltage2 / (1.0 - voltage2 / 5.0);
  float maxlux2 = pow(RL10 * 1e3 * pow(10, GAMMA) / 322.58, (1 / GAMMA));
  float lux2 = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance2, (1 / GAMMA)) / maxlux2;

  // Determine the highest intensity
  float lux = max(lux1, lux2);
  String(lux).toCharArray(lightAr,6);
  /*String luxName;
  if (lux == lux1) {
      luxName = "lux1";
  } else {
      luxName = "lux2";
  }*/
  Serial.println(lux); 
  // Output the highest intensity
  String highestLDR;
  //float lux;
  if (lux1 > lux2) {
    highestLDR = "INDOOR"; // LDR1 has higher intensity
    lux = lux1;
  } else {
    highestLDR = "OUTDOOR"; // LDR2 has higher intensity
    lux = lux2;
  }
  mqttClient.publish("Highest_LDR", highestLDR.c_str());
}

  // Output the highest intensity and which LDR has it
  /*Serial.print("Highest Intensity: ");
  Serial.println(lux);
  Serial.print("Higher Intensity LDR: ");
  Serial.println(higherIntensity);*/

  // Convert lux to a character array and send it to Node-RED dashboard
  /*char luxCharArray[6];
  dtostrf(lux, 6, 2, luxCharArray); // Convert float to char array
  String(luxCharArray).toCharArray(lightAr, 6);*/
//---------------------------------------------------------------------------------

//Position of th servo
//------------------------------------------------------------------------
void serv_mo(){
  //calculating position of servo motor
  pos = MIN_ANGLE + (180 - MIN_ANGLE) * lux * GAMMA;
  servo.write(pos);
}
//------------------------------------------------------------------------
// set the buzzer to tone when scheduled time comes
void check_schedule() {
  
  if (isScheduledON){
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime) {
      buzzerOn(true);
      isScheduledON = false;
      mqttClient.publish("ESP_Control", "1");
      mqttClient.publish("Sch_Control", "0");
      Serial.println("Scheduled ON");
      Serial.println(currentTime);
      Serial.println(scheduledOnTime);
    }
  }
}

//End of the codes related to Assignment 2
//===============================================================

void print_line(String text, int column, int row, int text_size) {

  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();

}

void print_time_now(void) {
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);

}

void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);

}

void ring_alarm() {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);
  digitalWrite(LED_1, HIGH);

  bool break_happened = false;
  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

void update_time_with_check_alarm() {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void go_to_menu() {
  // display.clearDisplay();
  // print_line("MENU",0,0,2);
  // delay(1000);
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = max_modes - 1;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      Serial.println(current_mode);
      run_mode(current_mode);
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

void set_time() {
  int temp_hour = 5;

  while (true) {
    display.clearDisplay();
    print_line("Hour offset:" + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      if (temp_hour > 14) {
        temp_hour = -12;
      }
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < -12) {
        temp_hour = 14;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      hours = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = 30;

  while (true) {
    display.clearDisplay();
    print_line("Minute offset:" + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 15;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 15;
      if (temp_minute < 0) {
        temp_minute = 45;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      minutes = temp_minute;
      if (temp_hour >= 0){
        UTC_OFFSET = temp_hour*60*60 + temp_minute*60;
      }
      else{
        UTC_OFFSET = -(-temp_hour*60*60 + temp_minute*60);
      }
      configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
      update_time();  
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Time is  set", 0, 0, 2);
  delay(1000);

}

void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter hour:" + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter minute:" + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Alarm is  set", 0, 0, 2);
  delay(1000);

}

void run_mode(int mode) {
  if (mode == 0) {
    set_time();
  }

  else if (mode == 1 || mode == 2 || mode == 3) {
    set_alarm(mode - 1);
  }

  else if (mode == 4) {
    alarm_enabled = false;
    print_line("All alarms are disabled", 0, 0, 2);
    delay(1000);
  }
}

void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (data.temperature > 35) {
    display.clearDisplay();
    print_line("Temp is high", 0, 40, 1);
  }
  else if (data.temperature < 25) {
    display.clearDisplay();
    print_line("Temp is low", 0, 40, 1);
  }

  if (data.humidity > 40) {
    display.clearDisplay();
    print_line("Humidity high", 0, 50, 1);
  }
  else if (data.humidity < 20) {
    display.clearDisplay();
    print_line("Humidity low", 0, 50, 1);
  }

    String(data.temperature,2).toCharArray(tempAr, 10);
}