#include <SoftwareSerial.h>
#include <ServoTimer2.h>

//rfid pins
#define enPin 6
#define rx 10
#define tx 11
#define indPin A3

//servo speeds
#define BLEFT 1495
#define FLEFT 1600
#define BRIGHT 1505
#define FRIGHT 1000
#define STILL 1500

//servo pins
#define leftP 13
#define rightP 12

//booleans
bool transmitting = false;
bool shouldLineFollow = true;
bool firstTime = true;
bool done = false;
bool finale = false;
bool shouldSend = false;

//ints
int count = 0;
int index = 0;
int tags[5];
int score = 0;

//serials
SoftwareSerial Xbee = SoftwareSerial(11, 5);
SoftwareSerial LCD = SoftwareSerial(255, 4);
SoftwareSerial rfid = SoftwareSerial(rx, 15);

//servos
ServoTimer2 left;
ServoTimer2 right;

void setup() {

  //led pins
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);

  //lcd
  pinMode(4, OUTPUT);
  LCD.begin(9600);
  digitalWrite(4, HIGH);
  lcdPrint("Welcome, Beater #1");
}

void loop() {

    //last hash mark
    if (count == 5)
    {
      //stop bot and back up for seeker
      left.write(STILL);
      right.write(STILL);
      delay(50);
      left.write(1400);
      right.write(1600);
      delay(200);
      left.write(STILL);
      right.write(STILL);

      //detach servos for interference
      left.detach();
      right.detach();

      //stop line following code
      shouldLineFollow = false;

      //turn off rfid, on xbee for communication
      rfid.end();
      Xbee.begin(9600);
      
      count++;
    }

    //after sensing wait for ramp data
    //done is bool that becomes true once ramp data is completely received
    if (count > 5 && !done)
    {
      if (Xbee.available())
      {
        receiveData();
      }
      
      return;
    }

    //wait for final score after our team has sent and the challenge is not yet over
    if (done && !finale)
    {
      if (Xbee.available())
      {
        receiveFinalScore();
      }
    }

    //skip all remaining code for faster loop execution
    if(!shouldLineFollow)
    {
      return;
    }

    //on first loop attach servos and enable rfid. Avoids interference issues.
    if (firstTime)
    {
      left.attach(leftP);
      right.attach(rightP);
      
      firstTime = false;

      pinMode(enPin, OUTPUT);
      pinMode(rx, INPUT);
      pinMode(indPin, OUTPUT);
      digitalWrite(enPin, HIGH);
      rfid.begin(2400);
      digitalWrite(enPin, LOW);
    }

    //line following
    
    bool pIntersect = false;

    //get qti numbers
    byte qti1 = RCtime(7);
    byte qti2 = RCtime(8);
    byte qti3 = RCtime(9);

    //intersection
    if (qti1 == 1 && qti2 == 1 && qti3 == 1)
    {

      left.write(STILL);
      right.write(STILL);
      
      if (pIntersect == false)
      {
         count++;
         pIntersect = true;
      }

      delay(50);

      //detach servos
      left.detach();
      right.detach();

      //empty rfid buffer to avoid double reads
      while(rfid.available() > 0)
      {
        rfid.read();
      }

      delay(1000);

      //read rfid sensor
      readRFID();

      left.attach(leftP);
      right.attach(rightP);
      
      //if not on last hash, go forward
      if (count < 5)
      {
        left.write(FLEFT);
        right.write(FRIGHT);

        delay(150);
      }
    }
    
    //forward
    if (qti1 == 0 && qti2 == 1 && qti3 == 0)
    {
      left.write(FLEFT);
      right.write(FRIGHT);
      pIntersect = false;
    }

    //go right
    if (qti1 == 1 && qti2 == 1 && qti3 == 0)
    {
      left.write(1540);
      right.write(BRIGHT);
      pIntersect = false;
    }

    //go left
    if (qti1 == 0 && qti2 == 1 && qti3 == 1)
    {
      left.write(1450);
      right.write(1460);
      pIntersect = false;
    }

    //go left
    if (qti1 == 0 && qti2 == 0 && qti3 == 1)
    {
      left.write(1450);
      right.write(1460);
      pIntersect = false;
    }

    //all white
    if (qti1 == 0 && qti2 == 0 && qti3 == 0)
    {
      left.write(STILL);
      right.write(STILL); 
    }

    //go right
    if (qti1 == 1 && qti2 == 0 && qti3 == 0)
    {
      left.write(1540);
      right.write(BRIGHT);
    }
  
    delay(1);
}

byte RCtime(int sensPin)
{
     //set pin as output to charge
     pinMode(sensPin, OUTPUT);       
     //charge capacitor
     digitalWrite(sensPin, HIGH);    
     delay(1);  
     //discharge capacitor                    
     pinMode(sensPin, INPUT);
     delay(25);
     //read pin voltage, if above 1.4 v, then it was black and returns 1. 
     //If white, capacitor discharges more quickly and read is below 1.4 v. Function returns 0.
     return digitalRead(sensPin);
}

void readRFID()
{
  //rfid reader sends out RF energy. The tag receives this on an antenna and uses the energy to power its internal microchip.
  //It then sends back modulated RF wave. Reader can then identify tag ID by modulated wave.
  //The received ID is put in the serial buffer. For this application, the exact ID is not needed.
  //If there is anything in the buffer, there is a tag.
  if(rfid.available() > 0)
  {
    //set current position in tags area to 1. Used to calculate team score later.
    tags[index] = 1;
    digitalWrite(indPin, HIGH);
    //disable rfid
    digitalWrite(enPin, HIGH);
    delay(250);
    digitalWrite(indPin, LOW);
    //empty rfid buffer
    while(rfid.available() > 0)
    {
      rfid.read();
    }
    //re-enable rfid
    digitalWrite(enPin, LOW);
  }
  else
  {
    //set array value to 0. No tag read.
    tags[index] = 0;
  }

  //increment current index
  index++;
}

void receiveData()
{
  //read data from Xbee. If it's one of designated team communication letters, compute score.
  char data = Xbee.read();

  //Each letter corresponds to a detected ramp by other team.
  //If we also had a tag, add 10 to score.
  if (data == 'o' && tags[0] == 1)
  {
    score += 10;
    tags[0] = 42;
  }

  if (data == 'p' && tags[1] == 1)
  {
    score += 10;
    tags[1] = 42;
  }

  if (data == 'q' && tags[2] == 1)
  {
    score += 10;
    tags[2] = 42;
  }

  if (data == 'r' && tags[3] == 1)
  {
    score += 10;
    tags[3] = 42;
  }

  if (data == 's' && tags[4] == 1)
  {
    score += 10;
    tags[4] = 42;
  }

  //if f is received, we are free to send calculated score to central bot
  if (data == 'f')
  {
    shouldSend = true;
  }

  //if t is received, other team is done sending
  if (data == 't')
  {
    done = true;
    lcdPrint(String(score));
    char scoreChar;
    char out;

    //send letters back to partner bot and central bot to communicate team score
    switch (score)
    {
      case 0:
        out = ' ';
        scoreChar = '%';
        break;
      case 10:
        out = 'g';
        scoreChar = '^';
        break;
      case 20:
        out = 'h';
        scoreChar = '&';
        break;
      case 30:
        out = 'i';
        scoreChar = '*';
        break;
      case 40:
        out = 'j';
        scoreChar = '(';
        break;
      case 50:
        out = 'k';
        scoreChar = ')';
        break;
       default:
        out = ' ';
        scoreChar = '%';
    }
  
    delay(100);
    Xbee.print(scoreChar);

    //ensure f is received before sending
    while (!shouldSend)
    {
      if (Xbee.available())
      {
        char j = Xbee.read();
        if (j == 'f')
        {
          shouldSend = true;
        }
      }
    }

    delay(1000);
    Xbee.print(out);
    delay(1000);
    //send l to indicate we are finished
    Xbee.print('l');
  }
}

void lcdPrint(String str)
{
  //print given string to the LCD
  LCD.listen();
  delay(100);
  LCD.write(12);
  LCD.write(17);
  delay(5);
  LCD.print(str);
  delay(500);
  LCD.write(18);
  Xbee.listen();
}

void receiveFinalScore()
{
  //receive character and subtract from A.
  //digit will be offset from A
  char final = Xbee.read();
  int digit = final - 'A';
  //if it wasn't a capital letter, keep listening
  if (digit < 0 || digit > 25)
  {
    return;
  }

  //compute final score and write to LCD.
  int finalScore = digit * 10;

  LCD.write(12);                       
  LCD.write(17);               
  delay(5);                          
  LCD.print(score);  
  LCD.write(13);                 
  LCD.print(finalScore);
  delay(500);

  //end serial streams
  Xbee.end();
  LCD.end();

  //indicate challenge complete
  finale = true;
}

