# RetroPlayer

Project Video on Youtube: https://www.youtube.com/watch?v=L54KPmg54jA

![image](https://github.com/user-attachments/assets/13f8eee9-93b4-4dea-92b4-2f1a606eab65)


RetroPlayer 项目旨在创造一个可以播放AMIGA计算机时代流行的MOD音乐格式的实体复古音乐播放器。该播放器基于ESP32实现，支持显示歌曲信息和播放控制，并通过旋转编码器进行操作。

## 功能

- **MOD音乐**：RetroPlayer可以播放存储在TF卡上的海量的AMIGA MOD音乐，MOD音乐在上世纪90年代非常流行，也是Demoscene文化中不可或缺的组成部分；
- **乐曲信息显示**： 可以通过OLED屏幕显示正在播放的乐曲的相关信息；
- **单一操控**：可以通过一个旋钮驱动旋转编码器方便地操作控制播放行为；
- **木制外壳**：虽然可以使用其它材质，但还是推荐使用实木制作，以便于得到温润厚实的手感。

## 从这里开始

### 前置准备

- 支持ESP32开发板的Arduino IDE环境
- 安装ESP8266Audio、Adafruit_SSD1306以及其依赖的函数库
- BOM表中涉及到的所有电子元器件
- 根据Gerber文件制作的PCB
- 一些板材，木材或亚克力板（2mm和1mm两种厚度规格，表层贴皮可以采用0.5mm厚度的木皮）
- 激光雕刻机或者其他能切割板材的装置（或者可以找代工商解决）
- 电烙铁和焊接耗材
- 灵巧的双手
- 耐心和面对挑战勇气

### 代码下载和说明

#### 代码下载
1. Clone the repository:
   ```sh
   git clone https://github.com/WellsWang/RetroPlayer.git
   cd RetroPlayer
   ```

 2. 直接下载zip文件，使用解压缩软件解开压缩包

#### 目录结构
- pcb  `电路设计相关`
	- BOM `物料表`
	- Gerber `PCB制作文件`
	- Schematics `电路原理图`
- enclosure `外壳设计相关`
	- 3D_Model `三维建模文件（效果验证）`
	- 2D_CAD `CAD文件，用于板材切割`
- src `源代码相关`
	- ESP8266Audio_mod_src `ESP8266Audio库中的部分需要修改定制的文件`
	- RetroPlayer25 `项目源代码`

#### 代码安装注意事项
由于ESP8266Audio库只提供了基本的文件解析和音频信号生成操作，一些我们所需要的功能并未实现，因此本项目修改了ESP8266Audio库中和MOD文件相关的部分代码并做了扩展。在编译前，需要将`src/ESP8266Audio_mod_src`目录中的文件复制到Arduino IDE使用的ESP8266Audio函数库目录中，替换掉源文件。在替换前建议对原文件进行备份。

在Windows环境下，Arduino IDE的函数库一般位于`C:\Users\用户名\Documents\Arduino\libraries`，而在Linux环境，一般位于`/home/用户名/Arduino/libraries`。

#### 电路设计安装说明
`Schematics` 目录中存放了电路原理设计图，如果您有新的想法或改进的设计，可以进行参考。

在电路图中，并未涉及锂电池充放电模块，可以使用小型的充放电一体电路模块，将5V电源输出接到H1，SW1接到开关。

<img src="https://github.com/user-attachments/assets/ea33d34e-b6a0-4b95-9776-0bf2c2519778" width="300px">


对于OLED屏幕，本项目中使用的是SSD1306驱动的1.3寸128x64 SPI接口的OLED屏幕。如果需要更换其他的OLED屏幕，例如使用SH1106驱动的屏幕，需要修改源代码，建议使用u8glib库并修改对应的驱动代码。另外因为性能的原因，不建议修改为IIC接口。如果需要修改OLED屏幕的尺寸，需要同步修改外壳设计中面板的开孔大小。

在本项目中扬声器使用了4040尺寸的，4欧3W腔体扬声器。注意接线时左右扬声器线序要一致。

<img src="https://github.com/user-attachments/assets/a03824c5-6c7d-4766-a69d-d90506599d5c" width="300px">


在电路图第三页（音频部分）中，R26和R27为两个0欧电阻，用于跳线选择。这两个电阻只需要焊接一个，建议选择只焊接R27。这种场景下，如有外接耳机、音箱插入3.5mmm音频插孔，播放器自身的扬声器将静音，只通过3.5mm音频接口输出。R26是用于测试的，如果只焊接R26，插入耳机时，扬声器依旧有声音输出，且会随着耳机插入改变声音强度。

![image](https://github.com/user-attachments/assets/8a2630a4-d780-42a9-a22b-ba3da890b81b)


#### 外壳设计组装说明
外壳可以使用木板或亚克力板来构建。可以使用`2D_CAD`目录中的dxf文件进行板材切割。

外壳板材除了绘制在最右侧的音箱网罩板和中央面板外，一般都使用2mm厚度的板材进行切割。

![image](https://github.com/user-attachments/assets/40b1082a-83a2-4e30-9b54-226513a91361)


音箱网罩板和中央面板使用1mm厚度的板材切割。

![image](https://github.com/user-attachments/assets/663cb054-4e8f-401d-977a-3616fc0fc5fa)


切割后的板材可以参考`3D_Model`目录中的三维建模来进行组装。该文件可以使用Sketchup软件打开。

如果想遮盖板材拼接的接口，可以使用`2D_CAD/2D加工图_贴皮.dxf`这个文件对表面贴皮进行切割。贴皮可以使用厚度低于0.5mm的材料，例如胡桃木皮。

音箱网罩板使用尼龙钉与播放器主体进行卡扣连接，推荐使用3.3*13黑色尼龙钉。音箱网罩板上的蒙布需要使用专用胶水粘贴。

<img src="https://github.com/user-attachments/assets/fc91b978-3c14-404b-8a8c-ebcc7308ee66" width="300px">


音箱铭牌为可选安装件。

## 未来计划与代码贡献
此项目为业余项目，虽然有很多想法，例如：
- 更完善的播放模式选择（随机，循环控制）
- 更多的歌曲信息显示（播放时长、当前音符信息）
- 更多的音乐格式（ESP8266Audio支持很多音乐格式）
- 互联网播放支持（可以直接播放modarchive.org网站的内容）
- ……还有好多想法……

但是，个人时间有限，所以，也许在未来的某一天会对此项目进行更新。

因此，非常欢迎大家fork此项目并对项目代码进行优化、改进或实现你自己的想法。
在此，先对大家的代码贡献做出感谢，谢谢大家的支持！

## 许可证
本项目基于GNU General Public License v3.0许可证发布。具体信息请参考LICENSE文件。
本项目及开发人员不对本项目中包含的所有信息、代码的质量、使用效果、带来的影响作出任何承诺，您需要自行进行判断并承担相应的责任。

## Acknowledgments
- Inspiration from the classic Amiga computers and their unique sound capabilities.
- ESP8266Audio library for MOD file handling. (https://github.com/earlephilhower/ESP8266Audio)
- Adafriut SSD1306 library for OLED display handling. (https://github.com/adafruit/Adafruit_SSD1306)


-------------------------------------------------------------------------------------------------------------------------------------


# RetroPlayer

![image](https://github.com/user-attachments/assets/13f8eee9-93b4-4dea-92b4-2f1a606eab65)

The RetroPlayer project aims to create a physical retro music player capable of playing the popular MOD music format from the AMIGA computer era. This player is based on the ESP32, supports song information display and playback control, and is operated using a rotary encoder.

## Features

- **MOD Music**: RetroPlayer can play a vast collection of AMIGA MOD music stored on a TF card. MOD music was highly popular in the 1990s and is an essential part of the Demoscene culture.
- **Track Information Display**: The OLED screen displays relevant information about the currently playing track.
- **Single Control Interface**: Playback can be conveniently controlled using a single knob-driven rotary encoder.
- **Wooden Enclosure**: While other materials can be used, solid wood is recommended for a warm and sturdy tactile feel.

## Getting Started

### Prerequisites

- Arduino IDE environment supporting ESP32 development boards
- Installed libraries: ESP8266Audio, Adafruit_SSD1306, and their dependencies
- All electronic components listed in the BOM
- PCBs manufactured according to the provided Gerber files
- Some sheet materials: wood or acrylic sheets (2mm and 1mm thickness, with 0.5mm veneer for surface finishing)
- A laser engraver or another cutting tool (or outsourcing to a professional service)
- A soldering iron and related materials
- Dexterous hands
- Patience and courage to face challenges

### Code Download and Instructions

#### Downloading the Code
1. Clone the repository:
   ```sh
   git clone https://github.com/WellsWang/RetroPlayer.git
   cd RetroPlayer
   ```
2. Alternatively, download the ZIP file and extract it using decompression software.

#### Directory Structure
- **pcb** (Circuit Design Related)
  - **BOM** (Bill of Materials)
  - **Gerber** (PCB Manufacturing Files)
  - **Schematics** (Circuit Schematics)
- **enclosure** (Enclosure Design Related)
  - **3D_Model** (3D modeling files for visualization)
  - **2D_CAD** (CAD files for material cutting)
- **src** (Source Code Related)
  - **ESP8266Audio_mod_src** (Modified files for the ESP8266Audio library)
  - **RetroPlayer25** (Project source code)

#### Code Installation Notes
The ESP8266Audio library only provides basic file parsing and audio signal generation. Some required functionalities were not implemented, so this project modifies and extends parts of the ESP8266Audio library related to MOD files. Before compiling, copy the files from `src/ESP8266Audio_mod_src` into the ESP8266Audio library directory used by the Arduino IDE, replacing the original files. It is recommended to back up the original files before replacing them.

In Windows, the Arduino IDE libraries are typically located at:
`C:\Users\YourUsername\Documents\Arduino\libraries`

In Linux, they are usually found at:
`/home/YourUsername/Arduino/libraries`

#### Circuit Design Installation Notes
The `Schematics` directory contains circuit schematic designs. If you have new ideas or improvements, feel free to reference them.

The circuit diagram does not include a lithium battery charging/discharging module. A small integrated charging circuit module can be used, connecting the 5V power output to H1 and SW1 to the switch.

<img src="https://github.com/user-attachments/assets/ea33d34e-b6a0-4b95-9776-0bf2c2519778" width="300px">


For the OLED screen, this project uses a 1.3-inch 128x64 SPI interface OLED display driven by SSD1306. If replacing it with another display, such as one driven by SH1106, modifications to the source code are required. The u8glib library is recommended for these changes. Due to performance reasons, using an I2C interface is not recommended. If changing the OLED screen size, adjust the enclosure design's panel cutout accordingly.

The speaker used in this project is a 4040-sized, 4-ohm 3W cavity speaker. Ensure that the wiring sequence for the left and right speakers is consistent.

<img src="https://github.com/user-attachments/assets/a03824c5-6c7d-4766-a69d-d90506599d5c" width="300px">

On the third page of the circuit diagram (audio section), R26 and R27 are two 0-ohm resistors used for jumper selection. Only one of these resistors should be soldered. It is recommended to solder only R27. In this configuration, when external headphones or speakers are connected to the 3.5mm audio jack, the player's built-in speaker will be muted, and audio output will be redirected through the 3.5mm jack. R26 is used for testing purposes; if only R26 is soldered, the speaker will continue to output sound even when headphones are inserted, and the volume will change accordingly.

![image](https://github.com/user-attachments/assets/8a2630a4-d780-42a9-a22b-ba3da890b81b)

#### Enclosure Design and Assembly Instructions
The enclosure can be made from wood or acrylic sheets. The DXF files in the `2D_CAD` directory can be used for material cutting.

Apart from the speaker grille panel and the central panel, most parts should be cut from 2mm thick material. 

![image](https://github.com/user-attachments/assets/40b1082a-83a2-4e30-9b54-226513a91361)

The speaker grille panel and the central panel should be cut from 1mm thick material.

![image](https://github.com/user-attachments/assets/663cb054-4e8f-401d-977a-3616fc0fc5fa)

After cutting, refer to the `3D_Model` directory for assembly guidance. These files can be opened using SketchUp.

To cover the joints of the assembled sheets, use the `2D_CAD/2D加工图_贴皮.dxf` file for veneer cutting. The veneer material should be less than 0.5mm thick, such as walnut veneer.

The speaker grille panel is attached to the player body using nylon pins. It is recommended to use 3.3*13mm black nylon pins. The fabric on the speaker grille should be glued with a specialized adhesive.

<img src="https://github.com/user-attachments/assets/fc91b978-3c14-404b-8a8c-ebcc7308ee66" width="300px">

The speaker nameplate is an optional component.

## Future Plans and Code Contributions
This is a hobby project with many ideas for future development, such as:
- Improved playback modes (shuffle, repeat control)
- More detailed song information display (play duration, current note data)
- Support for additional music formats (ESP8266Audio supports various formats)
- Internet streaming support (direct playback from modarchive.org)
- …and many more ideas…

However, due to limited personal time, updates to this project may happen sporadically.

Thus, contributions are highly encouraged! Feel free to fork this project, optimize the code, make improvements, or implement your own ideas. Thank you in advance for your contributions and support!

## License
This project is released under the GNU General Public License v3.0. For details, refer to the LICENSE file.

The project and its developers make no guarantees regarding the quality, usability, or impact of the provided information and code. Users must evaluate and assume responsibility for their usage.

## Acknowledgments
- Inspired by the classic Amiga computers and their unique sound capabilities.
- Thanks to the ESP8266Audio library for MOD file handling (https://github.com/earlephilhower/ESP8266Audio).
- Thanks to the Adafruit SSD1306 library for OLED display handling (https://github.com/adafruit/Adafruit_SSD1306).
