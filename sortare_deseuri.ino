/*
  Sortare automată a deșeurilor: metal / hârtie / plastic
  Platformă: Arduino UNO

  Fluxul de funcționare:
    1. senzorul IR detectează un obiect pus pe bandă
    2. banda avansează, apoi se oprește în dreptul senzorilor
    3. senzorul inductiv verifică dacă obiectul este metalic
    4. dacă nu, senzorul de culoare TCS3200 verifică dacă este alb (hârtie)
    5. altfel, obiectul este considerat plastic
    6. LED-ul RGB și LCD-ul arată materialul, iar servomotorul sortează obiectul

  Autor: Chiriac Nicolae
  Data: iunie 2025

  Biblioteci necesare: Servo, Wire, LiquidCrystal_I2C
*/

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD I2C, adresa 0x27, 16 coloane x 2 rânduri
Servo servoMotor;

// ---------- Pini ----------
const int pinMetal = 8;    // senzor inductiv (HIGH = metal detectat)
const int pinIR = 7;       // senzor IR (LOW = obiect detectat)
const int irAnalog = A0;   // ieșirea analogică a senzorului IR (nefolosită)
const int pinMotorBanda = 13;  // motorul benzii transportoare
const int pinServo = 9;

// Senzor de culoare TCS3200
const int pinColor = 2;  // OUT
const int pinS2 = 3;
const int pinS3 = 4;
const int pinS0 = 5;
const int pinS1 = 6;

// LED RGB (catod comun)
const int pinRed = 10;
const int pinGreen = 11;
const int pinBlue = 12;

// ---------- Praguri pentru senzorul de culoare ----------
// pulseIn() întoarce durata semnalului: valoare mică = lumină reflectată mai multă
const int PRAG_NEGRU = 100;  // toate canalele peste prag -> negru
const int PRAG_ALB = 55;     // toate canalele sub prag -> alb

// ---------- Timpi (milisecunde) ----------
const unsigned long TIMP_AVANS_BANDA = 3000;     // banda duce obiectul spre senzori
const unsigned long TIMP_CITIRE_METAL = 2000;    // banda oprită, senzorul de metal citește
const unsigned long TIMP_CITIRE_CULOARE = 3000;  // fereastra de citire a culorii
const unsigned long TIMP_PANA_LA_SERVO_METAL = 7000;
const unsigned long TIMP_PANA_LA_SERVO_HARTIE = 5000;
const unsigned long TIMP_PLASTIC = 7000;
const unsigned long TIMP_PAUZA_SERVO = 500;
const unsigned long TIMP_BANDA_INACTIVA = 25000;  // fără obiecte -> banda se oprește

// ---------- Variabile de stare ----------
unsigned long timpDetectie = 0;
unsigned long timpSenzor = 0;
String materialDetectat = "";
String Rand1 = "", Rand2 = "";  // ultimul text afișat pe LCD
bool motorOprit = false;

// Declarații
String detecteazaCuloare();
void AfisareLCD(String Linie1, String Linie2);
void AfisareRGB(int Red, int Green, int Blue);

void setup() {
  pinMode(pinMetal, INPUT);
  pinMode(pinIR, INPUT);
  pinMode(pinMotorBanda, OUTPUT);

  pinMode(pinS0, OUTPUT);
  pinMode(pinS1, OUTPUT);
  pinMode(pinS2, OUTPUT);
  pinMode(pinS3, OUTPUT);
  pinMode(pinColor, INPUT);

  pinMode(pinRed, OUTPUT);
  pinMode(pinGreen, OUTPUT);
  pinMode(pinBlue, OUTPUT);

  // S0 = HIGH, S1 = LOW -> frecvența de ieșire a senzorului de culoare la 20%
  digitalWrite(pinS0, HIGH);
  digitalWrite(pinS1, LOW);

  servoMotor.attach(pinServo);
  servoMotor.write(0);

  lcd.init();
  lcd.backlight();

  Serial.begin(9600);
}

void loop() {
  int valueIR = digitalRead(pinIR);

  if (valueIR == HIGH) {
    // niciun obiect pe bandă
    Serial.println("IR: Niciun obiect");
    AfisareRGB(LOW, LOW, LOW);
    AfisareLCD("In asteptare", "");
  } else {
    // obiect detectat
    timpDetectie = millis();
    Serial.println("IR: Obiect detectat");
    motorOprit = false;

    digitalWrite(pinMotorBanda, HIGH);  // pornește banda
    AfisareLCD("Obiect", "Detectat");
    delay(TIMP_AVANS_BANDA);

    AfisareLCD("Detectare:", "");
    digitalWrite(pinMotorBanda, LOW);  // banda se oprește cu obiectul în dreptul senzorului de metal

    // 1) Verificare metal: banda oprită, senzorul inductiv citește repetat
    materialDetectat = "";
    timpSenzor = millis();
    while (millis() - timpSenzor < TIMP_CITIRE_METAL) {
      if (digitalRead(pinMetal) == HIGH) {
        materialDetectat = "Metal";
        break;
      }
      delay(10);
    }
    digitalWrite(pinMotorBanda, HIGH);  // banda repornește indiferent de rezultat

    if (materialDetectat == "Metal") {
      // ---------- METAL ----------
      Serial.println("Metal: Detectat");
      AfisareLCD("Material:", materialDetectat);
      AfisareRGB(HIGH, LOW, LOW);  // LED roșu

      delay(TIMP_PANA_LA_SERVO_METAL);  // timpul în care obiectul ajunge la servomotor
      digitalWrite(pinMotorBanda, LOW);  // banda se oprește ca servomotorul să împingă obiectul

      for (int angle = 10; angle <= 155; angle += 30) {
        servoMotor.write(angle);
        delay(TIMP_PAUZA_SERVO);
      }

      servoMotor.write(0);                // revenire în poziția de repaus
      digitalWrite(pinMotorBanda, HIGH);  // banda repornește
      AfisareRGB(LOW, LOW, LOW);
      AfisareLCD("Sortare", "finalizata");

    } else {
      // 2) Nu este metal: se citește culoarea
      Serial.println("Metal: NU e detectat");
      timpSenzor = millis();
      while (millis() - timpSenzor < TIMP_CITIRE_CULOARE) {
        // aprox. 1 s: obiectul ajunge de la senzorul de metal la cel de culoare
        // aprox. 2 s: citirea culorii
        String culoare = detecteazaCuloare();
        Serial.println("Culoare detectata: " + culoare);
        if (culoare == "Alb") {
          materialDetectat = "Hartie";
          break;
        }
        delay(10);
      }

      if (materialDetectat == "Hartie") {
        // ---------- HÂRTIE ----------
        Serial.println("Hartie: Detectat");
        AfisareLCD("Material:", materialDetectat);
        AfisareRGB(LOW, LOW, HIGH);  // LED albastru

        servoMotor.write(180);
        delay(TIMP_PANA_LA_SERVO_HARTIE);  // timpul în care obiectul ajunge la servomotor
        digitalWrite(pinMotorBanda, LOW);  // banda se oprește ca servomotorul să împingă obiectul

        for (int angle = 150; angle >= 20; angle -= 20) {
          servoMotor.write(angle);
          delay(TIMP_PAUZA_SERVO);
        }

        digitalWrite(pinMotorBanda, HIGH);  // banda repornește
        servoMotor.write(0);
        AfisareRGB(LOW, LOW, LOW);
        AfisareLCD("Sortare", "finalizata");

      } else {
        // ---------- PLASTIC ----------
        // orice obiect nemetalic care nu este alb; servomotorul nu intervine
        Serial.println("Plastic: Detectat");
        materialDetectat = "Plastic";
        AfisareLCD("Material:", materialDetectat);
        AfisareRGB(LOW, HIGH, LOW);  // LED verde

        delay(TIMP_PLASTIC);
        AfisareRGB(LOW, LOW, LOW);
        AfisareLCD("Sortare", "finalizata");
      }
    }
  }

  // Dacă nu a mai fost detectat niciun obiect, banda se oprește
  if (((millis() - timpDetectie) > TIMP_BANDA_INACTIVA) && !motorOprit) {
    motorOprit = true;
    digitalWrite(pinMotorBanda, LOW);
    AfisareLCD("Banda STOP", "");
    delay(2000);
  }

  delay(10);
}

// Citește cele trei canale ale senzorului TCS3200 și întoarce culoarea dominantă
String detecteazaCuloare() {
  digitalWrite(pinS2, LOW);
  digitalWrite(pinS3, LOW);
  int red = pulseIn(pinColor, LOW);
  delay(10);

  digitalWrite(pinS2, HIGH);
  digitalWrite(pinS3, HIGH);
  int green = pulseIn(pinColor, LOW);
  delay(10);

  digitalWrite(pinS2, LOW);
  digitalWrite(pinS3, HIGH);
  int blue = pulseIn(pinColor, LOW);
  delay(10);

  // Pentru calibrare, decomentează:
  // Serial.print("R: "); Serial.print(red);
  // Serial.print(" G: "); Serial.print(green);
  // Serial.print(" B: "); Serial.println(blue);

  if (red > PRAG_NEGRU && green > PRAG_NEGRU && blue > PRAG_NEGRU) return "Negru";
  if (red < PRAG_ALB && green < PRAG_ALB && blue < PRAG_ALB) return "Alb";
  if (red < green && red < blue) return "Rosu";
  if (green < red && green < blue) return "Verde";
  if (blue < red && blue < green) return "Albastru";
  return "Necunoscut";
}

// Rescrie LCD-ul doar dacă textul s-a schimbat (evită pâlpâirea ecranului)
void AfisareLCD(String Linie1, String Linie2) {
  if ((Linie1 + Linie2) != (Rand1 + Rand2)) {
    Serial.println("Schimbare LCD");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(Linie1);
    lcd.setCursor(0, 1);
    lcd.print(Linie2);
    delay(50);
    Rand1 = Linie1;
    Rand2 = Linie2;
  }
}

// Comandă LED RGB (HIGH = aprins pentru fiecare culoare)
void AfisareRGB(int Red, int Green, int Blue) {
  digitalWrite(pinRed, Red);
  digitalWrite(pinGreen, Green);
  digitalWrite(pinBlue, Blue);
}
