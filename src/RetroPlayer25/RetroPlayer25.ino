/*

Project RetroPlaye25
===================================================================

Try to build a retro MOD player with OLED screen.

Wells Wang
geek-logic.com, 2025

Release under GPLv3 License.

Based on earlephilhower's ESP8266Audio library: https://github.com/earlephilhower/ESP8266Audio

====================================================================

--- W I R E   D I A G R A M ---

DAC (PCM5102):
  VCC, 33V, XMT, (SCK)      -> 3.3V
  GND, FLT, DMP, FMT, SCL   -> GND
  BCLK(BCK)                 -> D26
  I2SO(DIN)                 -> D22
  LRCLK(WS/LCK)             -> D25

SD card:
  CS          -> D5
  MOSI        -> D23
  CLK         -> D18
  MISO        -> D19

OLED SSD1306 SPI Mode:
  CLK(SCK)    -> D18
  SDA         -> D23
  RES(RESET)  -> D17
  DC          -> D16
  CS          -> D4

Rotate Switch:
  SW_A        -> D32
  SW_B        -> D35
  SW_Push     -> D34

PAM8403:
  MUTE        -> D33

EARPHONE:
  CONNECT     -> D21
--------------------------------

*/



#include <Arduino.h>
//#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMOD.h"
#include "AudioOutputI2S.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#if defined(ARDUINO_ARCH_RP2040)
    #define WIFI_OFF
    class __x { public: __x() {}; void mode() {}; };
    __x WiFi;
#elif defined(ESP32)
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif

#define SCREEN_WIDTH    128 // OLED display width, in pixels
#define SCREEN_HEIGHT   64 // OLED display height, in pixels

/*
#define OLED_RESET      -1 
#define SCREEN_ADDRESS  0x3C
#define SCREEN_SCL      4
#define SCREEN_SDA      21
Adafruit_SSD1306        display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
*/

// Comment out above, uncomment this block to use hardware SPI
#define OLED_DC     16
#define OLED_CS     4
#define OLED_RESET  17
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

#define SW_P        34
#define SW_A        32
#define SW_B        35
#define SW_GAP      150000

#define MUTE        33
#define EARPHONE    21
// enigma.mod sample from the mod archive: https://modarchive.org/index.php?request=view_by_moduleid&query=42146
//#include "enigma.h"

#define ICON_WIDTH 8
#define ICON_HEIGHT 8
#define ICON_SIZE 8
static const unsigned char PROGMEM icons[] =
{ 0b00100000, 
  0b00110000, 
  0b00111000, 
  0b00111100, 
  0b00111000, 
  0b00110000, 
  0b00100000, 
  0b00000000, 

  0b01010000, 
  0b01011000, 
  0b01011100, 
  0b01011110, 
  0b01011100, 
  0b01011000, 
  0b01010000, 
  0b00000000, 

  0b01000010, 
  0b01100010, 
  0b01110010, 
  0b01111010, 
  0b01110010, 
  0b01100010, 
  0b01000010, 
  0b00000000, 

  0b01000010, 
  0b01000110, 
  0b01001110, 
  0b01011110, 
  0b01001110, 
  0b01000110, 
  0b01000010, 
  0b00000000, 

  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b00000000, 

 };


File dir;
AudioGeneratorMOD *mod;
AudioFileSourceSD *file = NULL;
AudioOutputI2S *out;
bool next = false;
float gain = 0.02;
float maxgain, step;
float prg = 0.0;
float lastprg = 0.0;
unsigned long t, lasttime, lastkeytime;
byte last_a = 1;
bool earphone = false;
char* desc; 

void drawProgressBar(int x, int y, int length, float percent, int mode)
{
  int pct = int(percent / 100.0 * (length - 2));
  if (mode == 0) display.drawRect(x, y, length, 4, SSD1306_WHITE);
  if (mode < 2) display.fillRect(x + 1, y + 1, pct , 2, SSD1306_WHITE);
  if (mode == 2) display.fillRect(x + 1 + pct, y + 1, length - 3 - pct, 2, SSD1306_BLACK);
}

void drawIcon(int x, int y, byte id, int color) {
  unsigned char icon[ICON_SIZE];
  for (int i=0; i<ICON_SIZE; i++){
    icon[i] = pgm_read_word_near(icons + id * ICON_SIZE + i);
   // Serial.println(txt[i],BIN);
  }
  display.drawBitmap(x,y,icon, ICON_WIDTH, ICON_HEIGHT, color);
}

void drawButton(int x, int y, int size, byte id, bool active){
  int color;
  if (active) {
    color = SSD1306_BLACK;
    display.fillRoundRect(x, y, 8 * (size + 2), 11, 3, SSD1306_WHITE);

  } else {
    color = SSD1306_WHITE;
    display.fillRoundRect(x, y, 8 * (size + 2), 11, 3, SSD1306_BLACK);
    display.drawRoundRect(x, y, 8 * (size + 2), 11, 3, SSD1306_WHITE);
  }
 for (int i=0; i<size; i++)
   drawIcon(x + 8 * (i + 1), y + 2, id + i, color);
}

void drawUI()
{
  display.clearDisplay();
  display.fillRect(0, 0, display.width()-1 , 8, SSD1306_WHITE);
  display.setTextSize(1);  
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(0, 0);
  display.println(F(":: RETROPLAYER 25 ::"));
  display.setCursor((display.width()-54)/2-1 , 9);
  display.println(F("mod - 4ch"));
  drawProgressBar(0, 60, 128, 0.0, 0);
  for (int i=0; i<5; i++) {
    drawButton((i%3)*42, 30 + i / 3 * 12 , 1, i, 1);
  }
  display.display();
}

void updateSongInfo(String name, int ch, int pt)
{
  name.toUpperCase();
  name = name.substring(0, name.length() - 4);
  if (name.length() > 21) name = name.substring(0, 18) + "...";
  display.fillRect(0, 0, display.width()-1, 8, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.fillRect(0, 8, display.width()-1, 10, SSD1306_BLACK);
  display.setCursor((display.width()- 6 * name.length()) / 2 - 1, 0);
  display.println(name); 
  String info = String(pt) + "pt | " + ch + "ch";
  display.setCursor((display.width() - 6 * info.length())/2-1 , 9);
  display.print(info);
  display.display();
}

void setup()
{
  WiFi.mode(WIFI_OFF); //WiFi.forceSleepBegin();
  Serial.begin(115200);
  delay(1000);

  pinMode(SW_A, INPUT);
  pinMode(SW_B, INPUT);
  pinMode(SW_P, INPUT);
  
  pinMode(MUTE, OUTPUT);
  pinMode(EARPHONE, INPUT);
  digitalWrite(MUTE, HIGH);
  earphone = true;
  maxgain = 0.3;
  step = 0.01;

  audioLogger = &Serial;
  file = new AudioFileSourceSD();
  //file = new AudioFileSourcePROGMEM( enigma_mod, sizeof(enigma_mod) );
  // out = new AudioOutputI2S(0, 1); Uncomment this line, comment the next one to use the internal DAC channel 1 (pin25) on ESP32
  out = new AudioOutputI2S();
  mod = new AudioGeneratorMOD();

/*
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  Wire.begin(SCREEN_SDA, SCREEN_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
*/
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  drawUI();

  if (!SD.begin(5, SPI, 8000000)) {
    Serial.println("SD initialization failed!");
    display.setCursor(0, 18);
    display.println(F("SD INIT FAILED!"));
    display.display();
    while (1);
  }

  dir = SD.open("/"); 

  mod->SetBufferSize(6*1024);
  mod->SetSampleRate(33000);
  mod->SetStereoSeparation(32);
  t = micros();
  lasttime = t;
  lastkeytime = t;
  //mod->SetSkipJump(true);
}

void loop()
{
  int cmd = 0;
  if ((mod) && mod->isRunning() && !next) {
    if (!mod->loop()) mod->stop();
    if (Serial.available() > 0) {
      // read the incoming byte:
      cmd = Serial.read();
      switch(cmd){
        case 'n': next = true;
                  break;
        case '+': gain += step;
                  out->SetGain(gain);
                  break;
        case '-': gain -= step;
                  out->SetGain(gain);
                  break;
        case 'j': mod->SetSkipJump(!mod->GetSkipJump());
                  break;
      }
    } 
    t = micros();
    if ((t - lasttime) > 100000 ) {
      prg = float(mod->GetPlayPos()) / mod->GetSongLength() * 100;
      //Serial.println(prg);
      if (prg > lastprg) {
        drawProgressBar(0, 60, 128, prg, 1);
        display.display();
        lastprg = prg;
      } else if (prg < lastprg) {
        drawProgressBar(0, 60, 128, prg, 2);
        display.display();
        lastprg = prg;
      }

      lasttime = t;
    }

    if (digitalRead(EARPHONE)!=earphone){
      earphone = !earphone;
      if (!earphone){
        maxgain = 3;
        step = 0.1;
        gain = gain * 10;
      } else {
        maxgain = 0.3;
        step = 0.01;
        gain = gain / 10.0;
        if (gain > maxgain) gain = maxgain;
      }
      out->SetGain(gain);
      digitalWrite(MUTE, earphone);
    }

    byte a;
    a = digitalRead(SW_A);
    if (a != last_a){
      t = micros(); 
      last_a = a;
      if (t - lastkeytime > SW_GAP ){
        if (digitalRead(SW_B) != a) {
          if (gain > 0) {
            gain -= step;
            out->SetGain(gain);
            Serial.println(gain);
          }
          Serial.println("--");
        } else {
          if (gain < maxgain) {
            gain += step;
            out->SetGain(gain);
            Serial.println(gain);
          }
          Serial.println("++");
        }
        lastkeytime = t;
      }
      // 修正延迟导致的波形匹配错误
      a = digitalRead(SW_A);
      last_a = a;
      ///////
    }
    if (!digitalRead(SW_P)){
      t = micros();
      if (t - lastkeytime > SW_GAP * 2){
        next = true;
        Serial.println("VV");
        lastkeytime = t;
      }
    } 

   /* for (int i = 0; i < mod->GetChannelNumber(); i++){
      Serial.print(mod->GetChannelFreq(i));
      Serial.print(" | ");      
    }*/
    //Serial.println("");
  } else {
    next = false;
    Serial.printf("MOD done\n");
    delay(1000);
    File f = dir.openNextFile();
    if (f) {      
      if (String(f.name()).endsWith(".mod")) {
        if (mod->isRunning()) mod->stop();
        file->close();
        if (file->open(("/"+String(f.name())).c_str())) { 
          Serial.printf_P(PSTR("Playing '%s' from SD card...\n"), f.name());
          out->SetGain(gain);
          mod->begin(file, out);
          desc = mod->GetFileDescription();
          Serial.println(desc+20);
          updateSongInfo(f.name(), mod->GetChannelNumber(), mod->GetPatternNumber());
          Serial.print(mod->GetChannelNumber());
          Serial.print(" / ");
          Serial.println(mod->GetPatternNumber());
          Serial.println(mod->GetSongLength());
          delay(1000);
        } else {
          Serial.printf_P(PSTR("Error opening '%s'\n"), f.name());
        }
      }
    } else {
      Serial.println(F("Playback from SD card done\n"));
      if (mod->isRunning()) mod->stop();
      file->close();
      dir = SD.open("/"); 
      delay(1000);
    }       
  }
}

