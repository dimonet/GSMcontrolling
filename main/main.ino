/// GSM устройство для контроля за пожилыми людьми или за помещением
/// ВНИМАНИЕ: для корретной работы sms необходимо установить размеры буферов вместо 64 на SERIAL_TX_BUFFER_SIZE 24 и SERIAL_RX_BUFFER_SIZE 170 в файле hardware\arduino\avr\cores\arduino\HardwareSerial.h

#include <EEPROM.h>
#include "MyGSM.h"
#include <avr/pgmspace.h>

#define debug Serial

//// НАСТРОЕЧНЫЕ КОНСТАНТЫ /////
const char sms_PIR1[]          PROGMEM = {"ALARM: PIR1 sensor."};                                 // текст смс для датчика движения 1

const char sms_ErrorCommand[]    PROGMEM = {"Available commands:\nSendSMS,\nBalance,\nTest on/off,\nRedirect on/off,\nControl on/off,\nSkimpy,\nReboot,\nStatus,\nBalanceGSMcode,\nNotInContr,\nInContr,\nSmsCommand."};  // смс команда не распознана
const char sms_RedirectOn[]      PROGMEM = {"Command: SMS redirection has been turned on."};                        // выполнена команда для включения перенаправления всех смс от любого отправителя на номер SMSNUMBER
const char sms_RedirectOff[]     PROGMEM = {"Command: SMS redirection has been turned off."};                       // выполнена команда для выключения перенаправления всех смс от любого отправителя на номер SMSNUMBER
const char sms_WasRebooted[]     PROGMEM = {"Command: Device was rebooted."};                                       // выполнена команда для коротковременного включения сирены
const char sms_WrongGsmCommand[] PROGMEM = {"Command: Wrong GSM command."};                                         // сообщение о неправельной gsm комманде
const char sms_BalanceGSMcode[]  PROGMEM = {"Command: GSM command for getting balance was changed to "};            // выполнена команда для замены gsm команды для получения баланса
const char sms_ErrorSendSms[]    PROGMEM = {"Command: Format of command should be next:\nSendSMS 'number' 'text'"}; // выполнена команда для отправки смс другому абоненту
const char sms_SmsWasSent[]      PROGMEM = {"Command: Sms was sent."};                                              // выполнена команда для отправки смс другому абоненту

// паузы
#define  timeAllLeds          1200                         // время горение всех светодиодов во время включения устройства (тестирования светодиодов)
#define  timeHoldingBtn       2                            // время удерживания кнопки для включения режима охраны  2 сек.
#define  timeRejectCall       3000                         // время пауза перед збросом звонка

//// КОНСТАНТЫ ДЛЯ ПИНОВ /////
#define gsmLED 13
#define Button 9                              // нога на кнопку

// Спикер
#define SpecerPin 8
#define specerTone 98                         // тон спикера

//Sensores
#define pinPIR1 4                             // нога датчика движения 1


//// КОНСТАНТЫ EEPROM ////
#define E_isRedirectSms  0                    // адресс для сохранения режима перенаправления всех смс
#define E_wasRebooted    1                    // адресс для сохранения факта перезагрузки устройства по смс команде

#define numSize            13                 // количество символов в строке телефонного номера

#define E_BalanceGSMcode   70                 // GSM код для запроса баланца

#define E_NumberGsmCode    85                 // для промежуточного хранения номера телефона, от которого получено gsm код и которому необходимо отправить ответ (баланс и т.д.)

#define E_NUM1_InCall  100                    // 1-й номер для снятие с охраны
#define E_NUM2_InCall  115                    // 2-й номер для снятие с охраны
#define E_NUM3_InCall  130                    // 3-й номер для снятие с охраны

#define E_NUM1_SmsCommand  145                // 1-й номер для управления через sms
#define E_NUM2_SmsCommand  160                // 2-й номер для управления через sms
#define E_NUM3_SmsCommand  175                // 3-й номер для управления через sms
  


//// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ////

bool btnIsHolding = false;
bool wasRebooted = false;                       // указываем была ли последний раз перезагрузка программным путем

MyGSM gsm(gsmLED);                             // GSM модуль

void(* RebootFunc) (void) = 0;                          // объявляем функцию Reboot

void setup() 
{
  delay(1000);                                // !! чтобы нечего не повисало при включении
  debug.begin(9600);
  pinMode(SpecerPin, OUTPUT);
  pinMode(gsmLED, OUTPUT);
  pinMode(pinPIR1, INPUT);                    // нога датчика движения 1
  pinMode(Button, INPUT_PULLUP);              // кнопка для установки режима охраны
   
  // блок сброса очистки EEPROM (сброс всех настроек)
  if (digitalRead(Button) == LOW)
  { 
    byte count = 0;
    while (count < 100)
    {
      if (digitalRead(Button) == HIGH) break;
      count++;
      delay(100);
    }
    if (count == 100)
    {
        PlayTone(specerTone, 1000);               
        for (int i = 0 ; i < EEPROM.length() ; i++) 
          EEPROM.write(i, 0);                           // стираем все данные с EEPROM
        // установка дефолтных параметров      
        EEPROM.write(E_isRedirectSms, false);           // режим перенаправления всех смс по умолчанию выключен
        EEPROM.write(E_wasRebooted, false);             // факт перезагрузки устройства по умолчанию выключено (устройство не перезагружалось)
        RebootFunc();                                   // перезагружаем устройство
    }
  }  
 
  // блок тестирования спикера и всех светодиодов
  PlayTone(specerTone, 100);                          
  delay(500);
  digitalWrite(gsmLED, HIGH);  
  delay(timeAllLeds);
  digitalWrite(gsmLED, LOW); 
   
  gsm.Initialize();                                     // инициализация gsm модуля (включения, настройка) 
    
  // чтение конфигураций с EEPROM  
  wasRebooted = EEPROM.read(E_wasRebooted);             // читаем был ли последний раз перезагрузка программным путем  
}

bool newClick = true;

void loop() 
{  
  gsm.Refresh();                                                      // читаем сообщения от GSM модема   

  if(wasRebooted)
  {    
    gsm.SendSms(&GetStringFromFlash(sms_WasRebooted), &NumberRead(E_NUM1_SmsCommand));
    wasRebooted = false;
    EEPROM.write(E_wasRebooted, false);
  }
  
  if (gsm.NewRing)                                                  // если обнаружен входящий звонок
  {
    if (NumberRead(E_NUM1_InCall).indexOf(gsm.RingNumber) > -1 ||  // если найден зарегистрированный звонок то поднимаем трубку
        NumberRead(E_NUM2_InCall).indexOf(gsm.RingNumber) > -1 ||
        NumberRead(E_NUM3_InCall).indexOf(gsm.RingNumber) > -1          
       )      
    {               
      debug.println("Zvonokkkkkkkkkkkkk");
      delay(timeRejectCall);                              // пауза перед збросом звонка        
      gsm.RejectCall();                                   // сбрасываем вызов             
      return;     
    }
    else gsm.RejectCall();                                // если не найден зарегистрированный звонок то сбрасываем вызов (без паузы)      
    gsm.ClearRing();                                        // очищаем обнаруженный входящий звонок    
  }
      
  bool sPIR1 = SensorTriggered_PIR1();                    // проверяем датчики
  
  /*if (SensorTriggered_PIR1())                             // если сработал батчик
  {                                                                 
  } */ 
  if (gsm.NewUssd)                                            // если доступный новый ответ на gsm команду
  {    
    gsm.SendSms(&gsm.UssdText, &NumberRead(E_NumberGsmCode)); // отправляем ответ на gsm команду
    gsm.ClearUssd();                                          // сбрасываем ответ на gsm команду 
  }
  ExecSmsCommand();                                           // проверяем доступна ли новая команда по смс и если да то выполняем ее
}



//// ------------------------------- Functions --------------------------------- ////

// подсчет сколько прошло милисикунд после последнего срабатывания события (сирена, звонок и т.д.)
unsigned long GetElapsed(unsigned long &prEventMillis)
{
  unsigned long tm = millis();
  return (tm >= prEventMillis) ? tm - prEventMillis : 0xFFFFFFFF - prEventMillis + tm + 1;  //возвращаем милисикунды после последнего события
}

////// Function for setting of mods ////// 

bool ButtonIsHold(byte timeHold)
{  
  if (digitalRead(Button) == HIGH) btnIsHolding = false;               // если кнопка не нажата сбрасываем показатеь удерживания кнопки
  if (digitalRead(Button) == LOW && btnIsHolding == false)             // проверяем нажата ли кнопка и отпускалась ли после предыдущего нажатия (для избежание ложного считывание кнопки)
  { 
    btnIsHolding = true;
    if (timeHold == 0) return true;                                    // если нужно реагировать немедленно после нажатия на кнопку (без паузы на удерживания)   
    byte i = 1;
    while(1) 
    {
      delay(500);
      if (i == timeHold) return true;  
      if (digitalRead(Button) == HIGH) return false;                 
      i++;
      delay(500);      
    }
  }   
  return false;  
}

void PlayTone(byte tone, unsigned int duration) 
{
  for (unsigned long i = 0; i < duration * 1000L; i += tone * 2) 
  {
    digitalWrite(SpecerPin, HIGH);
    delayMicroseconds(tone);
    digitalWrite(SpecerPin, LOW);
    delayMicroseconds(tone);
  }
} 

// Метод датчика ////// 
bool SensorTriggered_PIR1()                                             // датчик движения 1
{
  if (digitalRead(pinPIR1) == HIGH) return true;
  else return false;
}

// Блымание светодиодом
void BlinkLEDhigh(byte pinLED,  unsigned int millisBefore,  unsigned int millisHIGH,  unsigned int millisAfter)
{ 
  digitalWrite(pinLED, LOW);                          
  delay(millisBefore);  
  digitalWrite(pinLED, HIGH);                                           // блымаем светодиодом
  delay(millisHIGH); 
  digitalWrite(pinLED, LOW);
  delay(millisAfter);
}

void BlinkLEDlow(byte pinLED,  unsigned int millisBefore,  unsigned int millisLOW,  unsigned int millisAfter)
{ 
  digitalWrite(pinLED, HIGH);                          
  delay(millisBefore);  
  digitalWrite(pinLED, LOW);                                            // выключаем светодиод
  delay(millisLOW); 
  digitalWrite(pinLED, HIGH);
  delay(millisAfter);
}

// Блымание светодиодом со спикером
void BlinkLEDSpecer(byte pinLED,  unsigned int millisBefore,  unsigned int millisHIGH,  unsigned int millisAfter)
{ 
  digitalWrite(pinLED, LOW);                          
  delay(millisBefore);  
  digitalWrite(pinLED, HIGH);                                           // блымаем светодиодом
  PlayTone(specerTone, millisHIGH);
  digitalWrite(pinLED, LOW);
  delay(millisAfter);
}

String GetStringFromFlash(char* addr)
{
  String buffstr = "";
  int len = strlen_P(addr);
  char currSymb;
  for (byte i = 0; i < len; i++)
  {
    currSymb = pgm_read_byte_near(addr + i);
    buffstr += String(currSymb);
  }
  return buffstr;
}

void WriteToEEPROM(byte e_addr, String *number)
{
  char charStr[numSize+1];
  number->toCharArray(charStr, numSize+1);
  EEPROM.put(e_addr, charStr);
}

String NumberRead(byte e_add)
{
 char charread[numSize+1];
 EEPROM.get(e_add, charread);
 String str(charread);
 if (str.startsWith("+")) return str;
 else return "***";
}

String ReadFromEEPROM(byte e_add)
{
 char charread[numSize+1];
 EEPROM.get(e_add, charread);
 String str(charread);
 return str;
}

// читаем смс и если доступна новая команда по смс то выполняем ее
void ExecSmsCommand()
{ 
  if (gsm.NewSms)
  {
    if ((gsm.SmsNumber.indexOf(NumberRead(E_NUM1_SmsCommand)) > -1 ||                                  // если обнаружено зарегистрированый номер
         gsm.SmsNumber.indexOf(NumberRead(E_NUM2_SmsCommand)) > -1 ||
         gsm.SmsNumber.indexOf(NumberRead(E_NUM3_SmsCommand)) > -1
        ) 
        ||
        (NumberRead(E_NUM1_SmsCommand).startsWith("***")  &&                                            // если нет зарегистрированных номеров (при первом включении необходимо зарегистрировать номера)
         NumberRead(E_NUM2_SmsCommand).startsWith("***")  &&
         NumberRead(E_NUM3_SmsCommand).startsWith("***")
         )
       )
    { 
      gsm.SmsText.toLowerCase();                                                         // приводим весь текст команды к нижнему регистру что б было проще идентифицировать команду
      gsm.SmsText.trim();                                                                // удаляем пробелы в начале и в конце комманды
      
      if (gsm.SmsText.startsWith("*"))                                                   // Если сообщение начинается на * то это gsm код
      {
        PlayTone(specerTone, 250); 
        if (gsm.RequestGsmCode(&gsm.SmsText))                                                                
          WriteToEEPROM(E_NumberGsmCode, &gsm.SmsNumber);                                // сохраняем номер на который необходимо будет отправить ответ                                            
        else
          gsm.SendSms(&GetStringFromFlash(sms_WrongGsmCommand), &gsm.SmsNumber);         
      }
      else
      if (gsm.SmsText.startsWith("sendsms"))                                             // запрос на отправку смс другому абоненту
      {
        PlayTone(specerTone, 250); 
        String number = "";                                                              // переменная для хранения номера получателя
        String text = "";                                                                // переменная для хранения текста перенаправляемого смс
        String str = gsm.SmsText;
        
        int beginStr = str.indexOf('\'');                                                // достаем номер телефона кому перенаправляем смс
        if (beginStr > 0)                                                                // если обнаружены параметры команды (номер, текст) то обрабатываем их и перенаправляем смс получателю
        {
          str = str.substring(beginStr + 1);                                             // достаем номер получателя
          int duration = str.indexOf('\'');  
          number = str.substring(0, duration);      
          str = str.substring(duration +1);
          
          beginStr = 0;                                                                  // достаем текст который будет перенаправляться
          duration = 0;
          beginStr = str.indexOf('\'');
          str = str.substring(beginStr + 1);
          duration = str.indexOf('\'');  
          text = str.substring(0, duration);
         }
        number.trim();
        if (number.length() > 0)                                                         // проверяем что номер получателя не пустой (смс текст не проверяем так как перенаправление пустого смс возможное)
        {
          if(gsm.SendSms(&text, &number))                                                // перенаправляем смс указанному получателю
            gsm.SendSms(&GetStringFromFlash(sms_SmsWasSent), &gsm.SmsNumber);            // и если сообщение перенаправлено успешно то отправляем отчет об успешном выполнении комманды          
        }
        else
          gsm.SendSms(&GetStringFromFlash(sms_ErrorSendSms), &gsm.SmsNumber);            // если номер получателя не обнаружен (пустой) то отправляем сообщение с ожидаемым форматом комманды 
      }
      else      
      if (gsm.SmsText == "balance")                                                      // запрос баланса
      {       
        PlayTone(specerTone, 250); 
        if(gsm.RequestGsmCode(&ReadFromEEPROM(E_BalanceGSMcode)))
          WriteToEEPROM(E_NumberGsmCode, &gsm.SmsNumber);                                // сохраняем номер на который необходимо будет отправить ответ           
        else
        {
          gsm.SendSms(&GetStringFromFlash(sms_WrongGsmCommand), &gsm.SmsNumber);                                
        }
      }         
      else
      if (gsm.SmsText.startsWith("redirect on"))        
      {
        PlayTone(specerTone, 250);
        EEPROM.write(E_isRedirectSms, true);         
        gsm.SendSms(&GetStringFromFlash(sms_RedirectOn), &gsm.SmsNumber);                                          
      }
      else 
      if (gsm.SmsText.startsWith("redirect off")) 
      {
        PlayTone(specerTone, 250);
        EEPROM.write(E_isRedirectSms, false);          
        gsm.SendSms(&GetStringFromFlash(sms_RedirectOff), &gsm.SmsNumber);       
      }     
      else
      if (gsm.SmsText.startsWith("reboot"))          
      {
        PlayTone(specerTone, 250);
        EEPROM.write(E_wasRebooted, true);                                                       // записываем статус что устройство перезагружается        
        gsm.Shutdown();                                                                          // выключаем gsm модуль
        RebootFunc();                                                                            // вызываем Reboot
      }
      else
      if (gsm.SmsText.startsWith("status"))          
      {
        PlayTone(specerTone, 250);        
        String msg = "Redirect SMS: "     + String((EEPROM.read(E_isRedirectSms)) ? "on" : "off");                   
        gsm.SendSms(&msg, &gsm.SmsNumber);          
      }           
      else 
      if(gsm.SmsText.startsWith("balancegsmcode"))
      {
        PlayTone(specerTone, 250);
        String str = gsm.SmsText;
        int beginStr = str.indexOf('\'');
        str = str.substring(beginStr + 1);
        int duration = str.indexOf('\'');  
        str = str.substring(0, duration);             
        WriteToEEPROM(E_BalanceGSMcode, &str);
        String msg = GetStringFromFlash(sms_BalanceGSMcode) + "'" + ReadFromEEPROM(E_BalanceGSMcode) + "'";
        gsm.SendSms(&msg, &gsm.SmsNumber);          
      }     
      else
      if (gsm.SmsText.startsWith("incall1"))
      {
        PlayTone(specerTone, 250);                      
        String nums[3];
        String str = gsm.SmsText;;
        for(int i = 0; i < 3; i++)
        {
          int beginStr = str.indexOf('\'');
          str = str.substring(beginStr + 1);
          int duration = str.indexOf('\'');  
          nums[i] = str.substring(0, duration);      
          str = str.substring(duration +1);            
        }        
        WriteToEEPROM(E_NUM1_InCall, &nums[0]);        
        WriteToEEPROM(E_NUM2_InCall, &nums[1]);  
        WriteToEEPROM(E_NUM3_InCall, &nums[2]);          
        String msg = "InCall1:\n'" + NumberRead(E_NUM1_InCall) + "'" + "\n"
                   + "InCall2:\n'" + NumberRead(E_NUM2_InCall) + "'" + "\n"
                   + "InCall3:\n'" + NumberRead(E_NUM3_InCall) + "'";
        gsm.SendSms(&msg, &gsm.SmsNumber);    
      }
      else           
      if (gsm.SmsText.startsWith("smscommand1"))
      {
        PlayTone(specerTone, 250);                     
        String nums[3];
        String str = gsm.SmsText;        
        for(int i = 0; i < 3; i++)
        {
          int beginStr = str.indexOf('\'');
          str = str.substring(beginStr + 1);
          int duration = str.indexOf('\'');  
          nums[i] = str.substring(0, duration);      
          str = str.substring(duration +1);         
        }        
        WriteToEEPROM(E_NUM1_SmsCommand, &nums[0]);  
        WriteToEEPROM(E_NUM2_SmsCommand, &nums[1]);
        WriteToEEPROM(E_NUM3_SmsCommand, &nums[2]);        
        String msg = "SmsCommand1:\n'" + NumberRead(E_NUM1_SmsCommand) + "'" + "\n"
                   + "SmsCommand2:\n'" + NumberRead(E_NUM2_SmsCommand) + "'" + "\n"
                   + "SmsCommand3:\n'" + NumberRead(E_NUM3_SmsCommand) + "'";
        gsm.SendSms(&msg, &gsm.SmsNumber);     
      }
      else      
      if (gsm.SmsText.startsWith("incall"))
      {
        PlayTone(specerTone, 250);        
        String msg = "InCall1:\n'" + NumberRead(E_NUM1_InCall) + "'" + "\n"
                   + "InCall2:\n'" + NumberRead(E_NUM2_InCall) + "'" + "\n"
                   + "InCall3:\n'" + NumberRead(E_NUM3_InCall) + "'";
        gsm.SendSms(&msg, &gsm.SmsNumber);                    
      }
      else      
      if (gsm.SmsText.startsWith("smscommand"))
      {
        PlayTone(specerTone, 250);       
        String msg = "SmsCommand1:\n'" + NumberRead(E_NUM1_SmsCommand) + "'" + "\n"
                   + "SmsCommand2:\n'" + NumberRead(E_NUM2_SmsCommand) + "'" + "\n" 
                   + "SmsCommand3:\n'" + NumberRead(E_NUM3_SmsCommand) + "'";
        gsm.SendSms(&msg, &gsm.SmsNumber);
      }   
      //смс команда не распознана
      else
      {
        PlayTone(specerTone, 250);              
        gsm.SendSms(&GetStringFromFlash(sms_ErrorCommand), &gsm.SmsNumber);        
      }                                                                                       // очищаем обнаруженное входящие Смс
    }    
    else if (EEPROM.read(E_isRedirectSms))                                                    // если смс пришла не с зарегистрированых номеров и включен режим перенаправления всех смс
    {
      gsm.SendSms(&String(gsm.SmsText), &NumberRead(E_NUM1_SmsCommand));     
    }    
  gsm.ClearSms(); 
  }  
}


