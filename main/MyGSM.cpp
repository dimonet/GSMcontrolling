#include "MyGSM.h"
#include "Arduino.h"

#include <SoftwareSerial.h>                   // если программный

//#define serial Serial                           // если аппаратный в UNO
SoftwareSerial serial(2, 3); // RX, TX

#define debug Serial                           // если аппаратный в UNO

#define GSM_TIMEOUT 60000                       // врем ожидание готовности модема (милсек)  
#define SMS_LIMIT   150                         // максимальное куличество символов в смс (большео лимита все символы обрезается)

MyGSM::MyGSM(byte gsmLED, byte pinBOOT)
{
  _gsmLED = gsmLED;
  _pinBOOT = pinBOOT;
  ClearRing();
  ClearSms();
  ClearUssd();  
}

// Инициализация gsm модуля (включения, настройка)
void MyGSM::Initialize()
{
  serial.begin(9600);                      // незабываем указать скорость работы UART модема
  debug.begin(9600);
  delay(1000);                            
  digitalWrite(_gsmLED, HIGH);             // на время включаем лед  
  digitalWrite(_pinBOOT, LOW);             // включаем модем   
  delay(2000);                             // нужно дождатся включения модема и соединения с сетью
  
  serial.println("ATE0");                  // выключаем эхо  
  delay(100);
  
  serial.println("AT+CLIP=1");             // включаем АОН
  delay(100);
    
  // настройка смс
  serial.println("AT+CMGF=1");           // режим кодировки СМС - обычный (для англ.)
  delay(200);
  serial.println("AT+CSCS=\"GSM\"");       // режим кодировки текста
  delay(200);
  serial.println("AT+IFC=1, 1");           // устанавливает программный контроль потоком передачи данных
  delay(200);
  serial.println("AT+CPBS=\"SM\"");        // открывает доступ к данным телефонной книги SIM-карты
  delay(200);
  serial.println("AT+CNMI=1,2,2,1,0");     // включает оповещение о новых сообщениях
  delay(200);
  serial.println("AT+CMGD=1,4");           // удаление всех старых смс
  delay(500);
  
    
  while(1)                                 // ждем подключение модема к сети
  {                             
    serial.println("AT+COPS?");
    if (serial.find("+COPS: 0")) 
    {
      BlinkLED(500, 150, 0);               // блымаем светодиодом 
      BlinkLED(150, 150, 200);             // блымаем светодиодом 
      break;
    }    
    BlinkLED(0, 500, 0);                   // блымаем светодиодом  
  }      
}

bool MyGSM::Available()
{
  return serial.available();
}

// ожидание готовности gsm модуля
bool MyGSM::IsAvailable()
{
  int i = 0;   
  while(i <= GSM_TIMEOUT)
  {  
    serial.println("AT+CPAS");                        // спрашиваем состояние модема
    if (serial.find("+CPAS: 0")) 
      {
        delay(10);       
        return true;                                  // и если он в "готовности" выходим из цикла возвращая true - модуль "готов"
      }
    delay(10);
    i += 10;
  }
  return false;                                       // если gsm так и не ответил за заданный таймаут то возвращаем false - модуль не готов к работе
}

//отправка СМС
bool MyGSM::SendSms(String *text, String *phone)      //процедура отправки СМС
{
  if (!IsAvailable()) return false;                   // ждем готовности модема и если он не ответил за заданный таймаут то прырываем отправку смс 
  // отправляем смс
  serial.println("AT+CMGS=\"" + *phone + "\""); 
  delay(100);
  serial.print(*text); 
  delay(850);
  serial.print((char)26);
  BlinkLED(0, 250, 0);                               // сигнализируем об этом   
  return true;                                       // метод возвращает true - смс отправлено успешно 
}

// звонок на заданый номер
bool MyGSM::Call(String *phone)
{  
  if (!IsAvailable()) return false;                  // ждем готовности модема и если он не ответил за заданный таймаут то прырываем выполнения звонка 
  *phone = phone->substring(1);                      // отрезаем плюс в начале так как для звонка формат номера без плюса
  serial.println("ATD+" + *phone + ";");
  BlinkLED(0, 250, 0);                               // сигнализируем об этом 
  return true;
}

// сброс звонка
void MyGSM::RejectCall()
{
  serial.println("ATH0");
}

// запрос gsm кода (*#) 
bool MyGSM::RequestUssd(String *code)
{
  if (!IsAvailable()) return false;                   // ждем готовности модема и если он не ответил то прырываем запрос 
  delay(100);                                         // для некоторых gsm модулей (SIM800l) обязательно необходима пауза между получением смс и отправкой Ussd запроса
  BlinkLED(0, 250, 0);    
  serial.println("AT+CUSD=1,\"" + *code + "\"");
  return true; 
}

// Блымание gsm светодиодом 
void MyGSM::BlinkLED(unsigned int millisBefore, unsigned int millisHIGH, unsigned int millisAfter)
{ 
  digitalWrite(_gsmLED, LOW);                          
  delay(millisBefore);  
  digitalWrite(_gsmLED, HIGH);                 // блымаем светодиодом
  delay(millisHIGH); 
  digitalWrite(_gsmLED, LOW);
  delay(millisAfter);
}

void MyGSM::Refresh()
{
  String currStr = "";     
  byte strCount = 0;  
  
  while (Available())
  {
    char currSymb = serial.read();    
    debug.print(currSymb);
    if ('\r' == currSymb) 
    {
      if (strCount == 0)
      {
        if (currStr.startsWith("RING"))                    // если входящий звонок
        {
          BlinkLED(0, 250, 0);                             // сигнализируем об этом 
          NewRing = true;          
          strCount = 1;
        }
        else
        if (currStr.startsWith("+CMT"))                    // если СМС
        {
          BlinkLED(0, 250, 0);                             // сигнализируем об этом 
          NewSms = true;
          SetString(&currStr, &SmsNumber);                                                           
          strCount = 1;
        }
        else
        if (currStr.startsWith("+CUSD"))                   // если ответ от gsm команды
        {
          BlinkLED(0, 250, 0);                             // сигнализируем об этом
          NewUssd = true;         
          SetString(&currStr, &UssdText);                    
        }         
      }
      else
      if (strCount == 1) 
      {         
        if (NewRing)                                       // если входящий звонок
          strCount = 2;           
        else       
        if (NewSms)                                        // если СМС
        {
          SmsText = currStr;                                        
        }
                
      }
      else
      if (strCount == 2) 
      {
        if (NewRing)                                       // если входящий звонок
        {
         SetString(&currStr, &RingNumber);                               
        }                 
      }        
    currStr = "";    
    } 
    else if ('\n' != currSymb ) 
    {
      if (currSymb == '\"') currStr += "\\" + String(currSymb);
      else currStr += String(currSymb);           
      delay(5);
    }
  }
      
  if (NewSms)
  { 
    serial.println("AT+CMGD=1,4");                         // удаление всех старых смс
    delay(300);
  }    
}

void MyGSM::SetString(String *source, String *target)
{
  byte beginStr = source->indexOf('\"');
  *target = source->substring(beginStr + 1);
  byte duration = target->indexOf('\"');  
  if (duration > 0)
    *target = target->substring(0, duration - 1);                      // если длина строки не нулевая то вырезаем строку согласно вычесленной длины иначе возвращаем до конца всей строки
  if (target->length() > SMS_LIMIT)
  {  
    *target = target->substring(0, SMS_LIMIT - 4);                               // обрезаем строку до 156 символов что б она поместилась в одну смс
    *target += "...";                                            // добавляем многоточие для указания, что текст не полный
  } 
}

void MyGSM::ClearRing()
{
  NewRing = false;
  RingNumber = "";  
}

void MyGSM::ClearSms()
{
  NewSms = false;
  SmsNumber = "";
  SmsText = "";  
}

void MyGSM::ClearUssd()
{
  NewUssd = false;  
  UssdText = "";  
}

void MyGSM::Shutdown()
{
  digitalWrite(_pinBOOT, HIGH);                          // выключаем пинг который включает модем
  serial.println("AT+CPWROFF");                          // посылаем команду выключения gsm модема  
  delay(100);
}
;
