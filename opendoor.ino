#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <BLINKER_PMSX003ST.h>
#include <ESP8266WiFi.h>
#include <Servo.h>
#define BLINKER_WIFI         //����WiFi������
#define BLINKER_MIOT_OUTLET  //����С��ͬѧ�ĺ�����
#include <Blinker.h>
//��ʹ��EEPROMʱһ��Ҫע��Blinker����ռ����ǰ2000����ֽ�
#define CARD_SAVE_ADRESS 2500  //EEPROM�б���Ŀ�ƬID����ʼ��ַ
#define WIFIFLAG_ADRESS 4000   //EEPROM�б��������״̬����ʼ��ַ
#define WIFIFLAG 666
#define LED_BUILTIN 2

char auth[] = "xxxxxx";  //�����Կ
char ssid[] = "xxxxxx";  //WiFi����
char pswd[] = "xxxxxx";     //WiFi����

// �½��������
BlinkerButton Button1("btn-max-set");  //λ��1 ��ť ���ݼ��� ���ڿ��ƶ��ָ���ض��Ƕ�
BlinkerButton Button2("btn-max");      //λ��2 ��ť ���ݼ��� ���app���Ű�ť
BlinkerButton Button3("btn-read");     //λ��3 ��ť ���ݼ��� ���濨Ƭ��ť
BlinkerButton Button4("btn-eraser");   //λ��4 ��ť ���ݼ��� ������Ƭ��ť
BlinkerButton Button5("btn-reset");    //λ��5 ��ť ���ݼ��� ������Ƭ��ť
BlinkerSlider Slider1("ser-max-set");  //ʵʱλ�� ���� ���ݼ���  ��Χ1-180  ���ƶ��ָ��ĽǶ�
BlinkerSlider Slider2("ser-num2");     //���飬ָ��Ҫ������EEPROM
Servo myservo;                         //�����������
                                       //int ser_num ;
union int_byte {
  int int_data;
  byte byte_data[4];
};
int_byte ser_max;
int ser_max_temp;

#define RST_PIN 5  // �������
#define SS_PIN 4
MFRC522 mfrc522(SS_PIN, RST_PIN);  // �����µ�RFIDʵ��
//RCģ��ʹ����D1.D2.D5.D6.D7
/*************************IO����**************************/
//int Buzzer = 16;                   //D0(io16)����������ʾ����Ҳ������led
//int btn = 15;                      //D8(io15)��ť�������Ž���ID��EEPROM���Ұ����������õ��app��ť�ٿش濨
//����ģ��ʵ��������û���õ�
/*************************����**************************/
//ʹ��union�ṹ���ϲ�4��byte���ݣ�ת��Ϊ1��long������eeprom�洢
union long_byte {
  long long_data;
  byte byte_data[4];
};
long_byte lb;
long_byte wifi_status_temp;

long EEPROM_RFID1 = -1;  //EEPROM�б�����Ž���ID1
long EEPROM_RFID2 = -1;
long EEPROM_RFID3 = -1;
long EEPROM_RFID4 = -1;
long EEPROM_RFID5 = -1;
long EEPROM_RFID6 = -1;
long EEPROM_RFID7 = -1;
long read_RFID = -1;
long wifi_status;
int num_op = -1;  //����������Ŀ�Ƭ�ı�ʶ

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
    EEPROM.commit();  //ִ�б���EEPROM
  }
}

void miotPowerState(const String &state)  //��С��ͬѧ���ƣ���������һƪ�������Ļ������ûɶ�õĹ���
{
  BLINKER_LOG("need set power state: ", state);
  //��С��ͬѧ˵�����š�
  if (state == BLINKER_CMD_ON) {
    digitalWrite(LED_BUILTIN, HIGH);

    BlinkerMIOT.powerState("on");
    BlinkerMIOT.print();
    opendoor();
  }
}

void button1_callback(const String &state) {  //λ��1��ť ��ť���ƣ����Ǹ����������ָ��λ�õİ�ť������Ҳûɶ��
  ser_max.int_data = ser_max_temp;
  EEPROM.begin(4096);
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i + 3500, ser_max.byte_data[i]);
  }
  EEPROM.commit();
  delay(500);
  Serial.println("�����ѱ���");
  Blinker.print("���Ƕ�����Ϊ: ", ser_max_temp);
}

void button2_callback(const String &state) {  //λ��2��ť�����Ĺ��ܰ�ť��ͨ�����app�ٿ���������
  Blinker.print("���ڿ���...");
  opendoor();
  Blinker.delay(1000);
  Blinker.print("notice", "����");
  delay(300);
}
void button3_callback(const String &state)  //nfc��¼�밴ť���ҽ�ԭ���İ�ťʽ����������˳���������ʽ����ȥ�˰�ť
{
  EEPROM.begin(4096);  //�ƺ��Ǹ���EEPROMʱ��Ҫ�ĺ�������
  if (1)               //�����ᵽ��di������ԭ�������������������������Ҿ���̫����û�ӷ�����
  {
    delay(200);
    if (read_RFID == -1) {
      Blinker.print("��ǰδ����");
      Serial.println("��ǰδ����");
    }

    else {
      switch (num_op) {

        case 0:
          lb.long_data = read_RFID;
          EEPROM_RFID1 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //ִ�б���EEPROM
          Serial.println("�Ž���ID1�ѱ���");
          Blinker.print("�Ž���ID1�ѱ���");
          break;
        case 1:
          lb.long_data = read_RFID;
          EEPROM_RFID2 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 4 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //ִ�б���EEPROM
          Serial.println("�Ž���ID2�ѱ���");
          Blinker.print("�Ž���ID2�ѱ���");
          break;

        case 2:
          lb.long_data = read_RFID;
          EEPROM_RFID3 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 8 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //ִ�б���EEPROM
          Serial.println("�Ž���ID3�ѱ���");
          Blinker.print("�Ž���ID3�ѱ���");
          break;
        case 3:
          lb.long_data = read_RFID;
          EEPROM_RFID4 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 12 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //ִ�б���EEPROM
          Serial.println("�Ž���ID4�ѱ���");
          Blinker.print("�Ž���ID4�ѱ���");
          break;

        case 4:
          lb.long_data = read_RFID;
          EEPROM_RFID5 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 16 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //ִ�б���EEPROM
          Serial.println("�Ž���ID5�ѱ���");
          Blinker.print("�Ž���ID5�ѱ���");
          break;
        case 5:
          lb.long_data = read_RFID;
          EEPROM_RFID6 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 20 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //ִ�б���EEPROM
          Serial.println("�Ž���ID6�ѱ���");
          Blinker.print("�Ž���ID6�ѱ���");
          break;
        case 6:
          lb.long_data = read_RFID;
          EEPROM_RFID7 = lb.long_data;
          for (int i = 0; i < 4; i++) {
            EEPROM.write(i + 24 + CARD_SAVE_ADRESS, lb.byte_data[i]);
          }
          EEPROM.commit();  //ִ�б���EEPROM
          Serial.println("�Ž���ID7�ѱ���");
          Blinker.print("�Ž���ID7�ѱ���");
          break;
        default:
          break;
      }
    }
  }
}

void button5_callback(const String &state) {  //����
  Blinker.print("��������....");
  ESP.reset();  
}

void opendoor(void) {

  read_RFID = -1;
  int fromPos;
  int toPos;
  fromPos = myservo.read();
  toPos = 10;
  //��ʼ�����
  if (fromPos <= toPos) {  //�������ʼ�Ƕ�ֵ��С�ڡ�Ŀ��Ƕ�ֵ��
    for (int i = fromPos; i <= toPos; i++) {
      myservo.write(i);
      delay(5);
    }
  } else {  //������ʼ�Ƕ�ֵ�����ڡ�Ŀ��Ƕ�ֵ��
    for (int i = fromPos; i >= toPos; i--) {
      myservo.write(i);
      delay(5);
    }
  }
  if (wifi_status == WIFIFLAG)
    Blinker.vibrate();
  int i;
  for (i = 10; i <= ser_max.int_data; i = i + 5) {  //�ٿض������
    myservo.write(i);
  }
  //delay(1000);
  for (i = ser_max.int_data; i >= 10; i = i - 5) {
    myservo.write(i);
  }
  Serial.println("�����Ѵ�");
  if (wifi_status == WIFIFLAG){
    Blinker.print("�����Ѵ�");
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

/***********************RFID�����Զ��庯��***********************/
//��ʼ������
void RFID_init() {
  SPI.begin();         // SPI��ʼ
  mfrc522.PCD_Init();  // ��ʼ��zu
}
//�ѵ�ǰ��������ID���Ա�EEPROM�е�ID
void RFID_Handler(long data) {
  Serial.println(data);
  if (EEPROM_RFID1 == -1 && EEPROM_RFID2 == -1 && EEPROM_RFID3 == -1 && EEPROM_RFID4 == -1 && EEPROM_RFID5 == -1 && EEPROM_RFID6 == -1 && EEPROM_RFID7 == -1) {
    if (wifi_status == WIFIFLAG)
      Blinker.print("��ǰδ���ÿ�");
    Serial.println("��ǰδ���ÿ�");
  } else {
    if (data == EEPROM_RFID1) {
      Serial.println("ID1��ȷ����֤ͨ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID1��ȷ����֤ͨ�������ڿ���...");
      opendoor();  //���ź���
    } else if (data == EEPROM_RFID2) {
      Serial.println("ID2��ȷ����֤ͨ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID2��ȷ����֤ͨ�������ڿ���...");
      opendoor();
    }

    else if (data == EEPROM_RFID3) {
      Serial.println("ID3��ȷ����֤ͨ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID3��ȷ����֤ͨ�������ڿ���...");
      opendoor();
    }

    else if (data == EEPROM_RFID4) {
      Serial.println("ID4��ȷ����֤ͨ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID4��ȷ����֤ͨ�������ڿ���...");
      opendoor();
    } else if (data == EEPROM_RFID5) {
      Serial.println("ID5��ȷ����֤ͨ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID5��ȷ����֤ͨ�������ڿ���...");
      opendoor();
    }

    else if (data == EEPROM_RFID6) {
      Serial.println("ID6��ȷ����֤ͨ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID6��ȷ����֤ͨ�������ڿ���...");
      opendoor();
    }

    else if (data == EEPROM_RFID7) {
      Serial.println("ID7��ȷ����֤ͨ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID7��ȷ����֤ͨ�������ڿ���...");
      opendoor();
    } else {
      Serial.println("ID������֤ʧ��");
      if (wifi_status == WIFIFLAG)
        Blinker.print("ID������֤ʧ��");
    }
  }
}
//�û�IDת������
long RFID_toLong(byte *by) {
  long data;
  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = by[i];
  }
  data = lb.long_data;
  return data;
}

//���ж���
void RFID_read() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {

    return;
  } else {
    read_RFID = RFID_toLong(mfrc522.uid.uidByte);
    RFID_Handler(read_RFID);
    mfrc522.PICC_HaltA();       //ֹͣ PICC
    mfrc522.PCD_StopCrypto1();  //ֹͣ����PCD
    return;
  }
}



//ʵʱ��������
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

//����ָ����EEPROM
void button4_callback(const String &state)  //
{
  EEPROM.begin(4096);  //�ƺ��Ǹ���EEPROMʱ��Ҫ�ĺ�������
  if (1) {

    switch (num_op) {

      case 0:
        EEPROM_RFID1 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //ִ�б���EEPROM
        Serial.println("�Ž���ID1������");
        Blinker.print("�Ž���ID1������");
        break;
      case 1:
        EEPROM_RFID2 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 4 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //ִ�б���EEPROM
        Serial.println("�Ž���ID2������");
        Blinker.print("�Ž���ID2������");
        break;

      case 2:
        EEPROM_RFID3 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 8 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //ִ�б���EEPROM
        Serial.println("�Ž���ID3������");
        Blinker.print("�Ž���ID3������");
        break;
      case 3:
        EEPROM_RFID4 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 12 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //ִ�б���EEPROM
        Serial.println("�Ž���ID4������");
        Blinker.print("�Ž���ID4������");
        break;
      case 4:
        EEPROM_RFID5 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 16 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //ִ�б���EEPROM
        Serial.println("�Ž���ID5������");
        Blinker.print("�Ž���ID5������");
        break;
      case 5:
        EEPROM_RFID6 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 20 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //ִ�б���EEPROM
        Serial.println("�Ž���ID6������");
        Blinker.print("�Ž���ID6������");
        break;
      case 6:
        EEPROM_RFID7 = 0;
        for (int i = 0; i < 4; i++) {
          EEPROM.write(i + 24 + CARD_SAVE_ADRESS, 0);
        }
        EEPROM.commit();  //ִ�б���EEPROM
        Serial.println("�Ž���ID7������");
        Blinker.print("�Ž���ID7������");
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
  EEPROM.commit();  //ִ�б���EEPROM
  Serial.printf(" ����״̬�л������������� ");
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
    Serial.println(" ��ǰ������ģʽ����");
  else if (wifi_status == -WIFIFLAG)
    Serial.println(" ��ǰ�Զ���ģʽ����");

  if (wifi_status == WIFIFLAG) {
    BLINKER_DEBUG.stream(Serial);
    Blinker.begin(auth, ssid, pswd);  //���ɶ����޷����е�������������û��֤��
    Button1.attach(button1_callback);
    Button2.attach(button2_callback);
    Button3.attach(button3_callback);
    Button4.attach(button4_callback);
    Button5.attach(button5_callback);
    Slider1.attach(slider1_callback);
    Slider2.attach(slider2_callback);
    Blinker.attachData(dataRead);
    BlinkerMIOT.attachPowerState(miotPowerState);  //����ӳ��ΪС�����ܲ������͵����ܼҾӣ�ΪС��ʶ���ṩ����
    //ʵʱ��������
    Blinker.attachData(dataRead);
    Blinker.attachRTData(rtData);
  }
  myservo.attach(0);  //servo.attach():���ö����������
  myservo.write(10);  //servo.write():����ת���Ƕ�


  //ser_num = 100;//�����������ݶ��ָ���ض��Ƕȵı�������ʼΪ�κ�ֵ������
  //ser_max.int_data = 115;//�趨�������ʱ��ת�����Ƕȣ����λ�ô�ž�����������

  //��ȡEEPROM������ֵ            ���������������ſ�������Ҳ�����ټ�
  EEPROM.begin(4096);
  for (int i = 0; i < 4; i++) {
    ser_max.byte_data[i] = EEPROM.read(i + 3500);
  }
  ser_max_temp = ser_max.int_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + CARD_SAVE_ADRESS);  //��1
  }
  EEPROM_RFID1 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + CARD_SAVE_ADRESS + 4);  //��2
  }
  EEPROM_RFID2 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 8 + CARD_SAVE_ADRESS);  //��3
  }
  EEPROM_RFID3 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 12 + CARD_SAVE_ADRESS);  //��4
  }
  EEPROM_RFID4 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 16 + CARD_SAVE_ADRESS);  //��5
  }
  EEPROM_RFID5 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 20 + CARD_SAVE_ADRESS);  //��6
  }
  EEPROM_RFID6 = lb.long_data;

  for (int i = 0; i < 4; i++) {
    lb.byte_data[i] = EEPROM.read(i + 24 + CARD_SAVE_ADRESS);  //��7
  }
  EEPROM_RFID7 = lb.long_data;
  //IO_init();
  RFID_init();
  Serial.println("�����ɹ�");
}

void loop()  //����ѭ�����п�ʼ
{
  if ((WiFi.status() != WL_CONNECTED && WiFi.status() != 7) && wifi_status == WIFIFLAG)  //������
    Reset(0);
  if ((WiFi.status() == WL_CONNECTED || WiFi.status() == 7) && wifi_status == -WIFIFLAG)  //������
    Reset(1);


  myservo.write(10);

  if (wifi_status == WIFIFLAG && Blinker.connect()) {
    RFID_init();  //ÿ��ѭ������ʼ��һ�ζ������������Ļ��ᷢ��δ֪�Ŀ�����
    Blinker.run();
  } else {
    RFID_read();
    RFID_init();
  }

  RFID_read();  //����������������
}