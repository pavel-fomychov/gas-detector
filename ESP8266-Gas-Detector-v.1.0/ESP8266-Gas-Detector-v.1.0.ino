/*******************************************************************

    Gas detector with Telegram alerts v.1.0
    https://github.com/pavel-fomychov/gas-detector
    
    Autor: Pavel Fomychov
    YouTube: https://www.youtube.com/c/PavelFomychov
    Instagram: https://www.instagram.com/pavel.fomychov/
    Telegram: https://t.me/PavelFomychov
    
 *******************************************************************/
 
#include <Wire.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BME280.h>
#include <OneButton.h>

#define WIFI_SSID "********"      // Имя Wi-Fi сети
#define WIFI_PASSWORD "*******"   // Пароль Wi-Fi сети
#define BOT_TOKEN ""              // Токен Telegram Bot
#define CHAT_ID ""                // CHAT_ID в Telegram

#define pinBTN_1 3         // (Rx) Левая кнопка
#define pinBTN_2 0         // (D3) Центральная кнопка
#define pinBTN_3 1         // (Tx) Правая кнопка
#define pinLED_R 16        // (D0) Вывод красного светодиода
#define pinLED_G 13        // (D7) Вывод зеленого светодиода
#define pinLED_B 2         // (D4) Вывод синего светодиода
#define pinRelay 15        // (D8) Сигнальный вывод реле
#define pinBuzzer 14       // (D6) Сигнальный вывод пищалки
#define pinBacklight 12    // (D5) Вывод подсветки дисплея
#define pinMQ A0           //  MQ Датчик
#define timeUpadate 500    //  Период опроса датчиков в мс

const unsigned long BOT_MTBS = 2000;  // период проверки сообщений в Telegram (мс)
boolean LEDpinOn = 0;  // Светодиод с общим катодом - 1; Светодиод с общим анодом - 0;

int screen = 1;
int16_t MQvalue = 0;
int16_t MQalarm = 5000;
int16_t lastValueMQ = 0;
int16_t backlightLevel = 14;
int16_t relayMode = 1;

float Temperature = 0;
float Humidity = 0;
float Pressure = 0;
float lastValueTemperature = 0;
float lastValueHumidity = 0;
float lastValuePressure = 0;

long lastTime = 0;
long lastTimeAll = 0;
long lastTimeRelay = 0;
long lastTimeScroll = 0;
long lastTimeTelegram = 0;
unsigned long bot_lasttime;

boolean alarmFlag = true;
boolean warningFlag = true;
boolean normalFlag = false;
boolean telegramFlag = false;
boolean relayFlag = true;
boolean resetFlag = false;
boolean autoScrollFlag = false;

int16_t eepromMQ = 0;
int16_t eepromBacklight = 0;
int16_t eepromRelay = 0;

String inlineKey;
const String inlineKeyStatus = "[{ \"text\" : \"   Получить значения датчиков   \", \"callback_data\" : \"/status\" }]";
const String inlineKeyRelay1sec = "[{ \"text\" : \" Закрыть клапан \", \"callback_data\" : \"/relon\" }]";
const String inlineKeyRelayOn = "[{ \"text\" : \" Включить реле \", \"callback_data\" : \"/relon\" }]";
const String inlineKeyRelayOff = "[{ \"text\" : \" Выключить реле \", \"callback_data\" : \"/reloff\" }]";

byte black[8] = {0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111};
byte degree[8] = {0B11100, 0B10100, 0B11100, 0B00000, 0B00000, 0B00000, 0B00000, 0B00000};

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_BME280 bme;
OneButton button1(pinBTN_1, true);
OneButton button2(pinBTN_2, true);
OneButton button3(pinBTN_3, true);


void setup() {
  Wire.begin();
  Serial.begin(115200);
  pinMode(pinMQ, INPUT);
  pinMode(pinBTN_1, INPUT_PULLUP);
  pinMode(pinBTN_2, INPUT_PULLUP);
  pinMode(pinBTN_3, INPUT_PULLUP);
  pinMode(pinLED_R, OUTPUT);
  pinMode(pinLED_G, OUTPUT);
  pinMode(pinLED_B, OUTPUT);
  pinMode(pinBacklight, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinRelay, OUTPUT);

  EEPROM.begin(10);
  EEPROM_read(0, eepromMQ);
  EEPROM_read(2, eepromRelay);
  EEPROM_read(4, eepromBacklight);
  EEPROM.end();

  if (eepromMQ > 0 && eepromMQ < 10000) MQalarm = eepromMQ;
  if (eepromRelay == 0 || eepromRelay == 1) relayMode = eepromRelay;
  if (eepromBacklight >= 0 && eepromBacklight <= 16) backlightLevel = eepromBacklight;
  analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 1024));

  digitalWrite(pinBuzzer, LOW);
  digitalWrite(pinRelay, LOW);
  LEDoff();

  button1.attachClick(click1);
  button2.attachClick(click2);
  button3.attachClick(click3);
  button1.attachLongPressStart(longPressStart1);
  button2.attachLongPressStart(longPressStart2);
  button3.attachLongPressStart(longPressStart3);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(1, 0);
  lcd.print("Pavel Fomychov");
  lcd.setCursor(2, 1);
  lcd.print("Technologies");
  lcd.createChar(0, black);
  lcd.createChar(1, degree);
  delay(1500);
  blueLED();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_SSID);

  configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    lcd.print(".");
    delay(500);
  }

  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    now = time(nullptr);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected");
  lcd.setCursor(0, 1);
  lcd.print("IP:");
  lcd.print(WiFi.localIP());
  greenLED();

  inlineButtonsMain();
  if (CHAT_ID > 0) bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Детектор газа включен", "", inlineKey);
}

void loop() {
  Button();
  Sensor();
  Display();
}

void Button() {
  button1.tick();
  button2.tick();
  button3.tick();
}

void Sensor() {
  if (millis() - timeUpadate > lastTime) {
    bme.begin(0x76);
    Temperature = bme.readTemperature();
    Humidity = bme.readHumidity();
    Pressure = bme.readPressure() / 100.0F;
    MQvalue = map(analogRead(pinMQ), 30, 1024, 0, 10000);
    lastTime = millis();
  }
}

void Display() {
  if (screen >= 10) {
    switch (screen) {
      case 10: SettingMQvalues(); break;
      case 11: SettingRelayMode(); break;
      case 12: SettingBacklightLevel(); break;
    }
  } else if (MQvalue >= MQalarm) {
    Alarm();
  } else if (MQvalue < 40) {
    Warning();
  } else {
    Normal();
    switch (screen) {
      case 1: DisplayGasLevel(); break;
      case 2: DisplayTemperature(); break;
      case 3: DisplayHumidity(); break;
      case 4: DisplayPressure(); break;
      case 5: DisplayAllParametrs(); break;
    }
    Telegram();
  }
}

void Alarm() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("High Gas level!");
  lcd.setCursor(1, 1);
  lcd.print(MQvalue);
  lcd.print("ppm");
  lcd.setCursor(12, 1);
  lcd.print(MQalarm);
  redLED();
  digitalWrite(pinBacklight, HIGH);
  digitalWrite(pinBuzzer, HIGH);
  if (relayMode == 0) digitalWrite(pinRelay, HIGH);
  if (alarmFlag) {
    lastTimeRelay = millis();
    digitalWrite(pinRelay, HIGH);
    bot.sendMessage(CHAT_ID, "<b>Внимание:</b>\nЗафиксировано превышение допустимого уровня газа", "html");
    if (relayMode == 1) {
      while (1) {
        if (millis() - lastTimeRelay > 1000) break;
        delay(10);
      }
      digitalWrite(pinRelay, LOW);
    }
    alarmFlag = false;
    normalFlag = true;
  } else {
    delay(500);
  }
  digitalWrite(pinBacklight, LOW);
  digitalWrite(pinBuzzer, LOW);
  delay(100);
}

void Warning() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Error");
  lcd.setCursor(0, 1);
  lcd.print("Check Gas Sensor");
  yellowLED();
  digitalWrite(pinBacklight, HIGH);
  if (warningFlag) {
    bot.sendMessage(CHAT_ID, "<b>Ошибка:</b>\nДатчик газа неисправен", "html");
    warningFlag = false;
  } else {
    delay(1000);
  }
  digitalWrite(pinBacklight, LOW);
  delay(100);
}

void Normal() {
  analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 1024));
  if (relayFlag) digitalWrite(pinRelay, LOW);
  digitalWrite(pinBuzzer, LOW);
  greenLED();
  if (normalFlag) {
    inlineButtonsMain();
    bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Уровень газа в норме", "", inlineKey);
    normalFlag = false;
  }
  alarmFlag = true;
  warningFlag = true;
  autoScroll();
  if (millis() > lastTimeTelegram) telegramFlag = true;
  if (resetFlag) ESP.reset();
}

void DisplayGasLevel() {
  if (abs(MQvalue - lastValueMQ) > 30) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gas level:");
    lcd.setCursor(1, 1);
    lcd.print(MQvalue);
    lcd.setCursor(12, 1);
    lcd.print(MQalarm);
    lastValueMQ = MQvalue;
  }
}

void DisplayTemperature() {
  if (Temperature != lastValueTemperature) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temperature:");
    lcd.setCursor(1, 1);
    lcd.print(Temperature, 1);
    lcd.write(byte(1));
    lcd.print("C");
    lastValueTemperature = Temperature;
  }
}

void DisplayHumidity() {
  if (Humidity != lastValueHumidity) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Humidity:");
    lcd.setCursor(1, 1);
    lcd.print(Humidity, 1);
    lcd.print("%");
    lastValueHumidity = Humidity;
  }
}

void DisplayPressure() {
  if (Pressure != lastValuePressure) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pressure:");
    lcd.setCursor(1, 1);
    lcd.print(Pressure * 0.750062, 0);  // 0.750062 - коефициент для перевода в мм рт.ст.
    lcd.print("mmHg");
    lastValuePressure = Pressure;
  }
}

void DisplayAllParametrs() {
  if (millis() - timeUpadate > lastTimeAll) {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(Temperature, 1);
    lcd.write(byte(1));

    lcd.setCursor(8, 0);
    lcd.print("H:");
    lcd.print(Humidity, 1);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(Pressure * 0.750062, 0);
    lcd.print("mm");

    lcd.setCursor(8, 1);
    lcd.print("G:");
    lcd.print(MQvalue);

    lastTimeAll = millis();
  }
}

void Telegram() {
  if (millis() - bot_lasttime > BOT_MTBS && telegramFlag) {
    /*
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Check Telegram");
      lcd.setCursor(1, 1);
      lcd.print("Messages ");
    */
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;
    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Гость";

    if (chat_id == CHAT_ID) {
      boolean flagCommand = true;

      if (text == "/status") {
        flagCommand = false;
        String statusMessage = "<b>Текущие значения датчиков:</b>\nУровень газа: ";
        statusMessage += MQvalue;
        statusMessage += "\nТемпература: ";
        statusMessage += Temperature;
        statusMessage += "°С\n";
        statusMessage += "Влажность: ";
        statusMessage += Humidity;
        statusMessage += "%\n";
        statusMessage += "Давление: ";
        statusMessage += abs(Pressure * 0.750062);
        statusMessage += "мм рт.ст.\n";
        inlineButtonsMain();
        bot.sendMessageWithInlineKeyboard(CHAT_ID, statusMessage, "html", inlineKey);
      }

      if (text == "/relon") {
        flagCommand = false;
        relayFlag = false;
        if (relayMode == 1) {
          digitalWrite(pinRelay, HIGH);
          delay(relayMode * 1000);
          digitalWrite(pinRelay, LOW);
          bot.sendMessage(CHAT_ID, "✔️ Клапан закрыт", "");
        } else {
          digitalWrite(pinRelay, HIGH);
          inlineButtonsRelay();
          bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Реле включено", "", inlineKey);
        }
      }

      if (text == "/reloff") {
        flagCommand = false;
        relayFlag = true;
        digitalWrite(pinRelay, LOW);
        inlineButtonsRelay();
        bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Реле выключено", "", inlineKey);
      }

      if (text == "/chatid") {
        flagCommand = false;
        bot.sendMessage(chat_id, "CHAT_ID: " + chat_id, "");
      }

      if (text == "/red") {
        flagCommand = false;
        redLED();
        bot.sendMessage(chat_id, "✔️ Команда \"/red\" выполнена", "");
        LEDoff();
      }

      if (text == "/blue") {
        flagCommand = false;
        blueLED();
        bot.sendMessage(chat_id, "✔️ Команда \"/blue\" выполнена", "");
        LEDoff();
      }

      if (text == "/bip") {
        flagCommand = false;
        digitalWrite(pinBuzzer, HIGH);
        bot.sendMessage(chat_id, "✔️ Команда \"/bip\" выполнена", "html");
        digitalWrite(pinBuzzer, LOW);
      }

      if (text == "/reset") {
        flagCommand = false;
        bot.sendMessage(chat_id, "✔️ Перезагрузка запущена", "");
        resetFlag = true;
      }

      if (text == "/start" || flagCommand) {
        if (text != "/start") bot.sendMessage(chat_id, "Команда <b>\"" + text + "\"</b> не найдена", "html");
        String startMessage = "<b>Список доступных команд:</b>\n";
        startMessage += "/status : Значения датчиков\n";
        if (relayMode == 1) {
          startMessage += "/relon : Закрыть клапан\n";
        } else {
          startMessage += "/relon : Включить реле\n";
          startMessage += "/reloff : Выключить реле\n";
        }
        startMessage += "/red : Моргнуть карсным цветом\n";
        startMessage += "/blue : Моргнуть синим цветом\n";
        startMessage += "/bip : Короткий сигнал зуммера\n";
        startMessage += "/chatid : Узнать CHAT_ID\n";
        startMessage += "/reset : Выполнить перезагрузку\n";
        bot.sendMessage(chat_id, startMessage, "html");
      }

    } else {
      if (text == "/start") {
        String startMessage = "Здравствуй, " + from_name + ".\n";
        startMessage += "Твой <b>CHAT_ID</b>: " + chat_id + "\n";
        bot.sendMessage(chat_id, startMessage, "html");
      }
    }

  }
}

void stopTelegram() {
  lastTimeTelegram = millis() + 10000;
  telegramFlag = false;
}

void inlineButtonsMain() {
  if (relayMode == 1) {
    inlineKey = "[" + inlineKeyStatus + "," + inlineKeyRelay1sec + "]";
  } else {
    if (digitalRead(pinRelay) == HIGH)
      inlineKey = "[" + inlineKeyStatus + "," + inlineKeyRelayOff + "]";
    else
      inlineKey = "[" + inlineKeyStatus + "," + inlineKeyRelayOn + "]";
  }
}

void inlineButtonsRelay() {
  if (relayMode == 1) {
    inlineKey = "[" + inlineKeyStatus + "]";
  } else {
    if (digitalRead(pinRelay) == HIGH)
      inlineKey = "[" + inlineKeyRelayOff + "]";
    else
      inlineKey = "[" + inlineKeyRelayOn + "]";
  }
}

void SettingMQvalues() {
  lcd.setCursor(0, 0);
  lcd.print("Gas Alarm value:");
  lcd.setCursor(0, 1);
  lcd.print(" <");
  lcd.print(MQalarm);
  lcd.print(">          ");
}

void SettingRelayMode() {
  lcd.setCursor(0, 0);
  lcd.print("Set Relay Mode: ");
  lcd.setCursor(0, 1);
  lcd.print(" <");
  if (relayMode == 1) {
    lcd.print(" 1 second ");
  } else {
    lcd.print("all period");
  }
  lcd.print(">      ");
}

void SettingBacklightLevel() {
  lcd.setCursor(0, 0);
  lcd.print("Backlight Level:");
  lcd.setCursor(0, 1);
  lcd.print("<");
  for (int i = 0; i < 15; i++) {
    lcd.setCursor(i + 1, 1);
    if (i < backlightLevel) lcd.write(byte(0));
    else lcd.print(" ");
  }
  lcd.setCursor(15, 1);
  lcd.print(">");
}

void click1() {
  stopTelegram();
  if (screen == 10) {
    MQalarm = MQalarm + 100;
    if (MQalarm > 9000) MQalarm = 9000;
    eepromMQ = MQalarm;
  } else if (screen == 11) {
    if (relayMode == 1) relayMode = 0; else relayMode = 1;
    eepromRelay = relayMode;
  } else if (screen == 12) {
    backlightLevel++;
    if (backlightLevel > 14) backlightLevel = 14;
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 1024));
    eepromBacklight = backlightLevel;
  } else {
    if (autoScrollFlag) {
      autoScrollOff();
    } else {
      screen++;
      checkScreen();
    }
  }
}

void click2() {
  stopTelegram();
  if (screen >= 10) {
    screen++;
    if (screen > 12) screen = 1;
    EEPROM.begin(10);
    EEPROM_write(0, eepromMQ);
    EEPROM_write(2, eepromRelay);
    EEPROM_write(4, eepromBacklight);
    EEPROM.end();
    lastValueMQ = 0;
    lastTime = 0;
  } else {
    screen = 5;
    lastTimeAll = 0;
    autoScrollFlag = false;
  }
}

void click3() {
  stopTelegram();
  if (screen == 10) {
    MQalarm = MQalarm - 100;
    if (MQalarm < 1000) MQalarm = 1000;
    eepromMQ = MQalarm;
  } else if (screen == 11) {
    if (relayMode == 1) relayMode = 0; else relayMode = 1;
    eepromRelay = relayMode;
  } else if (screen == 12) {
    backlightLevel--;
    if (backlightLevel < 0) backlightLevel = 0;
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 1024));
    eepromBacklight = backlightLevel;
  } else {
    if (autoScrollFlag) {
      autoScrollOff();
    } else {
      screen--;
      checkScreen();
    }
  }
}

void longPressStart1() {
  stopTelegram();
  if (screen == 10) {
    MQalarm = 9000;
    eepromMQ = MQalarm;
  } else if (screen == 11) {
    if (relayMode == 1) relayMode = 0; else relayMode = 1;
    eepromRelay = relayMode;
  } else if (screen == 12) {
    backlightLevel = 16;
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 1024));
    eepromBacklight = backlightLevel;
  } else {
    autoScrollOn();
  }
}

void longPressStart2() {
  stopTelegram();
  analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 1024));
  blueLED();
  screen = 10;
  lastValueMQ = 0;
  lastTime = 0;
}

void longPressStart3() {
  stopTelegram();
  if (screen == 10) {
    MQalarm = 1000;
    eepromMQ = MQalarm;
  } else if (screen == 11) {
    if (relayMode == 1) relayMode = 0; else relayMode = 1;
    eepromRelay = relayMode;
  } else if (screen == 12) {
    backlightLevel = 0;
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 1024));
    eepromBacklight = backlightLevel;
  } else {
    autoScrollOn();
  }
}

void checkScreen() {
  if (screen < 1) screen = 4;
  if (screen > 4) screen = 1;
  lastValueTemperature = 0;
  lastValueHumidity = 0;
  lastValuePressure = 0;
  lastValueMQ = 0;
  lastTime = 0;
}

void autoScroll() {
  if (autoScrollFlag && millis() - 2000 > lastTimeScroll) {
    screen++;
    if (screen > 4) screen = 1;
    lastTimeScroll = millis();
  }
}

void autoScrollOn() {
  autoScrollFlag = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scroll Mode On");
  delay(500);
}

void autoScrollOff() {
  autoScrollFlag = false;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scroll Mode Off");
  delay(500);
}

void redLED() {
  digitalWrite(pinLED_R, LEDpinOn);
  digitalWrite(pinLED_G, !LEDpinOn);
  digitalWrite(pinLED_B, !LEDpinOn);
}

void greenLED() {
  digitalWrite(pinLED_R, !LEDpinOn);
  digitalWrite(pinLED_G, LEDpinOn);
  digitalWrite(pinLED_B, !LEDpinOn);
}

void blueLED() {
  digitalWrite(pinLED_R, !LEDpinOn);
  digitalWrite(pinLED_G, !LEDpinOn);
  digitalWrite(pinLED_B, LEDpinOn);
}

void yellowLED() {
  digitalWrite(pinLED_R, LEDpinOn);
  digitalWrite(pinLED_G, LEDpinOn);
  digitalWrite(pinLED_B, !LEDpinOn);
}

void LEDoff() {
  digitalWrite(pinLED_R, !LEDpinOn);
  digitalWrite(pinLED_G, !LEDpinOn);
  digitalWrite(pinLED_B, !LEDpinOn);
}

template <class T> int EEPROM_write(int ee, const T& value) {
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  return i;
}

template <class T> int EEPROM_read(int ee, T& value) {
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}
