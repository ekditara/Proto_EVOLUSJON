/*#####################################
TPD4126: arduino-kode til spillet EVOLUSJON 12.5.2024
#######################################*/

/*---------------------------------------KREDITERING --------------------------------------------------
Emma Ditaranto har skrevet koden og implementert disse bibliotekene:

bibliotek til farge- og nærhets-sensor APDS9960: https://github.com/adafruit/Adafruit_APDS9960 
bibliotek til multiplexer APDS: https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/arduino-wiring-and-test 
bibliotek til NEOPIXEL STRIP: https://github.com/adafruit/Adafruit_NeoPixel
bibliotek til mp3-spilleren VS1053: https://github.com/adafruit/Adafruit_VS1053_Library/blob/master/Adafruit_VS1053.h 
bibliotek til elektromagnetene GROVE: https://wiki.seeedstudio.com/Grove-Electromagnet/ 

og selvfølgelig en stor takk til studassene Trond, Syver og Olaf for all hjelp<3
--------------------------------------------------------------------------------------------------*/


/* -------------- spillet EVOLUSJON sitt forløp ------------------------- */
/*
1. Startknappen trykkes, arduino resettes  
  1.1 tidLED lyser opp på det første stadiet
  1.2 Elektromagnetene skrus på slik at de tiltrekker klossene som kommer i nærheten
  1.3 Farge/Avstand-sensorene skrus på
  1.4 mp3, amplifier aktiveres

2. Når hver kloss settes inn så festes de til platen ved magnetisk kraft

FOR () 
{ 
  3. IF alle klossene er satt inn 
      3.2 Fargesensorene registrerer fargene til klossene
      3.3 IF klossene er i riktig rekkefølge/farge.
          3.3.1 lyser det grønt
          3.3.2 Neste tidLED lyser hvitt på tidslinja
          3.3.3 Bekreftende lyd høres
      3.5 ELSE feil, så blinker tidLED rødt
          3.5.1 tidLED lyser hvitt
          3.5.2 avkreftrende lyd
            
  4. Elektromagnetene deaktiveres og klossene detter ut
} 

5. Punkt 2-4 gjentas til alle tidLED lyser grønt 
    5.1 tidLED LØPELYS grønt og positiv lyd gis ut. Du har vunnet!
*/


/*-------------------------------------libraries --------------------------------------------*/
#include <Adafruit_APDS9960.h>  
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

/*---------------------- constants for color sensor (APDS9960) -------------------------*/
#define TCAADDR 0x70                  // fargesensor I2C adress
Adafruit_APDS9960 apds1;              // fargesensor 1, tilhører trekloss med hode
Adafruit_APDS9960 apds2;              // fargesensor  2, tilhører trekloss med overkropp
Adafruit_APDS9960 apds3;              // fargesensor 3, tilhører trekloss med underkropp

uint16_t colorBlock1[4];              // globalt array som lagrer de målte rgbc-verdiene til kloss nr.1
uint16_t colorBlock2[4];              // globalt array som lagrer de målte rgbc-verdiene til kloss nr.2
uint16_t colorBlock3[4];              // globalt array som lagrer de målte rgbc-verdiene til kloss nr.3

const int timeperiod = 4;             // antall tids-epoker i spillet
const int nTotalBlocks = 3;           // totalt antall klosser i spillet
const int nColorValues = 4;           // antall fargeverdier som blir målt (r,g,b,c)
const int tolerance = 4;

const uint16_t colorAnimals[timeperiod][nColorValues] = 
  {{10,41,84,110},{43,6,22,57},{54,27,41,98},{22,41,62,99}}; // 2darray[rows][cols] = {{fish}, {reptile}, {monkey},{human}}
                                                              // fish = lyseblå , reptil = rød, ape = gul , mennekse = lysegrønn

/*---------------------- constants for proximity sensor (APDS9960) -------------------------*/
const int pinProx1 = 3;
const int pinProx2 = 4;
const int pinProx3 = 5;
const int proxTreshold = 200; // tar verdier fra 0 til 255, som tilsvarer noen få cm. 255 er nærmest sensoren, 0 er lengst ifra. må kalibreres
const int pinProxArray[3] = {pinProx1, pinProx2, pinProx3};

/*---------------------- constants for electromagnetic component -------------------------*/
const int pinMagnet1 = 6;
const int pinMagnet2 = 7;
const int pinMagnet3 = 8;
const int pinMagnetArray[3] = {pinMagnet1, pinMagnet2, pinMagnet3};

/*------------------------- constants for Neopixel Strip in timeline -------------------------------------*/
const int pinTimelineLED = 2;
const int nTimelineLED = 21;
const int LEDarray[4] = {19,15,8,2}; 
Adafruit_NeoPixel timelineLED(nTimelineLED, pinTimelineLED, NEO_GRB + NEO_KHZ800);

/*------------------------- constants for Neopixel Strip to light up for APDS color sensor -------------------------------------*/
const int pinColorSensorLED = 12;
const int nColorSensorLED = 16;
Adafruit_NeoPixel colorSensorLED(nColorSensorLED, pinColorSensorLED, NEO_GRB + NEO_KHZ800);

/*------------------------------------- constants for MUSIC MAKER SHIELD VS1053 ----------------------------------------------------------*/
#define SHIELD_RESET  -1                                                    // VS1053 reset pin (unused!)
#define SHIELD_CS     7                                                     // VS1053 chip select pin (output)
#define SHIELD_DCS    6                                                     // VS1053 Data/command select pin (output)
#define CARDCS 4                                                            // Card chip select pin
#define DREQ 3                                                              // VS1053 Data request, ideally an Interrupt pin 

Adafruit_VS1053_FilePlayer musicPlayer =  
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS); // definerer musikkspiller objekt



/*--------------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------- MAIN CODE ------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

void setup() {

  Serial.begin(115200);                                       
  Wire.begin();                                                       // Initialiserer kommunikasjon mellom Arduino og multiplexeren.

  /* ----------------------- setup NeoPixel Strips ---------------------------------------*/
  pinMode(pinTimelineLED, OUTPUT);                                    // lysstripe til tidslinja
  timelineLED.begin();
  timelineLED.setBrightness(50);
  timelineLED.show();
  timelineLED.show();

  pinMode(pinColorSensorLED, OUTPUT);                                  // lysstripe til fargesensorene
  colorSensorLED.begin();                               
  colorSensorLED.clear();
  colorSensorLED.setBrightness(80);                                   // bestemmer høy lysstyrke for at fargesensorene skal ha nok leselys
  colorSensorLED.show();                                       
  

  /* ----------------------- setup ELECTROMAGNETIC SEED GROVE ---------------------------------------*/
  for (int i=0; i< sizeof(pinMagnetArray)/sizeof(int); i++){                
    pinMode(pinMagnetArray[i], OUTPUT);
  }


  /* ----------------------- setup PROXIMITY APDS9960  ---------------------------------------*/
  for (int i=0; i< sizeof(pinProxArray)/sizeof(int); i++){
    pinMode(pinProxArray[i], INPUT_PULLUP);
  }

  /*------------------------ setup MP3-PLAYER VS1053 -----------------------------------------------------*/
  if (! musicPlayer.begin()) {                                                            // sjekker om music player er initialisert. 
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));       // send feilmding hvis ikke
    while (1);}
  Serial.println(F("VS1053 found"));
  if (!SD.begin(CARDCS)) {                                                                // sjekker om SD kortet er detektert. 
    Serial.println(F("SD failed, or not present"));                                       // Send feilmelding hvis ikke.
    while (1);} // 
  musicPlayer.setVolume(1,1);                                                             // MAX VOLUM=(1,1), MEDIUM VOLUM=(20,20), LAV VOLUM=(50,50)

  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);                                    // musikk kan spilles i bakgrunnen


  /* ----------------------- setup APDS9960 and MULTIPLEXER---------------------------------------*/

  tcaselect(0);                                               // sett fargesensoren nr.1 til kanal nr.0 på multiplexer
  startAPDSensor(apds1);                                      // initialiser fargesensor nr.1
  apds1.setProximityInterruptThreshold(0, proxTreshold);      // sett grenseverdien til nærhetssensoren til sensor nr.1
  apds1.enableProximityInterrupt();                           // aktiver 'proximity interrupt' når sensoren har målt grenseverdien
  
  tcaselect(1);                                               // sett fargesensoren nr.2 til kanal nr.1 på multiplexer
  startAPDSensor(apds2);                                      //  initialiser fargesensor nr.2
  apds2.setProximityInterruptThreshold(0, proxTreshold);      // sett grenseverdien til nærhetssensoren til sensor nr.2
  apds2.enableProximityInterrupt();                           // aktiver 'proximity interrupt' når sensoren har målt grenseverdien

  tcaselect(2);                                               // sett fargesensoren nr.3 til kanal nr.2 på multiplexer
  startAPDSensor(apds3);                                      //  initialiser fargesensor nr.3
  apds3.setProximityInterruptThreshold(0, proxTreshold);      // sett grenseverdien til nærhetssensoren til sensor nr.3
  apds3.enableProximityInterrupt();                           // aktiver 'proximity interrupt' når sensoren har målt grenseverdien



}

void loop() {

/*------------------------- START -----------------------------*/
  setCubesIn();                                                   // aktivere elektromagnetene
  ledStripOn(LEDarray[0], 'white');                               // skru på første LED-lys i tidslinja for å vise at spillet har begynt
    
 
  int t = 0;                                                      // iterator for tidslinja. Den inkrementeres for hver riktige runde

  while(t < timeperiod){                                          // vi vet ikke hvor mange forsøk spilleren trenger før den får riktig på hver runde. derfor while og ikke for-loop
    
    /*---------- SJEKK OM ALLE TRE KLOSSENE ER I HULLENE DERES. --------------------------------*/

    if (!digitalRead(pinProx1) && !digitalRead(pinProx2) && !digitalRead(pinProx3)){              // sjekker om nærhetssensoren har nådd grenseverdien og har slått seg av
      LEDcolorSensorON();                                             
                                                          // tar målinger av fargene til klossene:
      tcaselect(0);                                       // bytte kanal på multiplexer for å måle data fra fargesensor nr.1
      readColorSensor(0);                                 // lagrer målte r,g,b,c-verdier i det globale arrayet som tilhører kloss nr.1
      tcaselect(1);                                       // bytte kanal på multiplexer for å måle data fra fargesensor nr.2
      readColorSensor(1);                                 // lagrer målte r,g,b,c-verdier i det globale arrayet som tilhører kloss nr.1
      tcaselect(2);                                       // bytte kanal på multiplexer for å måle data fra fargesensor nr.3
      readColorSensor(2);                                 // lagrer målte r,g,b,c-verdier i det globale arrayet som tilhører kloss nr.1
      
      Serial.print("color data block #1: ");              // printer ut de målte rgbc-verdiene til kloss 1 til serial monitor. bare for å sjekke:-)
      printColorSensor(colorBlock1);
      Serial.print("\n color data block #2: ");           // printer ut de målte rgbc-verdiene til kloss 2 til serial monitor. bare for å sjekke:-)
      printColorSensor(colorBlock2);
      Serial.print("\n color data block #3: ");           // printer ut de målte rgbc-verdiene til kloss 3 til serial monitor. bare for å sjekke:-)
      printColorSensor(colorBlock3);

      /*------------------------------------------ SJEKK OM KLOSSENE ER RIKTIG PLASSERTE ------------------------------*/

      if(checkColors(colorBlock1, colorAnimals[t]) && checkColors(colorBlock2, colorAnimals[t]) && checkColors(colorBlock3, colorAnimals[t])){ //sjekker arrayet til hver kloss mot arrayet til hvert dyr
        
        ledStripOn(LEDarray[t], 'green');                               // hvis du plasserte klossene riktig: lys grønt på tidslinja
        musicPlayer.playFullFile("/cheering.mp3");                      // spill av bekreftende lyd! 
        }

      else{                                                         // hvis du plasserte klossene feil:
        ledStripOn(LEDarray[t], 'red');                             // blink rødt på tidslinja
        musicPlayer.playFullFile("/fail.mp3");                      // spill av lydfil som indikerer at du har feil
        ledStripOn(LEDarray[t], 'white');}                          // sett LEDfargen tilbake til hvitt, for å indikere at du må prøve den runden på nytt

    /* --------------------------- ENDT RUNDE ----------------------------------*/
    ejectCubes();                                                   // Slipp klossene ut av hullet for å starte neste runde
  }//end if-cubes-in

  else {delay(3000);}                                               // vent 3 sekunder før den sjekker om alle tre klossene er satt inn enda

    apds1.clearInterrupt();                                         //clear interrupt til nærhetsensorene før neste runde. Setter de tilbake på HIGH
    apds2.clearInterrupt();                                         //clear interrupt til nærhetsensorene før neste runde. Setter de tilbake på HIGH
    apds3.clearInterrupt();                                         //clear interrupt til nærhetsensorene før neste runde. Setter de tilbake på HIGH

  }//end while
  // Nå har alle 4 runder blitt spilt og blitt riktig:
  /*------------------------- SPILLET ER VUNNET ------------------------------------*/
  musicPlayer.playFullFile("/drumroll.mp3");                          // spill gratulasjonsmusikk

}//end loop()



/*-----------------------------------------------------------------------------------------------*/
/*----------------------- funksjoner som kalles på i setup() og loop() --------------------------*/
/*-----------------------------------------------------------------------------------------------*/




void tcaselect(uint8_t channel){                                              // funksjon som skal bytte mellom kanalene på multiplexeren
  if (channel > 7) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();  
}//end tcaselect()










void startAPDSensor(Adafruit_APDS9960 apds) {                                 // funksjon som skal initialisere farge- og nærhets-sensorene
  if(!apds.begin()){                                                          // sjekker om den klarte å initialisere APDS9960
    Serial.println("failed to initialize device! Please check your wiring.");  // sender feilmelding hvis ikke
    delay(1000);
  }
  else Serial.println("Device initialized!");                                 // bekrefter at den fikk initialisertAPDS9960 
  delay(3000);
  apds.enableColor(true);                                                     // aktiver fargesensor modus
  apds.enableProximity(true);                                                 // aktiver nærhetssensor modus
}//end startAPDSensor












void readColorSensor(int channel){                              // funksjonen skal ta inn de målte fargeverdiene fra fargesensoren og lagre de i globalt array

  uint16_t r, g, b,c;                                           // deklarerer variabler for å lagre r,g,b,c -verdiene
  
  switch (channel){                                             // sjekker hvilken kanal/fargesensor den skal lagre målingene til
    case 1:
      while(!apds1.colorDataReady()){delay(5);}                 // vent på at fargesensoren er klar til å lese
      apds1.getColorData(&r, &g, &b, &c);                       // les fargeverdier fra fargesensor nr.1
      colorBlock1[0] = r;                                       // lagre de målte verdiene fra sensoren i globalt array til kloss 1 for å kunne sjekke de senere
      colorBlock1[1] = g;
      colorBlock1[2] = b;
      colorBlock1[3] = c;
      break;
    case 2:
      while(!apds2.colorDataReady()){delay(5);}                 // vent på at fargesensoren nr2 er klar til å lese
      apds2.getColorData(&r, &g, &b, &c);                       // les fargeverdier fra fargesensor nr.2
      colorBlock2[0] = r;                                       // lagre de målte verdiene fra sensoren i globalt array til kloss 1 for å kunne sjekke de senere
      colorBlock2[1] = g;
      colorBlock2[2] = b;
      colorBlock2[3] = c;
      break;
    case 3:
      while(!apds3.colorDataReady()){delay(5);}              // vent på at fargesensoren nr.3 er klar til å lese
      apds3.getColorData(&r, &g, &b, &c);                    // les fargeverdier fra fargesensor nr.2
      colorBlock3[0] = r;                                    // lagre de målte verdiene fra sensoren i globalt array til kloss 1 for å kunne sjekke de senere
      colorBlock3[1] = g;
      colorBlock3[2] = b;
      colorBlock3[3] = c;
      break;
    default:
      Serial.print("no inserted apds-argument matched any of the cases");
    break;
  }//end switch
  delay(1000);                                              // vent ett sekund før neste lesing
}//end readColorSensor












void printColorSensor(uint16_t* colorArray){                  // funksjonen skal printe ut de målte r,g,b-verdiene

  char letterArray[3] = {'r','g','b'};

  for (int i=0; i< 3; i++){
    Serial.print(letterArray[i]);
    Serial.print(colorArray[i]);                              // går gjennom elementene til det globale arrayet som tilhører klossen spesifisert i funksjons-argumentet
    Serial.println(" ");
  }
}//end printColorSensor









void ejectCubes(){                                                // funksjonen skal slippe klossene løs fra hullene de er plassert i modulen
  for(int i=0; sizeof(pinMagnetArray)/sizeof(int); i++){          // deaktiverer elektromagnetene
    digitalWrite(pinMagnetArray[i], LOW);
  }
}//end ejectCubes()








void setCubesIn(){                                                  // funksjonen skal trekke klossene til hullene i modulen når de nærmer seg 
  for(int i=0; sizeof(pinMagnetArray)/sizeof(int); i++){            // aktiver elektromagnetene
   digitalWrite(pinMagnetArray[i], HIGH);
  }
}// end setCubesIn()









bool checkColors(uint16_t measuredColors[4], uint16_t correctColors[4]){
  int ncolors = 4;                                                                             //r,g,b,c-verdiene
  int score = 0;
 
  for(int i=0; i<ncolors; i++){                                                                // kryssjekker om fargene i det globale arrayet er lik fargene til det korresponderende dyret
    
    if (measuredColors[i] <= (correctColors[i]+tolerance) || measuredColors[i] >= correctColors[i]-tolerance){

      score++;                                                                                 // hvis riktig innenfor en viss toleranse, så inkrementer score med 1.
    }
  }
  if(score==ncolors){return true;}                                                            // hvis alle tre fargeverdiene er riktige, har klossene blitt plassert riktig. return true
  else{return false;}                                                                         // hvis ikke alle tre fargeverdiene er riktgie, så klossene blitt plassert feil.
}// end checkColors







void LEDcolorSensorON(){                                                                // funksjonen skal skru på lyset slik at fargesensorene har nok leselys
  for(int i=0; i<nColorSensorLED; i++) {                                                // sett på lysene til neopixelstripa
    colorSensorLED.setPixelColor(i, colorSensorLED.Color(255,255,255));                 // alle skal ha hvit farge for å ikke påvirke målingene til fargene på klossene
    colorSensorLED.show();                                                              
  }
}// end LEDcolorSensorON()








void ledStripOn(int lightNum, char color[]){                  // funksjonen skal lyse opp tidslinjen ettersom tidsepokene er fullførte 
  
  int expression;                                            // omdanner fargen til en int, da switch-expression kun tar inn int-type og ikke char-type.
  if (color == 'green'){expression = 1;}
  else if (color == 'white'){expression = 2;}
  else if (color == 'red'){expression = 3;}

  switch(expression){ 
  	case 1:                                                 
    	timelineLED.setPixelColor(lightNum, timelineLED.Color(0,150,0));          // sett fargen på LEDlysnr lightNum på stripa til grønt
      Serial.println("green");
    	timelineLED.show();
    	break;
  	case 2:
      Serial.println("white");
    	timelineLED.setPixelColor(lightNum, timelineLED.Color(255,255,255));     // sett fargen på LEDlysnr lightNum på stripa til hvit
    	timelineLED.show();
    	break;
    
    case 3:
      int i =0;
      while(i <4){
        timelineLED.setPixelColor(lightNum, timelineLED.Color(150,0,0));    // sett fargen på LEDlysnr lightNum på stripa til rødt (den skal blinke)
        delay(400);                                                         // vent litt før fargen settes til 0 igjen
        timelineLED.show();
        Serial.println("red");
        timelineLED.setPixelColor(lightNum, timelineLED.Color(0,0,0));    
        delay(200);
        Serial.println("no light");
        timelineLED.show();
        i++;}// end while
    default:
    	Serial.print("color request for timeline is not accepted.");
    break;
   }//end switch
  
}//end ledStrip()