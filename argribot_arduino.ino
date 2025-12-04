// agribot_final_exact_clean.ino
// Final sketch: waits for READY, follows line, ARRIVED n L:x R:y, wait DONE, CLEAR, finish after MAX_STOPS.

//// PIN SETUP
const int IR_LEFT_PIN  = 2;
const int IR_RIGHT_PIN = 3;

const int L_IN1 = 4;
const int L_IN2 = 5;
const int L_ENA  = 6;

const int R_IN3 = 7;
const int R_IN4 = 8;
const int R_ENB  = 9;

const int ESTOP_PIN = 10;   // optional E-stop active LOW
const int STATUS_LED = 13;

//// CONFIG (tweak to match your hardware)
const bool invertSensors = true; // true if sensor reads LOW on black
const int STRAIGHT_SPEED = 200;
const int SLOW_SPEED = 120;
const int CLEAR_SPEED = 140;

const unsigned long debounce_ms = 30;
const unsigned long CLEAR_TIMEOUT_MS = 3000;      // max ms to attempt clearing
const unsigned long CLEAR_WHITE_DEBOUNCE_MS = 80; // require both-white stable this long
const unsigned long ARRIVED_RESEND_MS = 700;      // resend ARRIVED periodically

const int MAX_STOPS = 5; // finish after completing this many stops (>= behavior)

//// STATES
enum State { IDLE, FOLLOW, WAIT_DONE, CLEARING, FINISHED, ERROR };
State state = IDLE;

int plantCount = 0;        // 1..MAX_STOPS
unsigned long lastSensorChange = 0;
bool lastLeft = false, lastRight = false;
unsigned long lastArrivedSent = 0;
unsigned long clearStart = 0;
unsigned long bothWhiteSince = 0;

// snapshot of current plant being processed
int currentProcessingPlant = 0;

void setup() {
  pinMode(IR_LEFT_PIN, INPUT);
  pinMode(IR_RIGHT_PIN, INPUT);

  pinMode(L_IN1, OUTPUT);
  pinMode(L_IN2, OUTPUT);
  pinMode(L_ENA, OUTPUT);
  pinMode(R_IN3, OUTPUT);
  pinMode(R_IN4, OUTPUT);
  pinMode(R_ENB, OUTPUT);

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);

  Serial.begin(115200);
  stopMotors();
  Serial.println("ARD:BOOT");
}

bool readIR(int pin) {
  int v = digitalRead(pin);
  return invertSensors ? !v : v; // true == BLACK
}

void setMotorLeft(int pwm) {
  if (pwm == 0) { digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, LOW); analogWrite(L_ENA, 0); }
  else if (pwm > 0) { digitalWrite(L_IN1, HIGH); digitalWrite(L_IN2, LOW); analogWrite(L_ENA, constrain(pwm,0,255)); }
  else { digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, HIGH); analogWrite(L_ENA, constrain(-pwm,0,255)); }
}

void setMotorRight(int pwm) {
  if (pwm == 0) { digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW); analogWrite(R_ENB, 0); }
  else if (pwm > 0) { digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW); analogWrite(R_ENB, constrain(pwm,0,255)); }
  else { digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, HIGH); analogWrite(R_ENB, constrain(-pwm,0,255)); }
}

void stopMotors() {
  setMotorLeft(0); setMotorRight(0);
}

// send ARRIVED with sensor snapshot (plantNo must be set before calling)
void sendArrivedMessage(int plantNo, bool left, bool right) {
  Serial.print("ARRIVED ");
  Serial.print(plantNo);
  Serial.print(" L:");
  Serial.print(left ? 1 : 0);
  Serial.print(" R:");
  Serial.println(right ? 1 : 0);
  lastArrivedSent = millis();
}

// handle serial commands from Pi
void processSerial() {
  while (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length() == 0) continue;

    if (s == "READY") {
      if (state == IDLE) {
        Serial.println("ARD:READY_ACK");
        state = FOLLOW;
        Serial.println("ARD:START");
      }
    } else if (s == "DONE") {
      if (state == WAIT_DONE) {
        state = CLEARING;
        clearStart = millis();
        bothWhiteSince = 0;
        Serial.println("ARD:GOT_DONE_CLEARING");
      }
    } else if (s == "RESET") {
      state = IDLE;
      plantCount = 0;
      currentProcessingPlant = 0;
      stopMotors();
      Serial.println("ARD:RESET");
    } else {
      // ignore unknown
    }
  }
}

// simple differential steering
void setupFollowActions(bool left, bool right) {
  if (left && right) { stopMotors(); }
  else if (left && !right) { setMotorLeft(SLOW_SPEED); setMotorRight(STRAIGHT_SPEED); }
  else if (!left && right) { setMotorLeft(STRAIGHT_SPEED); setMotorRight(SLOW_SPEED); }
  else { setMotorLeft(STRAIGHT_SPEED); setMotorRight(STRAIGHT_SPEED); }
}

void loop() {
  // immediate emergency stop
  if (digitalRead(ESTOP_PIN) == LOW) state = ERROR;

  processSerial();

  bool left = readIR(IR_LEFT_PIN);
  bool right = readIR(IR_RIGHT_PIN);
  if (left != lastLeft || right != lastRight) {
    lastSensorChange = millis();
    lastLeft = left; lastRight = right;
  }

  if (millis() - lastSensorChange < debounce_ms) {
    // waiting for stable
  } else {
    if (state == IDLE) {
      stopMotors();
      digitalWrite(STATUS_LED, (millis() / 800) & 1);
    } else if (state == FOLLOW) {
      if (left && right) {
        // reached a stop line => increment plant counter and send ARRIVED
        plantCount++;
        currentProcessingPlant = plantCount; // snapshot which plant Pi will process
        stopMotors();
        sendArrivedMessage(currentProcessingPlant, left, right); // send ARRIVED n L:x R:y
        state = WAIT_DONE;
      } else {
        setupFollowActions(left, right);
      }
    } else if (state == WAIT_DONE) {
      // blink quickly; resend ARRIVED periodically with the latest sensor values
      digitalWrite(STATUS_LED, (millis() / 200) & 1);
      if (millis() - lastArrivedSent > ARRIVED_RESEND_MS) {
        sendArrivedMessage(currentProcessingPlant, lastLeft, lastRight);
      }
      // processSerial() will handle DONE -> CLEARING
    } else if (state == CLEARING) {
      digitalWrite(STATUS_LED, (millis() / 150) & 1);
      setMotorLeft(CLEAR_SPEED);
      setMotorRight(CLEAR_SPEED);

      if (!lastLeft && !lastRight) {
        // both white: require short debounce to avoid bounce
        if (bothWhiteSince == 0) bothWhiteSince = millis();
        else if (millis() - bothWhiteSince >= CLEAR_WHITE_DEBOUNCE_MS) {
          // cleared successfully: after clearing, decide finish or resume
          if (currentProcessingPlant >= MAX_STOPS) {
            Serial.println("FINISHED");
            state = FINISHED;
            stopMotors();
          } else {
            state = FOLLOW;
          }
          currentProcessingPlant = 0;
        }
      } else {
        bothWhiteSince = 0;
        if (millis() - clearStart > CLEAR_TIMEOUT_MS) {
          // fallback: resume following to avoid permanent stuck
          Serial.println("ARD:CLEAR_TIMEOUT_RESUME");
          state = FOLLOW;
          currentProcessingPlant = 0;
        }
      }
    } else if (state == FINISHED) {
      stopMotors();
      digitalWrite(STATUS_LED, HIGH);
      // remain finished until RESET/power-cycle
    } else { // ERROR
      stopMotors();
      digitalWrite(STATUS_LED, HIGH);
    }
  }
}

