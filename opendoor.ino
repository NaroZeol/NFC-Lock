#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <BLINKER_PMSX003ST.h>
#include <ESP8266WiFi.h>
#include <Servo.h>
#define BLINKER_WIFI         //启用WiFi函数库
#define BLINKER_MIOT_OUTLET  //启用小爱同学的函数库
#include <Blinker.h>
//在使用EEPROM时一定要注意Blinker服务占用了前2000多个字节
#define CARD_SAVE_ADRESS 2500  //EEPROM中保存的卡片ID的起始地址
#define WIFIFLAG_ADRESS 4000   //EEPROM中保存的网络状态的起始地址
#define WIFIFLAG 666
#define LED_BUILTIN 2

char auth[] = "xxxxxx";  //点灯密钥
char ssid[] = "xxxxxx";  //WiFi名称
char pswd[] = "xxxxxx";     //WiFi密码

// 新建组件对象
BlinkerButton Button1("btn-max-set");  //位置1 按钮 数据键名 用于控制舵机指向特定角度
BlinkerButton Button2("btn-max");      //位置2 按钮 数据键名 点灯app开门按钮
BlinkerButton Button3("btn-read");     //位置3 按钮 数据键名 储存卡片按钮
BlinkerButton Button4("btn-eraser");   //位置4 按钮 数据键名 擦除卡片按钮
BlinkerButton Button5("btn-reset");    //位置5 按钮 数据键名 擦除卡片按钮
BlinkerSlider Slider1("ser-max-set");  //实时位置 滑块 数据键名  范围1-180  控制舵机指向的角度
BlinkerSlider Slider2("ser-num2");     //滑块，指定要操作的EEPROM
Servo myservo;                         //创建电机对象
                                       //int ser_num ;
union int_byte {
  int int_data;
  byte byte_data[4];
};
int_byte ser_max;
int ser_max_temp;

#define RST_PIN 5  // 配置针脚
#define SS_PIN 4
MFRC522 mfrc522(SS_PIN, RST_PIN);  // 创建新的RFID实例
//RC模块使用了D1.D2.D5.D6.D7
/*************************IO配置**************************/
//int Buzzer = 16;                   //D0(io16)蜂鸣器，提示音，也可以用led
//int btn = 15;                      //D8(io15)按钮，保存门禁卡ID到EEPROM，我把它换成了用点灯app按钮操控存卡
//以上模块实际制作中没有用到
/*************************数据**************************/
//使用union结构，合并4个byte数据，转换为1个long，便于eeprom存储
union long_byte {
  long long_data;
  byte byte_data[4];
};
long_byte lb;
long_byte wifi_status_temp;

long EEPROM_RFID1 = -1;  //EEPROM中保存的门禁卡ID1
long EEPROM_RFID2 = -1;
long EEPROM_RFID3 = -1;
long EEPROM_RFID4 = -1;
long EEPROM_RFID5 = -1;
long EEPROM_RFID6 = -1;
long EEPROM_RFID7 = -1;
long read_RFID = -1;
long wifi_status;
int num_op = -1;  //定义待操作的卡片的标识

void WifiCheck() {
  EEPROM.begin(4096);
  for (int i = 0; i < 4; i++) {
    wifi_status_temp.byte_data[i] = EEPROM.read(i + WIFIFLAG_ADRESS);
  }
  wifi_status = wifi_status_temp.long_data;
  if (wifi_status_temp.long_data != WIFIFLAG && wifi_status_temp.long_data != -WIFIFLAG) {
    wifi_status_temp.long_data = WIFIFLAG;
    wifi_status = WIFIFLAG;
    for (int i = 0; i < 4; i++) {
      EEPROM.write(i + WIFIFLAG_ADRESS, wifi_status_temp.byte_data[i]);
    }
    EEPROM.commit();  //执行保存EEPROM
  }
}

void miotPowerState(const String &state)  //用小爱同学控制，整合了另一篇文章做的花里胡哨没啥用的功能
{
  BLINKER_LOG("need set power state: ", state);
  //对小爱同学说“开门”
  if (state == BLINKER_CMD_ON) {
    digitalWrite(LED_BUILTIN, HIGH);

    BlinkerMIOT.powerState("on");
    BlinkerMIOT.print();
    opendoor();
  }
}

void button1_callback(const String &state) {  //位置1按钮 按钮控制，这是个调整舵机到指定位置的按钮，但是也没啥用
  ser_max.int_data = ser_max_temp;
  EEPROM.begin(4096);
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i + 3500, ser_max.byte_data[i]);
  }
  EEPROM.commit();
  delay(500);
  Serial.println("设置已保存");
  Blinker.print("最大角度设置为: ", ser_max_temp);
}

void button2_callback(const String &state) {  //位置2按钮，核心功能按钮，通过点灯app操控门锁开启
  Blinker.print("正在开门...");
  opendoor();
  Blinker.delay(1000);
  Blinker.print("notice", "开门");
  delay(300);
}
void button3_callback(const String &state)  //nfc卡录入按钮，我将原本的按钮式触发储存改了成了这种形式，免去了按钮
{
  EEPROM.begin(4096);  //似乎是更改EEPROM时必要的函数调用
  if (1)               //下面提到的di函数是原本蜂鸣器的启动函数，但是我觉得太吵就没加蜂鸣器
  {
    delay(200);
    if (read_RFID == -1) {
      Blinker.print("当前未读卡");
      Serial.println("当前未读卡");
    }

    else {
      switch (num_op) {

        case 0:
          lb.long_data = read_RFID;
          EEPROM_RFID1 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //执行保存EEPROM
          Serial.println("门禁卡ID1已保存");
          Blinker.print("门禁卡ID1已保存");
          break;
        case 1:
          lb.long_data = read_RFID;
          EEPROM_RFID2 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 4 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //执行保存EEPROM
          Serial.println("门禁卡ID2已保存");
          Blinker.print("门禁卡ID2已保存");
          break;

        case 2:
          lb.long_data = read_RFID;
          EEPROM_RFID3 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 8 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //执行保存EEPROM
          Serial.println("门禁卡ID3已保存");
          Blinker.print("门禁卡ID3已保存");
          break;
        case 3:
          lb.long_data = read_RFID;
          EEPROM_RFID4 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 12 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //执行保存EEPROM
          Serial.println("门禁卡ID4已保存");
          Blinker.print("门禁卡ID4已保存");
          break;

        case 4:
          lb.long_data = read_RFID;
          EEPROM_RFID5 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 16 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //执行保存EEPROM
          Serial.println("门禁卡ID5已保存");
          Blinker.print("门禁卡ID5已保存");
          break;
        case 5:
          lb.long_data = read_RFID;
          EEPROM_RFID6 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 20 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //执行保存EEPROM
          Serial.println("门禁卡ID6已保存");
          Blinker.print("门禁卡ID6已保存");
          break;
        case 6:
          lb.long_data = read_RFID;
          EEPROM_RFID7 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 24 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //执行保存EEPROM
          Serial.println("门禁卡ID7已保存");
          Blinker.print("门禁卡ID7已保存");
          break;
        default:
          break;
      }
    }
  }
}

void button5_callback(const String &state) {  //重启
  Blinker.print("正在重启....");
  ESP.reset();  
}

void opendoor(void) {

  read_RFID = -1;
  int fromPos;
  int toPos;
  fromPos = myservo.read();
  toPos = 10;
  //初始化舵机
  if (fromPos <= toPos) {  //如果“起始角度值”小于“目标角度值”
    for (int i = fromPos; i <= toPos; i++) {
      myservo.write(i);
      delay(5);
    }
  } else {  //否则“起始角度值”大于“目标角度值”
    for (int i = fromPos; i >= toPos; i--) {
      myservo.write(i);
      delay(5);
    }
  }
  if (wifi_status == WIFIFLAG)
    Blinker.vibrate();
  int i;
  for (i = 10; i <= ser_max.int_data; i = i + 5) {  //操控舵机开门
    myservo.write(i);
  }
  //delay(1000);
  for (i = ser_max.int_data; i >= 10; i = i - 5) {
    myservo.write(i);
  }
  Serial.println("门锁已打开");
  if (wifi_status == WIFIFLAG){
    Blinker.print("门锁已打开");
  }
  delay(1000);
}



void slider1_callback(int32_t value) {
  ser_max_temp = value;
  Blinker.delay(100);
  Blinker.print("max set to : ", value);
}


void slider2_callback(int32_t value) {
  num_op = value - 1;

  Blinker.delay(100);
}

void dataRead(const String &data) {
  BLINKER_LOG("Blinker readString: ", data);

  Blinker.vibrate();

  uint32_t BlinkerTime = millis();
  Blinker.print(BlinkerTime);
  Blinker.print("millis", BlinkerTime);
}

/***********************RFID读卡自定义函数***********************/
//初始化读卡
void RFID_init() {
  SPI.begin();         // SPI开始
  mfrc522.PCD_Init();  // 初始化zu
}
//把当前读到卡的ID，对比EEPROM中的ID
void RFID_Handler(long data) {
  Serial.println(data);
  if (EEPROM_RFID1 == -1 && EEPROM_RFID2 == -1 && EEPROM_RFID3 == -1 && EEPROM_RFID4 == -1 && EEPROM_RFID5 == -1 && EEPROM_RFID6 == -1 && EEPROM_RFID7 == -1) {
    if (wifi_status == WIFIFLAG)
      Blinker.print("当前未设置卡");
    Serial.println("当前未设置卡");
  } else {
    if (data == EEPROM_RFID1) {
      Serial.println("ID1正确，验证通过");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID1正确，验证通过，正在开门...");
      opendoor();  //开门函数
    } else if (data == EEPROM_RFID2) {
      Serial.println("ID2正确，验证通过");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID2正确，验证通过，正在开门...");
      opendoor();
    }

    else if (data == EEPROM_RFID3) {
      Serial.println("ID3正确，验证通过");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID3正确，验证通过，正在开门...");
      opendoor();
    }

    else if (data == EEPROM_RFID4) {
      Serial.println("ID4正确，验证通过");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID4正确，验证通过，正在开门...");
      opendoor();
    } else if (data == EEPROM_RFID5) {
      Serial.println("ID5正确，验证通过");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID5正确，验证通过，正在开门...");
      opendoor();
    }

    else if (data == EEPROM_RFID6) {
      Serial.println("ID6正确，验证通过");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID6正确，验证通过，正在开门...");
      opendoor();
    }

    else if (data == EEPROM_RFID7) {
      Serial.println("ID7正确，验证通过");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID7正确，验证通过，正在开门...");
      opendoor();
    } else {
      Serial.println("ID错误，验证失败");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID错误，验证失败");
    }
  }
}
//用户ID转换类型
long RFID_toLong(byte *by) {
  long data;
  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = by[i];
  }
  data = lb.long_data;
  return data;
}

//运行读卡
void RFID_read() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {

    return;
  } else {
    read_RFID = RFID_toLong(mfrc522.uid.uidByte);
    RFID_Handler(read_RFID);
    mfrc522.PICC_HaltA();       //停止 PICC
    mfrc522.PCD_StopCrypto1();  //停止加密PCD
    return;
  }
}



//实时发送数据
void rtData() {
  Blinker.sendRtData("data1", EEPROM_RFID1);
  Blinker.sendRtData("data2", EEPROM_RFID2);
  Blinker.sendRtData("data3", EEPROM_RFID3);
  Blinker.sendRtData("data4", EEPROM_RFID4);
  Blinker.sendRtData("data5", EEPROM_RFID5);
  Blinker.sendRtData("data6", EEPROM_RFID6);
  Blinker.sendRtData("data7", EEPROM_RFID7);
  //Blinker.printRtData();
}

//擦除指定的EEPROM
void button4_callback(const String &state)  //
{
  EEPROM.begin(4096);  //似乎是更改EEPROM时必要的函数调用
  if (1) {

    switch (num_op) {

      case 0:
        EEPROM_RFID1 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //执行保存EEPROM
        Serial.println("门禁卡ID1已重置");
        Blinker.print("门禁卡ID1已重置");
        break;
      case 1:
        EEPROM_RFID2 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 4 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //执行保存EEPROM
        Serial.println("门禁卡ID2已重置");
        Blinker.print("门禁卡ID2已重置");
        break;

      case 2:
        EEPROM_RFID3 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 8 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //执行保存EEPROM
        Serial.println("门禁卡ID3已重置");
        Blinker.print("门禁卡ID3已重置");
        break;
      case 3:
        EEPROM_RFID4 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 12 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //执行保存EEPROM
        Serial.println("门禁卡ID4已重置");
        Blinker.print("门禁卡ID4已重置");
        break;
      case 4:
        EEPROM_RFID5 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 16 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //执行保存EEPROM
        Serial.println("门禁卡ID5已重置");
        Blinker.print("门禁卡ID5已重置");
        break;
      case 5:
        EEPROM_RFID6 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 20 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //执行保存EEPROM
        Serial.println("门禁卡ID6已重置");
        Blinker.print("门禁卡ID6已重置");
        break;
      case 6:
        EEPROM_RFID7 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 24 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //执行保存EEPROM
        Serial.println("门禁卡ID7已重置");
        Blinker.print("门禁卡ID7已重置");
        break;
      default:
        break;
    }
  }
}

void Reset(int choice) {
  if (choice == 0)
    wifi_status_temp.long_data = -WIFIFLAG;
  if (choice == 1)
    wifi_status_temp.long_data = WIFIFLAG;
  EEPROM.begin(4096);
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i + WIFIFLAG_ADRESS, wifi_status_temp.byte_data[i]);
  }
  EEPROM.commit();  //执行保存EEPROM
  Serial.printf(" 网络状态切换，正在重启中 ");
  ESP.reset();
}

void setup() {
  Serial.begin(115200);
  Serial.println("");
  WiFi.begin(ssid, pswd);
  long count = 100;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    if (count--)
      continue;
    else
      break;
  }
  WifiCheck();

  if (wifi_status == WIFIFLAG)
    Serial.println(" 当前以联网模式启动");
  else if (wifi_status == -WIFIFLAG)
    Serial.println(" 当前以断网模式启动");

  if (wifi_status == WIFIFLAG) {
    BLINKER_DEBUG.stream(Serial);
    Blinker.begin(auth, ssid, pswd);  //怀疑断网无法运行的问题出在这里，但没有证据
    Button1.attach(button1_callback);
    Button2.attach(button2_callback);
    Button3.attach(button3_callback);
    Button4.attach(button4_callback);
    Button5.attach(button5_callback);
    Slider1.attach(slider1_callback);
    Slider2.attach(slider2_callback);
    Blinker.attachData(dataRead);
    BlinkerMIOT.attachPowerState(miotPowerState);  //声明映射为小米智能插座类型的智能家居，为小爱识别提供帮助
    //实时发送数据
    Blinker.attachData(dataRead);
    Blinker.attachRTData(rtData);
  }
  myservo.attach(0);  //servo.attach():设置舵机数据引脚
  myservo.write(10);  //servo.write():设置转动角度


  //ser_num = 100;//这是用来操纵舵机指向特定角度的变量，初始为任何值都可以
  //ser_max.int_data = 115;//设定舵机开门时旋转的最大角度，这个位置大概就能拉开门锁

  //读取EEPROM索引的值            我这里设置了七张卡，不够也可以再加
  EEPROM.begin(4096);
  for (int i = 0; i < 4; i++) {
    ser_max.byte_data[i] = EEPROM.read(i + 3500);
  }
  ser_max_temp = ser_max.int_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + CARD_SAVE_ADRESS);  //卡1
  }
  EEPROM_RFID1 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + CARD_SAVE_ADRESS + 4);  //卡2
  }
  EEPROM_RFID2 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 8 + CARD_SAVE_ADRESS);  //卡3
  }
  EEPROM_RFID3 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 12 + CARD_SAVE_ADRESS);  //卡4
  }
  EEPROM_RFID4 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 16 + CARD_SAVE_ADRESS);  //卡5
  }
  EEPROM_RFID5 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 20 + CARD_SAVE_ADRESS);  //卡6
  }
  EEPROM_RFID6 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 24 + CARD_SAVE_ADRESS);  //卡7
  }
  EEPROM_RFID7 = lb.long_data;
  //IO_init();
  RFID_init();
  Serial.println("启动成功");
}

void loop()  //最后的循环运行开始
{
  if ((WiFi.status() != WL_CONNECTED && WiFi.status() != 7) && wifi_status == WIFIFLAG)  //断网了
    Reset(0);
  if ((WiFi.status() == WL_CONNECTED || WiFi.status() == 7) && wifi_status == -WIFIFLAG)  //来网了
    Reset(1);


  myservo.write(10);

  if (wifi_status == WIFIFLAG && Blinker.connect()) {
    RFID_init();  //每次循环都初始化一次读卡，不这样的话会发生未知的卡死。
    Blinker.run();
  } else {
    RFID_read();
    RFID_init();
  }

  RFID_read();  //读卡并做分析处理
}