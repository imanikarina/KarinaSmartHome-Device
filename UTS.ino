// Tugas Individu 1
// Karina Imani / 13519166

#include <WiFi.h>
#include <PubSubClient.h>

// Credentials

const char* ssid = "Eldo_22";
const char* password = "eldo2345";
const char* mqtt_server = "broker.mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient client(espClient);

#define MSG_BUFFER_SIZE	(50)
char msg1[MSG_BUFFER_SIZE];
char msg2[MSG_BUFFER_SIZE];

// Variables
int startime[3];
int currtime[3];
int startmillis = 0;

// Device status: 0 means off, 1 means on
int lamp_stat = 0;
int ac_stat = 0;
int lamp_prev = 0;
int ac_prev = 0;

// Device timer status: 0 means off, 1 means on
int lamp_timer[2];
int ac_timer[2];

// Additional data to make sure timer is set once
int timer_unset[2][2];

// Device timer
int lamp_timer_value[2][2];
int ac_timer_value[2][2];

// Function to connect to WiFi
void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function that runs when message is received
void callback(char* topic, byte* payload, unsigned int length) {

  // Message announcement and details
  Serial.print("RECV . . . [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    msg1[i] = (char)payload[i];
  }

  Serial.print(msg1);
  Serial.println();

  // First character: 0 for on/off, 1 for off timer, 2 for on timer
  // Second character: A for Lamp, B for AC
  // Check 3, 4, 5, 6th characters for time if second character is 1 or 2
  
  if (msg1[0] == '0') {
    // Turn Lamp or AC on/off
    changeState(msg1[1], toInt(msg1[2]));
    publishAndInform(msg1);
  }
  if (msg1[0] == '1' || msg1[0] == '2') {
    // Change Lamp or AC on/off timer
    if (toInt(msg1[2]) != 9) {
      changeTimer(toInt(msg1[0]) - 1, msg1[1], toInt(msg1[2]) * 10 + toInt(msg1[3]), toInt(msg1[4]) * 10 + toInt(msg1[5]));
    } else deleteTimer(toInt(msg1[0]) - 1, msg1[1]);
    publishAndInform(msg1);
  }
  if (msg1[0] == 'X') {
    // Initialize current time
    startime[0] = toInt(msg1[1]) * 10 + toInt(msg1[2]);
    startime[1] = toInt(msg1[3]) * 10 + toInt(msg1[4]);
    startime[2] = toInt(msg1[5]) * 10 + toInt(msg1[6]);
    setCurrTime(startime, 0);
    startmillis = millis();

    // Initialize timers
    // Timers are in the following order: lamp off, lamp on, ac off, ac on
    
    int i = 8;
    if (toInt(msg1[i]) != 9) {
      changeTimer(0, 'A',toInt(msg1[i]) * 10 + toInt(msg1[i+1]), toInt(msg1[i+2]) * 10 + toInt(msg1[i+3]));
    } else deleteTimer(0, 'A');

    int j = 13;
    if (toInt(msg1[j]) != 9) {      
      changeTimer(1, 'A', toInt(msg1[j]) * 10 + toInt(msg1[j+1]), toInt(msg1[j+2]) * 10 + toInt(msg1[j+3]));
    } else deleteTimer(1, 'A');

    int k = 18;
    if (toInt(msg1[k]) != 9) {
      changeTimer(0, 'B', toInt(msg1[k]) * 10 + toInt(msg1[k+1]), toInt(msg1[k+2]) * 10 + toInt(msg1[k+3]));
    } else deleteTimer(0, 'B');

    int l = 23;
    if (toInt(msg1[l]) != 9) {
      changeTimer(1, 'B', toInt(msg1[l]) * 10 + toInt(msg1[l+1]), toInt(msg1[l+2]) * 10 + toInt(msg1[l+3]));
    } else deleteTimer(1, 'B');

    // Send lamp and timer status
    snprintf(msg2, MSG_BUFFER_SIZE, "X%ld%ld", lamp_stat, ac_stat);
    publishAndInform(msg2);
    clearMsg2(3);
  }
  clearMsg1(length);
}

// Function to reconnect to WiFi
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "UTSKarina";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected.");
      // Once connected, publish an announcement and renew the subscription
      client.publish("KarinaSmartHome/out", "Connected to device.");
      client.subscribe("KarinaSmartHome/in");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  lamp_timer[0] = 0;
  lamp_timer[1] = 0;
  ac_timer[0] = 0;
  ac_timer[1] = 0;
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Change lamp stat if timer runs
  // If on and off timers show the same time, turn off
  // If timer is valid and already set, after 60 seconds the timer will be reset for tomorrow

  setCurrTime(startime, floor((millis()-startmillis)/1000));

  if ((millis()/1000)%5 == 0) {
    Serial.print("CURRENT TIME : ");
    printTime(currtime, 3);
    Serial.println();
  }
  
  // Lamp ON, OFF timer
  for (int p = 1; p >= 0; p--) {
    if (lamp_timer[p]) {
      if (isTimerTrue(lamp_timer_value[p][0], lamp_timer_value[p][1], currtime)) {
        if (lamp_stat != p && timer_unset[0][p]) {
          changeState('A', p);
          snprintf(msg2, MSG_BUFFER_SIZE, "X%ld%ld", lamp_stat, ac_stat);
          publishAndInform(msg2);
          clearMsg2(3);
        }
        timer_unset[0][p] = 0;
      } else {
        if (!timer_unset[0][p]) timer_unset[0][p] = 1;
      }
    }
  }
  // AC ON, OFF timer
  for (int q = 1; q >= 0; q--) {
    if (ac_timer[q]) {
      if (isTimerTrue(ac_timer_value[q][0], ac_timer_value[q][1], currtime)) {
        if (ac_stat != q  && timer_unset[1][q]) {
          changeState('B', q);
          snprintf(msg2, MSG_BUFFER_SIZE, "X%ld%ld", lamp_stat, ac_stat);
          publishAndInform(msg2);
          clearMsg2(3);
        }
        timer_unset[1][q] = 0;
      } else {
        if (!timer_unset[1][q]) timer_unset[1][q] = 1;
      }
    }
  }

  // Send message if lamp stat changes
  if (lamp_prev != lamp_stat) {
    lamp_prev = lamp_stat;
    if (lamp_stat == 0) publishAndInform("Lamp is turned OFF.");
    if (lamp_stat == 1) publishAndInform("Lamp is turned ON.");
  }
  if (ac_prev != ac_stat) {
    ac_prev = ac_stat;
    if (ac_stat == 0) publishAndInform("AC is turned OFF.");
    if (ac_stat == 1) publishAndInform("AC is turned ON.");
  }

  // Run the LED
  if (lamp_stat && ac_stat) {
    // Both are on
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
  } else if (!lamp_stat && ac_stat) {
    // Lamp is off and AC is on    
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  } else if (lamp_stat && !ac_stat) {
    // Lamp is on and AC is off
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
  } else if (!lamp_stat && !ac_stat) {
    // Both are off
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
  }
  
}

// ---------------
// EXTRA FUNCTIONS
// ---------------

// Changes state of device (type A/B) into off/on (0/1)
void changeState(char type, int stat) {
  if (type == 'A' && (stat == 0 || stat == 1)) {
    lamp_stat = stat;
    Serial.print("Changed lamp status from ");
    Serial.print(statString(lamp_prev));
    Serial.print(" to ");
    Serial.print(statString(lamp_stat));
    Serial.println(".");
  } else if (type == 'B' && (stat == 0 || stat == 1)) {
    ac_stat = stat;
    Serial.print("Changed AC status from ");
    Serial.print(statString(ac_prev));
    Serial.print(" to ");
    Serial.print(statString(ac_stat));
    Serial.println(".");
  }
}

// Changes timer of device (type A/B) off/on (0/1) into hh:mm
void changeTimer(int timer, char type, int hh, int mm) {
  if (type == 'A' && (timer == 0 || timer == 1)) {
    lamp_timer[timer] = 1;
    lamp_timer_value[timer][0] = hh;
    lamp_timer_value[timer][1] = mm;
    if (timer == 0) Serial.print("Changed lamp OFF timer to ");
    if (timer == 1) Serial.print("Changed lamp ON timer to ");
    timer_unset[0][timer] = 1;
  } else if (type == 'B' && (timer == 0 || timer == 1)) {
    ac_timer[timer] = 1;
    ac_timer_value[timer][0] = hh;
    ac_timer_value[timer][1] = mm;
    if (timer == 0) Serial.print("Changed AC OFF timer to ");
    if (timer == 1) Serial.print("Changed AC ON timer to ");
    timer_unset[1][timer] = 1;
  }
  Serial.print(hh);
  Serial.print(":");
  Serial.print(mm);
  Serial.println(".");
  publishAndInform("Successfully changed timer.");
}

// Removes timer of device (type A/B) off/on (0/1)
void deleteTimer(int timer, char type) {
  if (type == 'A') {
    lamp_timer[timer] = 0;
    timer_unset[0][timer] = 0;
  } else if (type == 'B') {
    ac_timer[timer] = 0;
    timer_unset[1][timer] = 0;
  }
  publishAndInform("Successfully deleted timer.");
}

// Publishes msg and prints what is published
void publishAndInform(char* msg) {
  Serial.print("SEND . . . [KarinaSmartHome/out] ");
  Serial.print(msg);
  Serial.println();
  client.publish("KarinaSmartHome/out", msg);
}

// Changes current time to time with an offset of secs
void setCurrTime(int time[3], int secs) {
  int ss = (int)floor((secs%3600)%60);
  int mm = (int)floor((secs%3600)/60);
  int hh = (int)floor(secs/3600);
  // The devil's work
  if (time[2] + ss > 59) {
    mm += 1;
    ss -= 60;
  }
  if (time[1] + mm > 59) {
    hh += 1;
    mm -= 60;
  }
  if (time[0] + hh > 23) {
    hh -= 23;
  }
  // Now set them down
  currtime[0] = time[0] + hh;
  currtime[1] = time[1] + mm;
  currtime[2] = time[2] + ss;
  
}

// Compares hh:mm with time with a grace period of 60s
bool isTimerTrue(int hh, int mm, int time[3]) {
  return (
    (time[0] * 3600 + time[1] * 60) % 86400 - (hh * 3600 + mm * 60) % 86400 < 60 &&
    (time[0] * 3600 + time[1] * 60) % 86400 - (hh * 3600 + mm * 60) % 86400 >= 0
  );
}

// Converts char to int
int toInt(char i) {
  return (int)(i-'0');  
}

void clearMsg1(int length) {
  for (int i = 0; i < length; i++) {
    msg1[i] = ' ';
  }
}

void clearMsg2(int length) {
  for (int i = 0; i < length; i++) {
    msg2[i] = ' ';
  }
}

// --------------
// PRINTING NEEDS
// --------------

// Converts off/on (0/1) int into words
char* statString(int stat) {
  if (stat == 1) {
    return "ON";
  } else if (stat == 0) {
    return "OFF";
  }
}

// Prints three or two digit time
void printTime(int* time, int length) {
  for (int i = 0; i < length; i++) {
    Serial.print(time[i]);
    if (i != length - 1) Serial.print(":");
  }
}