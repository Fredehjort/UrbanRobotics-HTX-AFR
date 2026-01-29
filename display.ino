#include <LiquidCrystal.h>

const int rs = 7, en = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int buttonPin = 6;

// Eye stages
byte openEye[8] = {
  B00000,
  B00000,
  B01110,
  B10001,
  B10001,
  B10001,
  B10001,
  B01110
};
byte halfClosedEye[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B10001,
  B10001,
  B01110
};
byte closedEye[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B10001,
  B10001,
  B10001,
  B01110
};

// Interaction faces:
byte leftSquint[8] = {
  B00000,
  B00000,
  B00000,
  B11000,
  B00110,
  B00001,
  B00110,
  B11000
};
byte rightSquint[8] = {
  B00000,
  B00000,
  B00000,
  B00011,
  B01100,
  B10000,
  B01100,
  B00011
};

// Mouth
byte mouth[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B10001,
  B10101,
  B01010
};
byte mouthSquint[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00100,
  B01010,
  B01010
};


enum chars {
  OPENEYE,
  CLOSEDEYE,
  HALFCLOSEDEYE,
  LEFTSQUINT,
  RIGHTSQUINT,
  MOUTH,
  MOUTHSQUINT
};

int sauceImpasta = 0;
bool state = true;

void setup() {
  lcd.begin(16, 2);
  lcd.createChar(OPENEYE, openEye);
  lcd.createChar(CLOSEDEYE, closedEye);
  lcd.createChar(HALFCLOSEDEYE, halfClosedEye);
  lcd.createChar(LEFTSQUINT, leftSquint);
  lcd.createChar(RIGHTSQUINT, rightSquint);
  lcd.createChar(MOUTH, mouth);
  lcd.createChar(MOUTHSQUINT, mouthSquint);

  lcd.setCursor(0, 0);

  pinMode(buttonPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(3), statechange, FALLING);

};


void loop() {
  if (state = true){
    if(sauceImpasta % 2 == 1){
      lcd.clear();
      lcd.write(HALFCLOSEDEYE);
      lcd.write(MOUTH);
      lcd.write(HALFCLOSEDEYE);
      delay(300);

      lcd.clear();
      lcd.write(OPENEYE);
      lcd.write(MOUTH);
      lcd.write(OPENEYE);
    }
    else {
      lcd.clear();
      lcd.write(HALFCLOSEDEYE);
      lcd.write(MOUTH);
      lcd.write(HALFCLOSEDEYE);
      delay(300);

      lcd.clear();
      lcd.write(CLOSEDEYE);
      lcd.write(MOUTH);
      lcd.write(CLOSEDEYE);
    }

    delay(2000);
    sauceImpasta += 1;
  }
  
  else{
    squint();
  }
};


void squint() {
  lcd.clear();
  lcd.write(LEFTSQUINT);
  lcd.write(MOUTHSQUINT);
  lcd.write(RIGHTSQUINT);
};

void statechange(){
  state = !state;
};
