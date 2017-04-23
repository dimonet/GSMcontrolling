#include "Arduino.h"

class MyGSM
{
  public: 
    MyGSM(byte gsmLED, byte pinBOOT);
    void Initialize();    
    bool Available();
    bool NewRing;
    bool NewSms;
    bool NewUssd;
    String RingNumber;
    String SmsNumber;
    String SmsText;
    String UssdText;
    String Read();
    bool SendSms(String *text, String *phone);         // метод возвращает true если смс отправлен успешно
    bool Call(String *phone);
    void RejectCall();    
    bool RequestUssd(String *code);                 // запрос gsm кода (*#)
    void Refresh();
    void ClearRing();
    void ClearSms();
    void ClearUssd();
    void Shutdown();                                  // выключения gsm модула (может использоваться при перезагрузке всего устройства)
    
  private:
    bool IsAvailable();                               // ожидание готовности gsm модуля
    void BlinkLED(unsigned int millisBefore, unsigned int millisHIGH, unsigned int millisAfter);
    void SetString(String *source, String *target);
    int _gsmLED;
    int _pinBOOT;                                     // нога BOOT или K на модеме       
};
