/*******************************************************************

    Gas detector with Telegram alerts v.1.1
    https://github.com/pavel-fomychov/gas-detector

    Autor: Pavel Fomychov
    YouTube: https://www.youtube.com/c/PavelFomychov
    Instagram: https://www.instagram.com/pavel.fomychov/
    Telegram: https://t.me/PavelFomychov

*******************************************************************/

#include <Wire.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SGP30.h>
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
#define timeUpadate 300    //  Период опроса датчиков в мс

const unsigned long BOT_MTBS = 2000;  // период проверки сообщений в Telegram (мс)
boolean LEDpinOn = 0;  // Светодиод с общим катодом - 1; Светодиод с общим анодом - 0;

int screen = 1;
uint16_t MQvalue = 0;
uint16_t MQalarm = 5000;
uint16_t backlightLevel = 14;
uint16_t relayMode = 0;
uint16_t displayRSSI = 0;

uint16_t eepromMQ = 0;
uint16_t eepromBacklight = 0;
uint16_t eepromRelay = 0;
uint16_t eepromRSSI = 0;

long rssi = 0;
long lastValueRSSI = 0;
String ValueMQ = "0";
String Temperature = "0";
String Humidity = "0";
String Pressure = "0";
String CarbonDioxide = "0";
String OrganicCompound = "0";
String lastValueMQ = "0";
String lastValueTemperature = "";
String lastValueHumidity = "";
String lastValuePressure = "";
String lastValueCarbonDioxide = "";
String lastValueOrganicCompound = "";

unsigned long lastTime = 0;
unsigned long lastTimeAllParam = 0;
unsigned long lastTimeRelay = 0;
unsigned long lastTimeScroll = 0;
unsigned long lastTimeTelegram = 0;
unsigned long lastTimeBot = 0;
unsigned long lastTimeScrollMode = 0;

boolean startFlag = false;
boolean alarmFlag = true;
boolean warningFlag = true;
boolean normalFlag = false;
boolean telegramFlag = false;
boolean telegramProcessFlag = false;
boolean relayFlag = true;
boolean resetFlag = false;
boolean autoScrollFlag = false;
boolean allParamFlag = false;

String inlineKey;
const String inlineKeyStart     = "[{ \"text\" : \"Список доступных команд\", \"callback_data\" : \"/start\" }]";
const String inlineKeyStatus    = "[{ \"text\" : \"Запросить статус\", \"callback_data\" : \"/status\" }]";
const String inlineKeySensor    = "[{ \"text\" : \"Получить значения датчиков\", \"callback_data\" : \"/sensor\" }]";
const String inlineKeyRelay1sec = "[{ \"text\" : \"Закрыть клапан\", \"callback_data\" : \"/relon\" }]";
const String inlineKeyRelayOn   = "[{ \"text\" : \"Включить реле\", \"callback_data\" : \"/relon\" }]";
const String inlineKeyRelayOff  = "[{ \"text\" : \"Выключить реле\", \"callback_data\" : \"/reloff\" }]";
const String inlineKeyRestart   = "[{ \"text\" : \"Выполнить перезагрузку\", \"callback_data\" : \"/reset\" }]";

byte black[8]            = {0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111};
byte degree[8]           = {0B11100, 0B10100, 0B11100, 0B00000, 0B00000, 0B00000, 0B00000, 0B00000};
byte antenna[8]          = {0B00000, 0B10101, 0B10101, 0B01110, 0B00100, 0B00100, 0B00100, 0B00000};
byte signalLevel_1[8]    = {0B00000, 0B00000, 0B00000, 0B00000, 0B00000, 0B10000, 0B10000, 0B00000};
byte signalLevel_2[8]    = {0B00000, 0B00000, 0B00000, 0B00100, 0B00100, 0B10100, 0B10100, 0B00000};
byte signalLevel_3[8]    = {0B00000, 0B00001, 0B00001, 0B00101, 0B00101, 0B10101, 0B10101, 0B00000};
byte NoSignal[8]         = {0B00000, 0B00000, 0B00000, 0B00000, 0B10100, 0B01000, 0B10100, 0B00000};
byte CheckMesTelegram[8] = {0B00000, 0B01000, 0B10100, 0B11100, 0B10111, 0B00010, 0B00010, 0B00000};
byte NewMesTelegram[8]   = {0B00000, 0B01000, 0B10100, 0B11100, 0B10101, 0B00001, 0B00001, 0B00000};
byte twoNumber[8]        = {0B00000, 0B00000, 0B01100, 0B10010, 0B00010, 0B00100, 0B01000, 0B11110};

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_BME280 bme;
Adafruit_SGP30 sgp;
OneButton button1(pinBTN_1, true);
OneButton button2(pinBTN_2, true);
OneButton button3(pinBTN_3, true);

ICACHE_RAM_ATTR void checkBTN_1() {
  if (startFlag && telegramProcessFlag) click1();
}
ICACHE_RAM_ATTR void checkBTN_2() {
  if (startFlag && telegramProcessFlag) click2();
}
ICACHE_RAM_ATTR void checkBTN_3() {
  if (startFlag && telegramProcessFlag) click3();
}

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

  digitalWrite(pinBuzzer, LOW);
  digitalWrite(pinRelay, LOW);
  LEDoff();
  sgp.begin();

  EEPROM.begin(10);
  EEPROM_read(0, eepromMQ);
  EEPROM_read(2, eepromRelay);
  EEPROM_read(4, eepromBacklight);
  EEPROM_read(6, eepromRSSI);
  EEPROM.end();

  if (eepromMQ > 0 && eepromMQ < 10000) MQalarm = eepromMQ;
  if (eepromRelay == 0 || eepromRelay == 1) relayMode = eepromRelay;
  if (eepromRSSI == 0 || eepromRSSI == 1) displayRSSI = eepromRSSI;
  if (eepromBacklight >= 0 && eepromBacklight <= 16) backlightLevel = eepromBacklight;
  analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 255));

  button1.attachClick(click1);
  button2.attachClick(click2);
  button3.attachClick(click3);
  button1.attachLongPressStart(longPressStart1);
  button2.attachLongPressStart(longPressStart2);
  button3.attachLongPressStart(longPressStart3);

  attachInterrupt(digitalPinToInterrupt(pinBTN_1), checkBTN_1, FALLING);
  attachInterrupt(digitalPinToInterrupt(pinBTN_2), checkBTN_2, FALLING);
  attachInterrupt(digitalPinToInterrupt(pinBTN_3), checkBTN_3, FALLING);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(1, 0);
  lcd.print("Pavel Fomychov");
  lcd.setCursor(2, 1);
  lcd.print("Technologies");
  lcd.createChar(0, black);
  lcd.createChar(1, degree);
  lcd.createChar(2, antenna);
  lcd.createChar(3, signalLevel_3);
  lcd.createChar(4, CheckMesTelegram);
  lcd.createChar(5, NewMesTelegram);
  lcd.createChar(6, twoNumber);
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
  if (CHAT_ID > 0) bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Детектор газа включен\n", "", inlineKey);
  startFlag = true;
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
    if (bme.begin(0x76)) {
      Serial.print("Temperature "); Serial.print(bme.readTemperature()); Serial.print(" C");
      Serial.print("Humidity "); Serial.print(bme.readHumidity()); Serial.print(" %");
      Serial.print("Pressure "); Serial.print((bme.readPressure() / 100.0) * 0.750062); Serial.println(" mm");
      Temperature = String(bme.readTemperature(), 1);
      Humidity = String(bme.readHumidity(), 1);
      Pressure = String((bme.readPressure() / 100.0) * 0.750062, 1);   // 0.750062 - коефициент для перевода в мм рт.ст.
    } else {
      Temperature = "0";
      Humidity = "0";
      Pressure = "0";
    }
    if (sgp.IAQmeasureRaw()) {
      if (Humidity != "0") sgp.setHumidity(getAbsoluteHumidity(Temperature.toFloat(), Humidity.toFloat()));
      sgp.IAQmeasure();
      OrganicCompound = String(sgp.TVOC);
      CarbonDioxide = String(sgp.eCO2);
    } else {
      sgp.begin();
      CarbonDioxide = "0";
    }
    MQvalue = map(analogRead(pinMQ), 0, 1024, 0, 10000);       // перевод значени в диапазо от 0 до 10000
    ValueMQ = String(MQvalue);
    rssi = WiFi.RSSI();
    lastTime = millis();
  }
}

void Display() {
  if (screen >= 10) {
    Setting();
  } else if (MQvalue > MQalarm) {
    Alarm();
  } else if (MQvalue < 40) {
    Warning();
  } else {
    Normal();
    Telegram();
  }
}

void Setting() {
  switch (screen) {
    case 10: SettingMQvalues(); break;
    case 11: SettingRelayMode(); break;
    case 12: SettingBacklightLevel(); break;
    case 13: SettingDisplayRSSI(); break;
  }
}

void Alarm() {
  screen = 1;
  lastTime = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("High Gas level! ");
  lcd.setCursor(0, 1);
  lcd.print(" ");
  lcd.print(MQvalue);
  lcd.print("           ");
  lcd.setCursor(12, 1);
  lcd.print(MQalarm);
  redLED();
  digitalWrite(pinBacklight, HIGH);
  digitalWrite(pinBuzzer, HIGH);
  if (relayMode == 0) digitalWrite(pinRelay, HIGH);
  if (alarmFlag) {
    lastTimeRelay = millis();
    digitalWrite(pinRelay, HIGH);
    bot.sendMessage(CHAT_ID, "Внимание:\nЗафиксировано превышение допустимого уровня газа\n", "");
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
  screen = 1;
  lastTime = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Error          ");
  lcd.setCursor(0, 1);
  lcd.print("Check Gas Sensor");
  yellowLED();
  digitalWrite(pinBacklight, HIGH);
  if (warningFlag) {
    bot.sendMessage(CHAT_ID, "Ошибка:\nДатчик газа неисправен\n", "");
    warningFlag = false;
  } else {
    delay(500);
  }
  digitalWrite(pinBacklight, LOW);
  delay(100);
}

void Normal() {
  analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 255));
  if (relayFlag) digitalWrite(pinRelay, LOW);
  digitalWrite(pinBuzzer, LOW);
  greenLED();
  alarmFlag = true;
  warningFlag = true;
  allParamFlag = false;
  autoScroll();
  if (millis() > lastTimeScrollMode) {
    switch (screen) {
      case 1: DisplayGasLevel(); break;
      case 2: DisplayTemperature(); break;
      case 3: DisplayHumidity(); break;
      case 4: DisplayPressure(); break;
      case 5: DisplayCarbonDioxide(); break;
      case 6: DisplayAllParametrs(); break;
    }
  }
  if (normalFlag) {
    inlineButtonsMain();
    bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Уровень газа в норме\n", "", inlineKey);
    normalFlag = false;
  }
  if (millis() > lastTimeTelegram) telegramFlag = true;
  if (resetFlag) ESP.reset();
}

void DisplayGasLevel() {
  if (ValueMQ != lastValueMQ || rssi != lastValueRSSI) {
    lcd.setCursor(0, 0);
    lcd.print("Gas level:    ");
    DisplaySignalLevel();
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.print(MQvalue);
    lcd.print("               ");
    if (displayRSSI == 1) {
      DisplaySignalValue();
    } else {
      lcd.setCursor(12, 1);
      lcd.print(MQalarm);
    }
    lastValueMQ = ValueMQ;
  }
}

void DisplayTemperature() {
  if (Temperature != lastValueTemperature || rssi != lastValueRSSI) {
    lcd.setCursor(0, 0);
    lcd.print("Temperature:  ");
    DisplaySignalLevel();
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.print(Temperature);
    lcd.write(byte(1));
    lcd.print("C           ");
    DisplaySignalValue();
    lastValueTemperature = Temperature;
  }
}

void DisplayHumidity() {
  if (Humidity != lastValueHumidity || rssi != lastValueRSSI) {
    lcd.setCursor(0, 0);
    lcd.print("Humidity:     ");
    DisplaySignalLevel();
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.print(Humidity);
    lcd.print("%           ");
    DisplaySignalValue();
    lastValueHumidity = Humidity;
  }
}

void DisplayPressure() {
  if (Pressure != lastValuePressure || rssi != lastValueRSSI) {
    lcd.setCursor(0, 0);
    lcd.print("Pressure:     ");
    DisplaySignalLevel();
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.print(Pressure);
    lcd.print("mm          ");
    DisplaySignalValue();
    lastValuePressure = Pressure;
  }
}

void DisplayCarbonDioxide() {
  if (CarbonDioxide != lastValueCarbonDioxide || rssi != lastValueRSSI) {
    lcd.setCursor(0, 0);
    lcd.print("CO");
    lcd.write(byte(6));
    lcd.print(" level:      ");
    DisplaySignalLevel();
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.print(CarbonDioxide);
    lcd.print("ppm          ");
    DisplaySignalValue();
    lastValueCarbonDioxide = CarbonDioxide;
  }
}

void DisplayAllParametrs() {
  allParamFlag = true;
  if (millis() - timeUpadate > lastTimeAllParam) {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(Temperature);
    lcd.write(byte(1));
    lcd.print("    ");

    lcd.setCursor(8, 0);
    lcd.print("H:");
    lcd.print(Humidity);
    lcd.print("%    ");

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(Pressure.toInt());
    lcd.print("mm   ");

    if (CarbonDioxide != "0") {
      lcd.setCursor(8, 1);
      lcd.print("C:");
      lcd.print(CarbonDioxide);
      if (CarbonDioxide.toInt() < 1000) lcd.print("ppm  ");
      else lcd.print("  ");
    } else {
      lcd.setCursor(8, 1);
      lcd.print("G:");
      lcd.print(MQvalue);
      lcd.print("     ");
    }
    lastTimeAllParam = millis();
  }
}

void DisplaySignalLevel() {
  lcd.setCursor(14, 0);
  lcd.write(byte(2));
  if (rssi > -57 && rssi < 10) {
    lcd.createChar(3, signalLevel_3);
  } else if (rssi <= -57 && rssi > -70) {
    lcd.createChar(3, signalLevel_2);
  } else if (rssi <= -70 && rssi > -92) {
    lcd.createChar(3, signalLevel_1);
  } else {
    lcd.createChar(3, NoSignal);
  }
  lcd.setCursor(15, 0);
  lcd.write(byte(3));
  lastValueRSSI = rssi;
}

void DisplaySignalValue() {
  if (displayRSSI == 1) {
    lcd.setCursor(11, 1);
    if (rssi < 0 && rssi > -92) {
      lcd.print(rssi);
      lcd.print("dB");
    } else {
      lcd.print("NoSig");
    }
  }
}

void Telegram() {
  if (millis() - lastTimeBot > BOT_MTBS && telegramFlag && rssi < 0 && rssi > -92) {
    telegramProcessFlag = true;
    iconCheckMessage();
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      iconNewMessage();
      handleNewMessages(numNewMessages);
      iconCheckMessage();
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      noIcon();
    }
    noIcon();
    lastTimeBot = millis();
    telegramProcessFlag = false;
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
        uint32_t secondsFromStart = millis() / 1000;
        uint32_t hours = secondsFromStart / 60 / 60;
        uint32_t minutes = secondsFromStart / 60 - hours * 60;
        uint32_t seconds = secondsFromStart - hours * 60 * 60 - minutes * 60;
        String timeFromStart = TimeFormat(String(hours)) + ":" + TimeFormat(String(minutes)) + ":" + TimeFormat(String(seconds));
        String MQalarmStr = String(MQalarm);
        String rssiStr = String(rssi);
        String statusMessage = "Уровень газа: " + ValueMQ + "\nПороговое значение: " + MQalarmStr + "\n";
        statusMessage += "Уровень сигнала Wi-Fi: " + String(rssi) + "dB  \nВремя работы: " + timeFromStart + "\n";
        inlineButtonsStatus();
        bot.sendMessageWithInlineKeyboard(CHAT_ID, statusMessage, "", inlineKey);
      }

      if (text == "/sensor") {
        flagCommand = false;
        if (Pressure != "0" || CarbonDioxide != "0") {
          String sensorMessage;
          if (Pressure != "0") sensorMessage += "Температура: " + Temperature + "°С\nВлажность: " + Humidity + "%\nДавление: " + Pressure + "мм рт.ст.        \n";
          if (CarbonDioxide != "0") sensorMessage += "Углекислый газ: " + CarbonDioxide + "ppm \n";
          inlineButtonsSensor();
          bot.sendMessageWithInlineKeyboard(CHAT_ID, sensorMessage, "", inlineKey);
        } else {
          bot.sendMessage(CHAT_ID, "Ошибка:\nПроверьте исправность датчиков\n", "");
        }
      }

      if (text == "/relon") {
        flagCommand = false;
        relayFlag = false;
        if (relayMode == 1) {
          digitalWrite(pinRelay, HIGH);
          delay(relayMode * 1000);
          digitalWrite(pinRelay, LOW);
          bot.sendMessage(CHAT_ID, "✔️ Клапан закрыт\n", "");
        } else {
          digitalWrite(pinRelay, HIGH);
          inlineButtonsRelay();
          bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Реле включено\n", "", inlineKey);
        }
      }

      if (text == "/reloff") {
        flagCommand = false;
        relayFlag = true;
        digitalWrite(pinRelay, LOW);
        inlineButtonsRelay();
        bot.sendMessageWithInlineKeyboard(CHAT_ID, "✔️ Реле выключено\n", "", inlineKey);
      }

      if (text == "/chatid") {
        flagCommand = false;
        bot.sendMessage(chat_id, "CHAT_ID: " + chat_id + "\n", "");
      }

      if (text == "/bip") {
        flagCommand = false;
        digitalWrite(pinBuzzer, HIGH);
        bot.sendMessage(chat_id, "✔️ Команда \"/bip\" выполнена\n", "html");
        digitalWrite(pinBuzzer, LOW);
      }

      if (text == "/reset") {
        flagCommand = false;
        bot.sendMessage(chat_id, "✔️ Перезагрузка запущена\n", "");
        resetFlag = true;
      }

      if (text == "/start" || flagCommand) {
        if (text != "/start") bot.sendMessage(chat_id, "Команда <b>\"" + text + "\"</b> не найдена", "html");
        String startMessage = "Список доступных команд:\n";
        startMessage += "/status : Запросить статус\n";
        if (Pressure != "0" || CarbonDioxide != "0") {
          startMessage += "/sensor : Значения датчиков\n";
        }
        if (relayMode == 1) {
          startMessage += "/relon : Закрыть клапан\n";
        } else {
          startMessage += "/relon : Включить реле\n";
          startMessage += "/reloff : Выключить реле\n";
        }
        startMessage += "/bip : Короткий сигнал зуммера\n";
        startMessage += "/reset : Выполнить перезагрузку\n";
        bot.sendMessage(chat_id, startMessage, "");
      }

    } else {
      if (text == "/start") {
        String startMessage = "Здравствуй, " + from_name + ".\n";
        startMessage += "Твой CHAT_ID: " + chat_id + "\n";
        bot.sendMessage(chat_id, startMessage, "");
      }
    }

  }
}

void stopTelegram() {
  lastTimeTelegram = millis() + 5000;
  telegramFlag = false;
}

void inlineButtonsMain() {
  if (bme.begin(0x76) || sgp.begin()) {
    inlineKey = "[" + inlineKeyStatus + "," + inlineKeySensor + "]";
  } else {
    if (relayMode == 1) {
      inlineKey = "[" + inlineKeyStatus + "," + inlineKeyRelay1sec + "]";
    } else {
      if (digitalRead(pinRelay) == HIGH)
        inlineKey = "[" + inlineKeyStatus + "," + inlineKeyRelayOff + "]";
      else
        inlineKey = "[" + inlineKeyStatus + "," + inlineKeyRelayOn + "]";
    }
  }
}

void inlineButtonsSensor() {
  if (relayMode == 1) {
    inlineKey = "["  + inlineKeySensor + "," + inlineKeyRelay1sec + "]";
  } else {
    if (digitalRead(pinRelay) == HIGH)
      inlineKey = "[" + inlineKeySensor + "," + inlineKeyRelayOff + "]";
    else
      inlineKey = "[" + inlineKeySensor + "," + inlineKeyRelayOn + "]";
  }
}

void inlineButtonsStatus() {
  inlineKey = "[" + inlineKeyStatus + "," + inlineKeyRestart + "]";
}

void inlineButtonsRelay() {
  if (digitalRead(pinRelay) == HIGH)
    inlineKey = "[" + inlineKeyRelayOff + "]";
  else
    inlineKey = "[" + inlineKeyRelayOn + "]";
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
    lcd.print("1 second");
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

void SettingDisplayRSSI() {
  lcd.setCursor(0, 0);
  lcd.print("Display RSSI:   ");
  lcd.setCursor(0, 1);
  lcd.print(" <");
  if (displayRSSI == 1) {
    lcd.print("on");
  } else {
    lcd.print("off");
  }
  lcd.print(">           ");
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
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 255));
    eepromBacklight = backlightLevel;
  } else if (screen == 13) {
    if (displayRSSI == 1) displayRSSI = 0; else displayRSSI = 1;
    eepromRSSI = displayRSSI;
  } else {
    if (autoScrollFlag) {
      autoScrollOff();
    } else {
      screen++;
      checkScreen();
    }
    Normal();
  }

  if (screen >= 10) Setting();
}

void click2() {
  stopTelegram();
  if (screen >= 10) {
    screen++;
    if (screen > 13) screen = 1;
    EEPROM.begin(10);
    EEPROM_write(0, eepromMQ);
    EEPROM_write(2, eepromRelay);
    EEPROM_write(4, eepromBacklight);
    EEPROM_write(6, eepromRSSI);
    EEPROM.end();
    lastValueMQ = "";
    lastTime = 0;
  } else {
    screen = 6;
    lastTimeAllParam = 0;
    autoScrollFlag = false;
    Normal();
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
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 255));
    eepromBacklight = backlightLevel;
  } else if (screen == 13) {
    if (displayRSSI == 1) displayRSSI = 0; else displayRSSI = 1;
    eepromRSSI = displayRSSI;
  } else {
    if (autoScrollFlag) {
      autoScrollOff();
    } else {
      screen--;
      checkScreen();
    }
    Normal();
  }

  if (screen >= 10) Setting();
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
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 255));
    eepromBacklight = backlightLevel;
  } else if (screen == 13) {
    if (displayRSSI == 1) displayRSSI = 0; else displayRSSI = 1;
    eepromRSSI = displayRSSI;
  } else {
    if (!telegramProcessFlag && (Pressure != "0" || CarbonDioxide != "0")) autoScrollOn();
    Normal();
  }

  if (screen >= 10) Setting();
}

void longPressStart2() {
  stopTelegram();
  analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 255));
  blueLED();
  screen = 10;
  lastValueMQ = "";
  lastTime = 0;
  Setting();
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
    analogWrite(pinBacklight, map(backlightLevel, 0, 14, 0, 255));
    eepromBacklight = backlightLevel;
  } else if (screen == 13) {
    if (displayRSSI == 1) displayRSSI = 0; else displayRSSI = 1;
    eepromRSSI = displayRSSI;
  } else {
    if (!telegramProcessFlag && (Pressure != "0" || CarbonDioxide != "0")) autoScrollOn();
    Normal();
  }

  if (screen >= 10) Setting();
}

void checkScreen() {
  if (Pressure == "0" && CarbonDioxide != "0") {
    if (screen < 1 || screen == 2) screen = 5;
    if (screen > 5 || screen == 4) screen = 1;
  } else if (Pressure != "0" && CarbonDioxide == "0") {
    if (screen < 1) screen = 4;
    if (screen > 4) screen = 1;
  } else if (Pressure == "0" && CarbonDioxide == "0") {
    screen = 1;
  } else {
    if (screen < 1) screen = 5;
    if (screen > 5) screen = 1;
  }
  lastValueTemperature = "";
  lastValueHumidity = "";
  lastValuePressure = "";
  lastValueCarbonDioxide = "";
  lastValueMQ = "";
  lastTime = 0;
}

void autoScroll() {
  if (autoScrollFlag && millis() - 2000 > lastTimeScroll) {
    screen++;
    checkScreen();
    lastTimeScroll = millis();
  }
}

void autoScrollOn() {
  stopTelegram();
  autoScrollFlag = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scroll Mode On");
  lastTimeScrollMode = millis() + 500;
  checkScreen();
}

void autoScrollOff() {
  stopTelegram();
  autoScrollFlag = false;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scroll Mode Off");
  lastTimeScrollMode = millis() + 500;
  checkScreen();
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

void iconCheckMessage() {
  if (!allParamFlag) {
    lcd.setCursor(13, 0);
    lcd.write(byte(4));
  }
}

void iconNewMessage() {
  if (!allParamFlag) {
    lcd.setCursor(13, 0);
    lcd.write(byte(5));
  }
}

void noIcon() {
  if (!allParamFlag) {
    lcd.setCursor(13, 0);
    lcd.print(" ");
  }
}

String TimeFormat (String str) {
  if (str.length() == 1)
    return "0" + str;
  if (str.length() == 0)
    return "00" + str;

  return str;
}

uint32_t getAbsoluteHumidity(float Temperature, float Humidity) {
  const float absoluteHumidity = 216.7f * ((Humidity / 100.0f) * 6.112f * exp((17.62f * Temperature) / (243.12f + Temperature)) / (273.15f + Temperature));
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity);
  return absoluteHumidityScaled;
}

template <class T> int EEPROM_write(int ee, const T & value) {
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  return i;
}

template <class T> int EEPROM_read(int ee, T & value) {
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}
