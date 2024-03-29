#include <ros.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Int32MultiArray.h>

#include <Controllino.h>

ros::NodeHandle  nh;

//Left- & Right-Position
#define LEFTPOS 10000   // max. enc steps track
#define RIGHTPOS 2500   // min. enc steps track

// Encoder
#include <Encoder.h>
#define ENC 2000 // pendel senkrecht nach oben 
#define TOLROT 250 // ENC -+ 100
const byte interruptPin0 = CONTROLLINO_IN0;
const byte interruptPin1 = CONTROLLINO_IN1;
const byte digitalIn0 = CONTROLLINO_DI0;
const byte digitalIn1 = CONTROLLINO_DI1;
Encoder encoder_lin(interruptPin0, digitalIn0);
Encoder encoder_rot(interruptPin1, digitalIn1);
std_msgs::Int32 enc_rot_msg;
ros::Publisher enc_rot_pub("enc", &enc_rot_msg);



// Setpoint_rot
volatile double Setpoint_rot = ENC / 2;
double Input_rot, Output_rot = 0;

// Setpoint_lin
double Setpoint_lin = (LEFTPOS - RIGHTPOS) / 2 + RIGHTPOS;
double Input_lin, Output_lin = 0;

//volatile int speed = 0;
void Setpoint_rotCb(const std_msgs::Int32& Setpoint_lin_msg) {
  Setpoint_lin = Setpoint_lin_msg.data;

}
ros::Subscriber<std_msgs::Int32> Setpoint_rot_sub("Setpoint_rot", Setpoint_rotCb); // topic in python anpassen

// PIDs
#include <PID_v1.h>
double Kp_rot = 0.3, Ki_rot = 0, Kd_rot = 0.1;
PID PID_rot(&Input_rot, &Output_rot, &Setpoint_rot, Kp_rot, Ki_rot, Kd_rot, DIRECT);

void pidsCb(const std_msgs::Int32MultiArray &pids_msg) {
  Kp_rot = pids_msg.data[0] / 100.0;
  Ki_rot = pids_msg.data[1] / 100.0;
  Kd_rot = pids_msg.data[2] / 100.0;
  PID_rot.SetTunings(Kp_rot, Ki_rot, Kd_rot);
}
ros::Subscriber<std_msgs::Int32MultiArray> pids_sub("pids", pidsCb);

// Motor
#define PWM_MIN 0
#define PWM_MAX 50
const int pwm_pin = CONTROLLINO_AO0;
const int motor_in1_pin = CONTROLLINO_DO1;
const int motor_in2_pin = CONTROLLINO_DO2;



// REFERENCE
const int ind_pin = CONTROLLINO_DI2;
bool referenced = false;

void referenceCb( const std_msgs::Empty& reference_msg) {
  reference();
}
ros::Subscriber<std_msgs::Empty> ref_sub("reference", &referenceCb );

void setup()
{
  Serial.begin(9600);

  // encoder
  pinMode(interruptPin0, INPUT);
  pinMode(interruptPin1, INPUT);
  pinMode(digitalIn0, INPUT);
  pinMode(digitalIn1, INPUT);

  // induktion
  pinMode(ind_pin, INPUT);

  // motor
  pinMode(pwm_pin, OUTPUT);
  pinMode(motor_in1_pin, OUTPUT);
  pinMode(motor_in2_pin, OUTPUT);
  // motor aktivieren ?!
  digitalWrite(motor_in1_pin, HIGH);
  digitalWrite(motor_in2_pin, LOW);

  // pid
  PID_rot.SetMode(AUTOMATIC);
  PID_rot.SetOutputLimits(-PWM_MAX, PWM_MAX);

  // ros node und topics
  //nh.initNode();
  //nh.advertise(enc_rot_pub);
  //nh.subscribe(Setpoint_rot_sub);
  //nh.subscribe(pids_sub);
  //nh.subscribe(ref_sub);

  delay(1000);
  //nh.spinOnce();
   // pendel hängt nach untern --> 0 ENC
  encoder_rot.write(0);
  //reference();
}

void loop()
{
  //erst ausführen, wenn referenziert wurde
  //if (referenced == true) {
    long encoder_read = encoder_rot.read();
    long encoder_abs = abs(encoder_read);
    int encoder_int = int(encoder_abs);
    int encoder_mod = encoder_int%ENC;
    Input_rot = encoder_mod;
    //Input_rot = abs(encoder_rot.read())%ENC;
    //Input_rot = abs(encoder_rot.read());
    printEncoder();

    if (controlable() == true)
    {
      PID_rot.Compute();
      int drv = int(Output_rot);
      drive_motor(drv);
    } else {
      drive_motor(0);
    }

    if (digitalRead(ind_pin) == false) {
      encoder_rot.write(0);
    }

    // winkel an ros senden
    //enc_rot_msg.data = int(Output_rot);
    //enc_rot_pub.publish( &enc_rot_msg );
  //}

  //nh.spinOnce();
  //delay(1000);
}

bool allowMovement(int pwm)
{
  return true;
  if (digitalRead(ind_pin) == false && pwm > 0) {
    return false;
  }

  //Positionsüberprüfung erst ausführen, wenn referenziert wurde
  if (referenced == true) {
    int currentPosition = encoder_lin.read();
    //wenn linker Anschlag und Fahrtrichtung nach links
    if (currentPosition > LEFTPOS && pwm < 0) {
      return false;
    }
    //wenn rechter Anschlag und Fahrtrichtung nach rechts
    if (currentPosition < RIGHTPOS && pwm > 0) {
      return false;
    }
  }
  return true;
}

// motor wird über geschwindikeit angesteuert (pwm signal)
void drive_motor(int pwm)
{
  // sofware grenzen abfragen
  if (allowMovement(pwm) == true) {

    if (pwm >= 0) {
      digitalWrite(motor_in1_pin, HIGH);
      digitalWrite(motor_in2_pin, LOW);
    }

    else {
      digitalWrite(motor_in1_pin, LOW);
      digitalWrite(motor_in2_pin, HIGH);
    }

    analogWrite(pwm_pin, int(abs(pwm)));
  }

  // motor ausschalten
  else {
    digitalWrite(motor_in1_pin, LOW);
    digitalWrite(motor_in2_pin, LOW);
    analogWrite(pwm_pin, int(0));
  }
}

void reference()
{
  referenced = false;
 

  // nach links fahren bis induction aktiviert wird
  while (digitalRead(ind_pin) == true)
  {
    drive_motor(8);
  }

  drive_motor(0);
  delay(1000);

  // nullpunkt der linearführung
  encoder_lin.write(0);
  referenced = true;

  delay(2500);

  // zur mitte fahren
  while (encoder_lin.read() < (LEFTPOS-RIGHTPOS) / 2 + RIGHTPOS)  
  {
    drive_motor(-8);
    printEncoder();
  }

  drive_motor(0);
  delay(2500);
}

void printEncoder() 
{
    if (!Serial) {
      return;
    }
    //Serial.print(encoder_lin.read());
    //Serial.print("\t");
    //Serial.print(encoder_rot.read());
    //Serial.print("\t");
    Serial.print(Input_rot);
    Serial.print("\t");
    //if (Output_rot != 0) {
      Serial.println(Output_rot);
      //Serial.print("\t");
    //}
    //Serial.println(controlable());
}

bool controlable()
{
  // % um vielfaches von ENC zu berechnen
  // true wenn pendel im regelbaren kegel liegt (ENC+-TOLROT)
  if ((Input_rot) >= (ENC/2 - TOLROT) && (Input_rot) <= (ENC/2 + TOLROT))
  {
    return true;
  }

  return false;
}
