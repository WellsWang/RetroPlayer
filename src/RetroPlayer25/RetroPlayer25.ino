/*

Project RetroPlaye25
===================================================================

Try to build a retro MOD player with OLED screen.
尝试用ESP32构建支持OLED显示的复古MOD音乐播放器

Wells Wang
geek-logic.com, 2025

Release under GPLv3 License.
遵循GPLv3协议开源

Based on earlephilhower's ESP8266Audio library: https://github.com/earlephilhower/ESP8266Audio
基于earlephilhower的ESP8266Audio库：https://github.com/earlephilhower/ESP8266Audio

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
{ // 0, |< Prev
  0b01000010, 
  0b01000110, 
  0b01001110, 
  0b01011110, 
  0b01001110, 
  0b01000110, 
  0b01000010, 
  0b00000000, 

  // 1, || Pause
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b01100110, 
  0b00000000, 

  // 2, >| Next
  0b01000010, 
  0b01100010, 
  0b01110010, 
  0b01111010, 
  0b01110010, 
  0b01100010, 
  0b01000010, 
  0b00000000,

  // 3, > Play
  0b00100000, 
  0b00110000, 
  0b00111000, 
  0b00111100, 
  0b00111000, 
  0b00110000, 
  0b00100000, 
  0b00000000, 

  // 4, |> Pause/Play
  0b01010000, 
  0b01011000, 
  0b01011100, 
  0b01011110, 
  0b01011100, 
  0b01011000, 
  0b01010000, 
  0b00000000, 

  // 5, [] Stop
  0b00000000, 
  0b01111100, 
  0b01111100, 
  0b01111100, 
  0b01111100, 
  0b01111100, 
  0b00000000, 
  0b00000000, 

  // 6, Selected
  0b11111110, 
  0b11000110, 
  0b10101010, 
  0b10010010, 
  0b10101010, 
  0b11000110, 
  0b11111110, 
  0b00000000, 

  // 7, not Selected
  0b11111110, 
  0b10000010, 
  0b10000010, 
  0b10000010, 
  0b10000010, 
  0b10000010, 
  0b11111110, 
  0b00000000, 

  // 8, Volume
  0b00000000, 
  0b00000011, 
  0b00000011, 
  0b00011011, 
  0b00011011, 
  0b11011011, 
  0b11011011, 
  0b00000000, 

  // 9, Pattern
  0b01111110, 
  0b10001111, 
  0b10110111, 
  0b10001011, 
  0b10110001, 
  0b10111011, 
  0b10111001, 
  0b01111110, 

  // 10, Channel
  0b01111110, 
  0b11001111, 
  0b10110111, 
  0b10110111, 
  0b10110001, 
  0b10110101, 
  0b11000101, 
  0b01111110, 

};

#define MAX_ITEMS 5
unsigned char menu[] = { 0, 5, 2, 7, 8};
static const unsigned char PROGMEM menu_x[] = { 0, 28, 56, 0, 77};
static const unsigned char PROGMEM menu_y[] = { 36, 36, 36, 50, 50};
String menu_items[MAX_ITEMS] = {"", "", "", "SkipJump", "Vol" };

File dir, f;
AudioGeneratorMOD *mod;
AudioFileSourceSD *file = NULL;
AudioOutputI2S *out;
bool next = false;
bool prev = false;
float gain = 0.02;
float maxgain, step;
float prg = 0.0;
float lastprg = 0.0;
unsigned long t, lasttime, lastkeytime;
byte last_a = 1;
bool earphone = false;
char* desc; 
byte cur_item = 1;
byte submenu = 0;
unsigned int totalfiles = 0;
unsigned int cur_file = 0;

void drawProgressBar(int x, int y, int length, float percent, int mode)
{
  int pct = int(percent / 100.0 * (length));
  //if (mode == 0) display.drawRect(x, y, length, 4, SSD1306_WHITE);
  if (mode < 2) display.fillRect(x , y, pct , 2, SSD1306_WHITE);
  if (mode == 2) display.fillRect(x + pct, y, length - pct, 2, SSD1306_BLACK);
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
    display.fillRoundRect(x, y, 8 * (size + 2), 9, 3, SSD1306_WHITE);

  } else {
    color = SSD1306_WHITE;
    display.fillRoundRect(x, y, 8 * (size + 2), 9, 3, SSD1306_BLACK);
    display.drawRoundRect(x, y, 8 * (size + 2), 9, 3, SSD1306_WHITE);
  }
 for (int i=0; i<size; i++)
   drawIcon(x + 8 * (i + 1), y + 1, id + i, color);
}

void drawOption(int x, int y, byte id, byte cap_index, bool active ){
  int color1, color2;
  if (active) {
    color1 = SSD1306_BLACK;
    color2 = SSD1306_WHITE;
  } else {
    color1 = SSD1306_WHITE;
    color2 = SSD1306_BLACK;
  }

  display.fillRect(x, y, menu_items[cap_index].length() * 6 + 10 , 8, color2);
  display.setTextColor(color1);
  drawIcon(x, y , id, color1);
  display.setCursor(x + 10, y);
  display.print(menu_items[cap_index]);
}

void drawMenuItem(byte i, bool active){
  if (menu_items[i].length() > 0)
    drawOption(menu_x[i], menu_y[i], menu[i], i, active);
  else
    drawButton(menu_x[i], menu_y[i], 1, menu[i], active);
}

void drawMenu(){
  bool active;
  for (int i = 0; i < MAX_ITEMS; i++){
    if (cur_item == i)
      active = true;
    else
      active = false;
    drawMenuItem(i, active);
  }
} 

void drawUI()
{
  display.clearDisplay();
  display.fillRect(0, 0, display.width() - 1 , 8, SSD1306_WHITE);
  display.setTextSize(1);  
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(0, 0);
  display.println(F(":: RETROPLAYER 25 ::"));
  display.setTextColor(SSD1306_WHITE);
  //display.setCursor((display.width()-54)/2-1 , 27);
  //display.println(F("mod - 4ch"));
  display.drawLine(0, 31, display.width() - 1, 31, SSD1306_WHITE);
  drawIcon(86, 36, 10, SSD1306_WHITE);
  drawIcon(106, 36, 9, SSD1306_WHITE);
  drawMenu();
  drawProgressBar(0, 62, 128, 0.0, 1);
  updateGainInfo();
  display.display();
}

void printDescription(uint8_t x, uint8_t y)
{
  uint8_t t = desc[20];
  desc[20] = 0;
  display.setCursor(x, y);
  display.print(desc);
  desc[20] = t;
  display.setCursor(x, y + 8);
  t = desc[40];
  desc[40] = 0;
  display.print(desc + 20);
  desc[40] = t;
}

void updateSongInfo(String name, int ch, int pt)
{
  name.toUpperCase();
  name = name.substring(0, name.length() - 4);
  if (name.length() > 21) name = name.substring(0, 18) + "...";
  display.fillRect(0, 0, display.width()-1, 8, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.fillRect(0, 11, display.width()-1, 16, SSD1306_BLACK);
  display.setCursor((display.width()- 6 * name.length()) / 2 - 1, 0);
  display.println(name); 
  display.setTextColor(SSD1306_WHITE);
  printDescription(0, 11);

  display.fillRect(96, 36, 8, 8, SSD1306_BLACK);
  display.setCursor(96, 36);
  display.print(ch);

  display.fillRect(116, 36, 12, 8, SSD1306_BLACK);
  display.setCursor(116, 36);
  display.print(pt);

  display.display();
}

void updateGainInfo(){
  display.fillRect(110, 50, 18, 8, SSD1306_BLACK);
  display.setCursor(110, 50);
  display.setTextColor(SSD1306_WHITE);
  display.print(int(gain*100/maxgain));
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
  while(f = dir.openNextFile()){
    if (String(f.name()).endsWith(".mod"))
      totalfiles++;
  }
  Serial.print("Total Files: ");
  Serial.println(totalfiles);

  mod->SetBufferSize(6*1024);
  mod->SetSampleRate(33000);
  mod->SetStereoSeparation(32);
  t = micros();
  lasttime = t;
  lastkeytime = t;
  //mod->SetSkipJump(true);
  //next = true;

}

void loop()
{
  int cmd = 0;

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

  byte a;
  a = digitalRead(SW_A);
  if (a != last_a){
    t = micros(); 
    last_a = a;
    if (t - lastkeytime > SW_GAP ){
      if (digitalRead(SW_B) != a) { //左
        if(submenu == 0){
          drawMenuItem(cur_item, false);
          if (cur_item == 0)
            cur_item = MAX_ITEMS - 1;
          else
            cur_item--;
          drawMenuItem(cur_item, true);
          display.display();
        } else if (submenu == 1) {
          switch(cur_item) {
            case 4:
                    if (gain > 0) {
                      gain -= step;
                      out->SetGain(gain);
                      updateGainInfo();
                      Serial.println(gain);
                    }
                    break;
          }
        }
        Serial.println(cur_item);
      } else {                      //右
        if(submenu == 0){
          drawMenuItem(cur_item, false);
          if (cur_item == MAX_ITEMS - 1)
            cur_item = 0;
          else
            cur_item++;
          drawMenuItem(cur_item, true);
          display.display();
        } else if (submenu == 1) {
          switch(cur_item) {
            case 4:
                    if (gain < maxgain) {
                      gain += step;
                      out->SetGain(gain);
                      updateGainInfo();
                      Serial.println(gain);
                    }
                    break;
          }
        }
        Serial.println(cur_item);
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
      if (submenu == 0){
        switch (cur_item){
          case 0:
                  prev = true;
                  //Serial.println("VV");
                  if (menu[1] == 3) {
                    menu[1] = 5;
                    drawMenuItem(1, false);
                    display.display();
                  }
                  break;
          case 1:
                  if (mod->isRunning()){
                    mod->stop();
                    menu[1] = 3;
                    drawMenuItem(cur_item, true);
                  } else {
                    file->open(("/"+String(f.name())).c_str());
                    mod->begin(file, out);
                    menu[1] = 5;
                    drawMenuItem(cur_item, true);
                  }
                  display.display();
                  break;
          case 2:
                  next = true;
                  //Serial.println("VV");
                  if (menu[1] == 3) {
                    menu[1] = 5;
                    drawMenuItem(1, false);
                    display.display();
                  }
                  break;
          case 3:
                  mod->SetSkipJump(!mod->GetSkipJump());
                  if (menu[3]==6)
                    menu[3] = 7;
                  else
                    menu[3] = 6;
                  drawMenuItem(cur_item, true);
                  display.display();
                  break;
          case 4:
                  submenu = 1;
                  break;

        }

      } else if (submenu == 1) {
        switch (cur_item){
          case 4:
                  submenu = 0;
        }
      }
      lastkeytime = t;
    }
  } 


  if ((mod) && mod->isRunning() && !(next || prev) ) {

    t = micros();

    if (!mod->loop()) mod->stop();
    if ((t - lasttime) > 100000 ) {
      prg = float(mod->GetPlayPos()) / mod->GetSongLength() * 100;
      //Serial.println(prg);
      if (prg > lastprg) {
        drawProgressBar(0, 62, 128, prg, 1);
        display.display();
        lastprg = prg;
      } else if (prg < lastprg) {
        drawProgressBar(0, 62, 128, prg, 2);
        display.display();
        lastprg = prg;
      }
      lasttime = t;
    }


   /* for (int i = 0; i < mod->GetChannelNumber(); i++){
      Serial.print(mod->GetChannelFreq(i));
      Serial.print(" | ");      
    }*/
    //Serial.println("");
  } else if (menu[1] != 3) {
    next = false;
    Serial.printf("MOD done\n");
    delay(1000);
    if (prev){
      unsigned int tgt_file, t;
      if (cur_file > 1)
        tgt_file = cur_file - 1;
      else
        tgt_file = totalfiles;

      Serial.print("Target - ");
      Serial.println(tgt_file);
      dir = SD.open("/"); 
      t = 0;
      while (t < tgt_file - 1) {
        Serial.print("CHK_");
        f = dir.openNextFile();
        if (f) {      
          if (String(f.name()).endsWith(".mod")) {
            t++;
            Serial.print(t);
            Serial.println(String(f.name()));
          }
        }
      }
      cur_file = tgt_file - 1;
      prev = false;
      Serial.print("Curr - ");
      Serial.println(cur_file);
    }
    while (f = dir.openNextFile()){
      if (String(f.name()).endsWith(".mod")) break;
    }
    
    if (f) {      
      cur_file++;
      Serial.print(cur_file);
      Serial.println(String(f.name()));
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
    } else {
      Serial.println(F("Playback from SD card done\n"));
      if (mod->isRunning()) mod->stop();
      file->close();
      dir = SD.open("/"); 
      cur_file = 0;
      delay(1000);
    }       
  }
}

