#include "Adafruit_FONA.h"
//#include <Adafruit_GPS.h>

#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4

#include <SoftwareSerial.h>
#include "MPU6050.h" //IVME kütüphanesi

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

//#include <ArduinoJson.h>
int durum=0;//kaza olmadıysa 0 olduysa 1
String parametre;

enum _parseState {
  PS_DETECT_MSG_TYPE,

  PS_IGNORING_COMMAND_ECHO,

  PS_HTTPACTION_TYPE,
  PS_HTTPACTION_RESULT,
  PS_HTTPACTION_LENGTH,

  PS_HTTPREAD_LENGTH,
  PS_HTTPREAD_CONTENT
};

byte parseState = PS_DETECT_MSG_TYPE;
char buffer[50];
byte pos = 0;

int contentLength = 0;

void resetBuffer() {
  memset(buffer, 0, sizeof(buffer));
  pos = 0;
}

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

float latitude = 0.0, longitude = 0.0, speed_kph, heading, speed_mph, altitude;

MPU6050 accelgyro; // Mpu6050 sensör tanımlama
int16_t ax, ay, az; //ivme tanımlama
int16_t gx, gy, gz; //gyro tanımlama
unsigned int aax, aay, aaz;

void setup() {
  pinMode(12, OUTPUT);
  while (!Serial);

  Serial.begin(9600);
  //Serial.begin(115200);

  accelgyro.initialize();

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    //Serial.println(F("GSM AYGITI BULUNAMADI"));
    while (1);
  }

  fona.enableGPS(true);

  // APN Adı, Kullanıcı Adı, Sifre
  fona.setGPRSNetworkSettings(F("internet"));

  //SSL Yonlendirmesi
  fona.setHTTPSRedirect(true);


}
int cont = 0;

void loop() {

  while (1) {

    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz); // ivme ve gyro değerlerini okuma
    aax = map(ax, -16384, 16383, 0, 256);
    aay = map(ay, -16384, 16383, 0, 256);
    aaz = map(az, -16384, 16383, 0, 256);

    bool kontrol = KazaKontrol(aax, aay, aaz);

    if (kontrol == true && durum==0) //kaza olduysa sms gönderme fonksiyonu devreye girecek
    {
      digitalWrite(12, HIGH);
      Serial.println("true");
      durum=1;
      break;
    }
    else{
      digitalWrite(12, LOW);
    }

    delay(5000);
  }

  flushFONA();

  //if (!fona.enableGPRS(true))
  //Serial.println(F("Failed to turn on"));
  delay(4000);
  cont = 1;
  //Serial.println("GPRS ON");

  int data_size = 0;
  int x = 0;

  //Serial.println("*********KONUM ARANIYOR");

  while (1) {
    // gps sensörden gelen ham datayı okuyup gpsdata[120] ye alır.
    // gps sensörden gelen datayı okuyup ilgili değişkenlere parse eder.
    bool gpsFix = fona.getGPS(&latitude, &longitude, &speed_kph, &heading,  &altitude);
    
    if (latitude != 0.0 || longitude != 0.0)
    {
      break;
    }
    delay(1000);
  }
  
  Serial.println("KONUM GÖNDERİLİYOR");
  gsm_sendhttp();
}

bool KazaKontrol(unsigned int aax, unsigned int aay, unsigned aaz) //kaza olup olmadığının kontrolü (kaza olduysa true olmadıysa false
{
  if (aax > 220 || aax < 10 || aay > 250 || aay < 10 || aaz > 26  0 || aaz < 100)
  {
    return true;//kaza olduğu için true değer döndürülür
  }
  return false; // kaza olmadığı için false değer döndürülür
}

void flushFONA() { //if there is anything is the fonaSS serial Buffer, clear it out and print it in the Serial Monitor.
  char inChar;
  while (fonaSS.available()) {
    inChar = fonaSS.read();
    Serial.write(inChar);
    delay(20);
  }
}

void gsm_sendhttp() {
  String url = "http://erdemkivanc.com/reyhantez/index.php";

  fona.println("AT");
  runsl();
  delay(4000);
  fona.println("AT+SAPBR=3,1,Contype,GPRS");
  runsl();
  delay(100);
  fona.println("AT+SAPBR=3,1,APN,internet");
  runsl();
  delay(100);
  fona.println("AT+SAPBR =1,1");
  runsl();
  delay(100);
  fona.println("AT+SAPBR=2,1");
  runsl();
  delay(2000);
  fona.println("AT+HTTPINIT");
  runsl();
  delay(100);
  fona.println("AT+HTTPPARA=CID,1");
  runsl();
  delay(100);
  fona.println("AT+HTTPPARA=URL," + url);
  runsl();
  delay(100);
  fona.println("AT+HTTPPARA=CONTENT,application/json");
  runsl();
  delay(100);
  fona.println("AT+HTTPDATA=192,10000");
  runsl();
  delay(100);
  fona.println("params=" + parametre);
  runsl();
  delay(10000);
  fona.println("AT+HTTPACTION=1");
  runsl();
  delay(5000);
  fona.println("AT+HTTPREAD");
  runsl();
  delay(5000);
  fona.println("AT+HTTPTERM");
  runsl();


  //Serial.println("deneme= " + deneme);
  //fona.callPhone(charBuf);

}

void runsl() {
  while (fona.available()) {
    parseATText(fona.read());
  }
}

void parseATText(byte b) {

  buffer[pos++] = b;
  int value = 0;
  int sign = 1;
  if ( pos >= sizeof(buffer) )
    resetBuffer(); // just to be safe

  switch (parseState) {
    case PS_DETECT_MSG_TYPE:
      {
        if ( b == '\n' )
          resetBuffer();
        else {
          if ( pos == 3 && strcmp(buffer, "AT+") == 0 ) {
            parseState = PS_IGNORING_COMMAND_ECHO;
          }
          else if ( b == ':' ) {
            //Serial.print("Checking message type: ");
            //Serial.println(buffer);

            if ( strcmp(buffer, "+HTTPACTION:") == 0 ) {
              Serial.println("Received HTTPACTION");
              parseState = PS_HTTPACTION_TYPE;
            }
            else if ( strcmp(buffer, "+HTTPREAD:") == 0 ) {
              Serial.println("Received HTTPREAD");
              parseState = PS_HTTPREAD_LENGTH;
            }
            resetBuffer();
          }
        }
      }
      break;

    case PS_IGNORING_COMMAND_ECHO:
      {
        if ( b == '\n' ) {
          parseState = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPACTION_TYPE:
      {
        if ( b == ',' ) {
          parseState = PS_HTTPACTION_RESULT;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPACTION_RESULT:
      {
        if ( b == ',' ) {
          parseState = PS_HTTPACTION_LENGTH;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPACTION_LENGTH:
      {
        if ( b == '\n' ) {
          // now request content
          fona.print("AT+HTTPREAD=0,");
          fona.println(buffer);

          parseState = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPREAD_LENGTH:
      {
        if ( b == '\n' ) {
          contentLength = atoi(buffer);
          Serial.print("HTTPREAD content: ");

          parseState = PS_HTTPREAD_CONTENT;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPREAD_CONTENT:
      {
        // for this demo I'm just showing the content bytes in the serial monitor
        //Serial.write(b);
        //Serial.println(buffer);

        //String msg = "ACIL YARDIM!!!";
        //msg.toCharArray(message, 300); // msg stringini message[200] char ına döndürür.
        //Serial.println(message);
        //fona.sendSMS("+905433709282",message);

        char newNumber[50];
        int k = 0;
        while (buffer[k] != '\0') {
          k++;
          if (k > 12) {
            delay(1000);


            //DynamicJsonDocument obj(10);
            //parametre = "";
            //delay(2000);
            //obj["KullaniciID"] = "7SDRf80EWJYgobyw8gj6GpfZmrr1";
            //obj["Lat"] = "20";
            //obj["Long"] = "90";
            //obj["KazaDurum"] = "1";
            //obj.printTo(parametre);
            //serializeJson(obj, parametre);
            //Serial.println(parametre);


            // gps sensörden gelen datayı okuyup ilgili değişkenlere parse eder.


            char message[80];
            String msg = "www.google.com/maps/place/" + String(latitude , 6) + "," + String(longitude, 6);
            msg.toCharArray(message, 80); // msg stringini message[200] char ına döndürür.
            //Serial.println(message);
            fona.sendSMS(buffer, message);
            resetBuffer();

          }
        }





        contentLength--;

        if ( contentLength <= 0 ) {
          parseState = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
      break;
  }
}
