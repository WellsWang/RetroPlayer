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

#define SCREEN_WIDTH    128 // OLED display width, in pixels / 显示屏宽度
#define SCREEN_HEIGHT   64 // OLED display height, in pixels / 显示屏高度

// Use hardware SPI to drive OLED / 使用硬件SPI驱动OLED屏
#define OLED_DC     16
#define OLED_CS     4
#define OLED_RESET  17
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

// Rotary encoder settings / 旋转编码器
#define SW_P        34
#define SW_A        32
#define SW_B        35
#define SW_GAP      150000  // Jitter time, 放置信号抖动的冗余时间

// Earphone and Speaker Muter / 耳机侦测和扬声器静音
#define MUTE        33
#define EARPHONE    21

// Icon settings / 图标设置
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

// UI Control items / UI控制控件设置
#define MAX_ITEMS 5       // Max control unit number / 控制控件数量
unsigned char menu[] = { 0, 5, 2, 7, 8};    // CU icon index array / 控件图标
static const unsigned char PROGMEM menu_x[] = { 0, 28, 56, 0, 77};    // CU position X / 控件坐标X
static const unsigned char PROGMEM menu_y[] = { 36, 36, 36, 50, 50};  // CU position Y / 控件坐标Y
String menu_items[MAX_ITEMS] = {"", "", "", "SkipJump", "Vol" };      // CU caption / 控件标题文本

// File relative / 文件相关变量
File dir, f;

// Audio process relative / 音频处理相关
AudioGeneratorMOD *mod;
AudioFileSourceSD *file = NULL;
AudioOutputI2S *out;

// Control relative / 控制相关
bool next = false;
bool prev = false;
unsigned long t, lasttime, lastkeytime;
byte last_a = 1;
byte cur_item = 1;
byte submenu = 0;
unsigned int totalfiles = 0;
unsigned int cur_file = 0;
float gain = 0.02;
float maxgain, step;
bool earphone = false;

// Display relative / 显示相关
float prg = 0.0;
float lastprg = 0.0;
char* desc; 


// -- Part 1 -- UI relative function / UI相关函数 

// ProgressBar / 进度条
void drawProgressBar(int x, int y, int length, float percent, int mode)
{
  int pct = int(percent / 100.0 * (length));
  //if (mode == 0) display.drawRect(x, y, length, 4, SSD1306_WHITE);              // （绘制进度条边框，已取消此功能）
  if (mode < 2) display.fillRect(x , y, pct , 2, SSD1306_WHITE);                  // 增长时绘制白色进度条
  if (mode == 2) display.fillRect(x + pct, y, length - pct, 2, SSD1306_BLACK);    // 缩减时用黑色擦除多余的进度
}

// Icon / 图标
void drawIcon(int x, int y, byte id, int color) {
  unsigned char icon[ICON_SIZE];
  for (int i=0; i<ICON_SIZE; i++){
    icon[i] = pgm_read_word_near(icons + id * ICON_SIZE + i);   //按索引号读取图标数据
  }
  display.drawBitmap(x,y,icon, ICON_WIDTH, ICON_HEIGHT, color);
}

// Button / 按钮
void drawButton(int x, int y, int size, byte id, bool active)
{
  int color;
  if (active) { //激活按钮，绘制实心圆角矩形
    color = SSD1306_BLACK;
    display.fillRoundRect(x, y, 8 * (size + 2), 9, 3, SSD1306_WHITE);

  } else {  // 非激活按钮，先绘制黑色实心圆角矩形（擦除原图形）后再绘制空心圆角矩形
    color = SSD1306_WHITE;
    display.fillRoundRect(x, y, 8 * (size + 2), 9, 3, SSD1306_BLACK);
    display.drawRoundRect(x, y, 8 * (size + 2), 9, 3, SSD1306_WHITE);
  }
 for (int i=0; i<size; i++) // 绘制图标，可以用多个连续图标拼接按钮上的图形
   drawIcon(x + 8 * (i + 1), y + 1, id + i, color);
}

// Option / 选项
void drawOption(int x, int y, byte id, byte cap_index, bool active )
{
  int color1, color2;
  if (active) { //激活时反色显示
    color1 = SSD1306_BLACK;
    color2 = SSD1306_WHITE;
  } else {
    color1 = SSD1306_WHITE;
    color2 = SSD1306_BLACK;
  }

  display.fillRect(x, y, menu_items[cap_index].length() * 6 + 10 , 8, color2);  //用背景色擦除
  display.setTextColor(color1);
  drawIcon(x, y , id, color1);
  display.setCursor(x + 10, y);
  display.print(menu_items[cap_index]);
}

// Control Unit Item / 绘制单个控件
void drawMenuItem(byte i, bool active){
  if (menu_items[i].length() > 0)   // 如果控件的标题文本为空，则画按钮，否则画选择项
    drawOption(menu_x[i], menu_y[i], menu[i], i, active);
  else
    drawButton(menu_x[i], menu_y[i], 1, menu[i], active);
}

// Control Panel / 绘制控件区
void drawMenu(){
  bool active;
  for (int i = 0; i < MAX_ITEMS; i++){
    if (cur_item == i)  // 判断当前空间是否激活
      active = true;
    else
      active = false;
    drawMenuItem(i, active);
  }
} 

// UI Initialize / UI界面初始化 
void drawUI()
{
  display.clearDisplay();
  display.fillRect(0, 0, display.width() - 1 , 8, SSD1306_WHITE);
  display.setTextSize(1);  
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(0, 0);
  display.println(F(":: RETROPLAYER 25 ::"));
  display.setTextColor(SSD1306_WHITE);
  display.drawLine(0, 31, display.width() - 1, 31, SSD1306_WHITE);
  drawIcon(86, 36, 10, SSD1306_WHITE);
  drawIcon(106, 36, 9, SSD1306_WHITE);
  drawMenu();
  drawProgressBar(0, 62, 128, 0.0, 1);
  updateGainInfo();
  display.display();
}

// File Description / 文件注释信息
void printDescription(uint8_t x, uint8_t y)
{ 
  // 将第21字节设置为0x00，以显示前20个字节
  uint8_t t = desc[20];
  desc[20] = 0;
  display.setCursor(x, y);
  display.print(desc);
  // 还原第20字节后，将第41字节设置为0x00，以显示后20字节
  desc[20] = t;
  display.setCursor(x, y + 8);
  t = desc[40];
  desc[40] = 0;
  display.print(desc + 20);
  // 还原第41字节内容
  desc[40] = t;
}

// Update Music Info / 更新音乐的相关信息
void updateSongInfo(String name, int ch, int pt)
{
  // 文件名信息
  name.toUpperCase();
  name = name.substring(0, name.length() - 4);
  if (name.length() > 21) name = name.substring(0, 18) + "...";   //长文件名截短
  display.fillRect(0, 0, display.width()-1, 8, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.fillRect(0, 11, display.width()-1, 16, SSD1306_BLACK);
  display.setCursor((display.width()- 6 * name.length()) / 2 - 1, 0);
  display.println(name); 

  // 文件注释信息
  display.setTextColor(SSD1306_WHITE);
  printDescription(0, 11);

  // 通道数量
  display.fillRect(96, 36, 8, 8, SSD1306_BLACK);
  display.setCursor(96, 36);
  display.print(ch);

  // 格式数量
  display.fillRect(116, 36, 12, 8, SSD1306_BLACK);
  display.setCursor(116, 36);
  display.print(pt);

  // 完成绘制更新显示
  display.display();
}

// Volume / 音量
void updateGainInfo()
{
  int vol = int(gain * 100 / maxgain);  //当前音量百分比
  if (vol > 100) vol = 100;

  display.fillRect(110, 50, 18, 8, SSD1306_BLACK);
  display.setCursor(110, 50);
  display.setTextColor(SSD1306_WHITE);
  display.print(vol); 
  display.display();
}

// Update ProgressBar / 更新进度条
void updateProgress()
{
  prg = float(mod->GetPlayPos()) / mod->GetSongLength() * 100;
  if (prg > lastprg) {                      // 进度增加
    drawProgressBar(0, 62, 128, prg, 1);
    display.display();
    lastprg = prg;
  } else if (prg < lastprg) {               // 进度回退
    drawProgressBar(0, 62, 128, prg, 2);
    display.display();
    lastprg = prg;
  }
}

// -- Part 2 -- Control Part / 控制部分

// update Play Button / 更新播放按键的状态
void startPlay()
{
  // 将停止播放状态变更为播放状态 （根据控件1的图标编号来判断是否处于播放状态）
  if (menu[1] == 3) {         // 编号为 3 ，播放箭头，图标为 ">"时，处于停止播放状态
    menu[1] = 5;              // 编号为 5 ，方块，按钮上图标为“[]”时，处于正在播放状态 
    drawMenuItem(1, false);
    display.display();
  }
}

// Stop and Start Playing / 停止或开始播放
void toggleStopStart()
{
  //停止或开始播放音乐
  if (mod->isRunning()){      // 处于播放状态
    mod->stop();
    menu[1] = 3;              // 将图标设为编号 3 ，播放箭头，图标为 ">"时，处于停止播放状态
  } else {
    file->open(("/"+String(f.name())).c_str());   // 重新打开文件
    mod->begin(file, out);
    menu[1] = 5;              // 将图标设为编号 5 ，方块，按钮上图标为“[]”时，处于正在播放状态 
  }

  drawMenuItem(cur_item, true);
  display.display();
}

// Toggle SkipJump Function / 切换强制跳过跳转的功能
void toggleSkipJump()
{
  mod->SetSkipJump(!mod->GetSkipJump());  // 切换SkipJump标志的值
  if (menu[3]==6)                         // 切换显示的图标
    menu[3] = 7;
  else
    menu[3] = 6;
  drawMenuItem(cur_item, true);
  display.display();
}

// Close Music File / 停止播放关闭音乐文件
void closeMusic()
{
    //结束当前播放
    if (mod->isRunning()) mod->stop();
    file->close();
}

// Open Music File / 打开并播放音乐文件
void openMusicFile()
{
  // 结束当前播放
  closeMusic();

  // 打开文件
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

// Rewind Folder / 跳转到文件目录开始
void rewindFolder()
{
  // 结束当前播放
  closeMusic();

  Serial.println(F("Playback from SD card done\n"));
  //重新打开目录，回到开头
  dir = SD.open("/"); 
  delay(1000);
}

// update Button / 更新按钮状态
void updateButton(int step)
{
  drawMenuItem(cur_item, false);        // 重新绘制当前控件为未激活状态
  cur_item = (cur_item + step + MAX_ITEMS) % MAX_ITEMS; // 当前激活的控件编号变更，超过范围时从头部或尾部循环
  drawMenuItem(cur_item, true);         // 绘制新的激活的控件
  display.display();
}

// Volume Control / 音量控制
void changeVolume(float step)
{
  if (gain > 0 && gain < maxgain) {
    gain += step;             // 音量调整
    if (gain < 0) gain = 0;
    if (gain > maxgain) gain = maxgain;
    out->SetGain(gain);
    updateGainInfo();         // 更新音量显示
    //Serial.println(gain);
  }  
}

// -- Part 3 -- Initialize / 初始化

void setup()
{
  WiFi.mode(WIFI_OFF); //WiFi.forceSleepBegin();
  Serial.begin(115200);
  delay(1000);
  
  // Rotary encoder / 旋转编码器
  pinMode(SW_A, INPUT);
  pinMode(SW_B, INPUT);
  pinMode(SW_P, INPUT);
  
  // Earphone detect & Speaker Muter / 耳机检测和扬声器静音
  pinMode(MUTE, OUTPUT);
  pinMode(EARPHONE, INPUT);
  digitalWrite(MUTE, HIGH);
  earphone = true;
  maxgain = 0.3;
  step = 0.01;

  audioLogger = &Serial;
  file = new AudioFileSourceSD();   // Music source from SD file / 音乐来源为SD/TF卡文件
  out = new AudioOutputI2S();       // Use external I2S DAC / 使用外部DAC
  mod = new AudioGeneratorMOD();    // MOD parser / MOD文件解析器

/*
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  Wire.begin(SCREEN_SDA, SCREEN_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
*/
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // Initialize SSD1306 Display / 初始化SSD1306 OLED显示屏
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1); // Don't proceed, loop forever
  }

  drawUI();

  // Initialize SD / 初始化SD文件系统
  if (!SD.begin(5, SPI, 8000000)) {
    Serial.println("SD initialization failed!");
    display.setCursor(0, 12);
    display.println(F("SD INIT FAILED!"));
    display.display();
    while (1); // Don't proceed, loop forever
  }

  // Get total MOD file numbers / 获取MOD文件总数
  dir = SD.open("/"); 
  while(f = dir.openNextFile()){
    if (String(f.name()).endsWith(".mod"))
      totalfiles++;
  }
  //Serial.print("Total Files: ");
  //Serial.println(totalfiles);

  // MOD file settings / MOD文件设置
  mod->SetBufferSize(6*1024);
  mod->SetSampleRate(33000);
  mod->SetStereoSeparation(32);

  // Initialize key/button timestamp / 初始化按键时间戳
  t = micros();
  lasttime = t;
  lastkeytime = t;
}

// -- Part 4 -- Infinite Big Loop / 无限循环体
void loop()
{
  // 耳机侦测，低电平为耳机插入
  if (digitalRead(EARPHONE)!=earphone){   // 耳机插孔状态有变化
    earphone = !earphone;
    if (!earphone){   // 耳机插入，由于没有经过功放芯片将放大倍数调大，适应耳机的音量比例关系
      maxgain = 3;
      step = 0.1;
      gain = gain * 10;
    } else {          // 耳机拔出，由于扬声器经过功放芯片输出，因此缩小增益倍数，适应扬声器的音量比例
      maxgain = 0.3;
      step = 0.01;
      gain = gain / 10.0;
      if (gain > maxgain) gain = maxgain;
    }
    out->SetGain(gain);
    digitalWrite(MUTE, earphone); // 扬声器静音也是地电平有效，同步耳机侦测信号，插入耳机时扬声器静音，拔出耳机时使用扬声器播放
  }

  // 串口调试控制
  int cmd = 0;
  if (Serial.available() > 0) {
    // read the incoming byte:
    cmd = Serial.read();
    switch(cmd){
      case 'p': prev = true;  //前一首
                break;
      case 'n': next = true;  //下一首
                break;
      case '+': gain += step;  //音量加
                out->SetGain(gain);
                break;
      case '-': gain -= step; //音量减
                out->SetGain(gain);
                break;
      case 'j': mod->SetSkipJump(!mod->GetSkipJump());  //跳过Jump开关
                break;
    }
  } 

  // 旋转编码器处理
  byte a;
  a = digitalRead(SW_A);
  if (a != last_a){
    t = micros(); 
    last_a = a;
    if (t - lastkeytime > SW_GAP ){             // 防止信号抖动
      if (digitalRead(SW_B) != a) {             // 向左旋转
        if(submenu == 0){                       // 在根级控件控制中
          updateButton(-1);                     // 回到前一个按钮
        } else if (submenu == 1) {              // 如果在一级子控键控制中
          switch(cur_item) {
            case 4:                             // 当前激活控件编号4 - 音量控制
                    changeVolume(-step);        // 降低音量
                    break;
          }
        }
        //Serial.println(cur_item);
      } else {                                  // 向右旋转
        if(submenu == 0){                       // 在根级控件控制中
          updateButton(1);                      //  下一个按钮
        } else if (submenu == 1) {              // 如果在一级子控键控制中
          switch(cur_item) {
            case 4:                             // 当前激活控件编号4 - 音量控制
                    changeVolume(step);         // 提高音量
                    break;
          }
        }
        //Serial.println(cur_item);
      }
      lastkeytime = t;
    }
    // 修正延迟导致的波形匹配错误
    a = digitalRead(SW_A);
    last_a = a;
    ///////
  }

  // 按下旋转编码器，执行功能
  if (!digitalRead(SW_P)){
    t = micros();
    if (t - lastkeytime > SW_GAP * 2){  // 防止信号抖动
      if (submenu == 0){                // 在根级控件控制中按下按键
        switch (cur_item){
          case 0:
                  prev = true;
                  startPlay();          // 如果为停止播放状态，将状态修改为播放状态
                  break;
          case 1:
                  toggleStopStart();    // 停止播放或开始播放
                  break;
          case 2:
                  next = true;
                  startPlay();          // 如果为停止播放状态，将状态修改为播放状态
                  break;
          case 3:
                  toggleSkipJump();     // 切换强制跳过跳转功能
                  break;
          case 4:
                  submenu = 1;          // 进入下一级子控制
                  break;
        }
      } else if (submenu == 1) {        // 在一级子控制中
        switch (cur_item){
          case 4:                       // 当前激活控件编号4 - 音量控制 
                  submenu = 0;          // 按下按键完成音量设置，退回到根级控制
        }
      }
      lastkeytime = t;
    }
  } 

  // 音乐切换控制
  if ((mod) && mod->isRunning() && !(next || prev) ) { // 正在在播放音乐，且未按下上一首、下一首

    t = micros();

    if (!mod->loop()) mod->stop();
    if ((t - lasttime) > 100000 ) { //  定时更新
      updateProgress();             //  更新进度条
      lasttime = t;
    }
   } else if (menu[1] != 3) {       //  处于非停止播放状态（可能一首曲子播完了或者按下了上一首、下一首按钮），控件1的图标为“>”（编号3）时，为停止播放状态
    next = false;
    Serial.printf("MOD done\n");
    delay(1000);
    if (prev){                                          // 如果是按下了上一首的按钮
      unsigned int tgt_file, t;
      if (cur_file > 1)                                 // 目标曲目号为前一号，如果已是第一首曲子，则为最后一首曲子的编号
        tgt_file = cur_file - 1;
      else
        tgt_file = totalfiles;

      dir = SD.open("/");                               // 重新打开目录
      t = 0;
      while (t < tgt_file - 1) {                        // 从第一个文件开始遍历到前【两】首曲子
        f = dir.openNextFile();
        if (f)     
          if (String(f.name()).endsWith(".mod"))        // 只统计.mod结尾的文件（MOD文件）
            t++;
      }
      cur_file = tgt_file - 1;
      prev = false;
    }
    while (f = dir.openNextFile()){                     // 找到下一首MOD文件
      if (String(f.name()).endsWith(".mod")) break;     // （如果按下下一首的按钮，直接执行这段代码，找到下一首曲子；
    }                                                   // 如果是按了上一首按钮，前面遍历到前两首曲子，这里执行到下一首正好就是当前曲目的上一首）
    
    if (f) {      
      cur_file++;                                       // 当前曲目号加一
      openMusicFile();                                  // 打开并播放新的音乐文件
    } else {                                            // 没有下一个文件了
      cur_file = 0;                                     // 曲目号为0
      rewindFolder();                                   // 跳转到目录开始
    }       
  }
}