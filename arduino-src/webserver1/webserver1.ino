/* Homecontroll via JSON Protokoll */

#include <TimedAction.h>
#include <TimerOne.h>
#include <DS18S20.h>
#include <OneWire.h>


#if ARDUINO > 18
#include <SPI.h> // Für Arduino Version größer als 0018
#endif
#include <Ethernet.h>
#include <TextFinder.h>
#include <SD.h>

/**
http://192.168.0.102/?pinD3=1
**/

byte mac[] = { 0x5A, 0xA2, 0xDA, 0x0D, 0x56, 0x7A }; // MAC-Adresse des Ethernet-Shield
byte ip[]  = { 192, 168, 0, 102 };                   // IP zum aufrufen des Webservers
byte sdPin = 4;                                      // Pin der SD-Karte

EthernetServer server(80);                           // Server port

File webFile;

//DS1820 an Pin2
OneWire  ds(2);        // ds18b20 pin #2 (middle pin) to Arduino pin 2
byte i;
byte present = 0;
byte data[12];
byte addr[8];

//Temp Berechnung  
int HighByte, LowByte, SignBit, Whole, Fract, TReading, Tc_100, FWhole;

// TEMP Management
int Solltemp = 20; // als Basis Temperatur setzen
int Isttemp;

// Vorlaufregelung
int FVup = 22;
int FVdown = 23;

TimedAction timedAction = TimedAction(1000,checkstate);
TimedAction timedval = TimedAction(10000,triggerstate);
boolean vorlauf1State = false;

void setup()
{ 
  Ethernet.begin(mac, ip); // Client starten
  server.begin();          // Server starten
  Serial.begin(9600);
  Serial.println("ARDUINO - STEUERUNG");
  
  if ( !ds.search(addr)) 
  {
    Serial.println("No more addrs");
    delay(1000);
    ds.reset_search();
    return;
  }

  if ( OneWire::crc8( addr, 7) != addr[7])
  {
      Serial.println("CRC not valid!");
      delay(1000);
      return;
  }
  /*
  Serial.println("Initialisiere SD-Karte...");
  
  if (!SD.begin(sdPin)) 
  {
    Serial.println(" - Initialisierung der SD-Karte fehlgeschlagen!");
    return;
  }
  Serial.println(" - SD-Karte erfolgreich initialisiert.");

  if (!SD.exists("index.htm")) 
  {
    Serial.println(" - Datei (index.htm) wurde nicht gefunden!");
    return;
  }
  Serial.println(" - Datei (index.htm) wurde gefunden.");
*/
}

void loop()
{
  getTemp();
  //printTemp();
  timedAction.check();
  
  EthernetClient client = server.available(); // Auf Anfrage warten

  if(client)
  {
    /*****************************************
      Ausgänge über das Webformular steuern  *
    *****************************************/
    TextFinder finder(client);

    if(finder.find("GET"))
    {
      while(finder.findUntil("pin", "\n\r"))
      {
        char typ = client.read();
        int  pin = finder.getValue();
        int  val = finder.getValue();

        if(typ == 'D')
        {
          pinMode(pin, OUTPUT);
          digitalWrite(pin, val);
          Serial.print(" - D"+String(pin));
        }
        else if(typ == 'A')
        {
          analogWrite(pin, val);
          Serial.print(" - A"+String(pin));
        }
        else if(typ == 'T')
        {
          Serial.print(" - T"+String(pin));
          if(pin == 1)//T1 Vorlauf Fussboden
          {
            Solltemp = val;
            Serial.print(" - TEMP SOLL Vorlauf Fussboden "+String(Solltemp));
            
          }
        }
        else Serial.print(" - Falscher Typ");

        if(val==1) Serial.println(" ein"); else Serial.println(" aus");
      }
    }

    /************************
      Webformular anzeigen  *
    ************************/
    boolean current_line_is_blank = true;       // eine HTTP-Anfrage endet mit einer Leerzeile und einer neuen Zeile

    while (client.connected()) 
    {
      EthernetClient client = server.available();
      if (client) 
      {
        Serial.println("new client");
        // an http request ends with a blank line
        boolean currentLineIsBlank = true;
        while (client.connected()) 
        {
          if (client.available()) 
          {
            char c = client.read();
            Serial.write(c);
            // if you've gotten to the end of the line (received a newline
            // character) and the line is blank, the http request has ended,
            // so you can send a reply
            if (c == '\n' && currentLineIsBlank) 
            {
              // send a standard http response header
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html");
              client.println("Connnection: close");
              client.println();
              client.println("<!DOCTYPE HTML>");
              client.println("<html>");
              // add a meta refresh tag, so the browser pulls again every 5 seconds:
              //client.println("<meta http-equiv=\"refresh\" content=\"5\">");
              client.print("DS18B20 <br/>");
              client.print("Temp: ");
              client.print(Whole);
              client.print("<br />");
              /*
              for (int analogChannel = 0; analogChannel < 6; analogChannel++) 
              {
                int sensorReading = analogRead(analogChannel);
                client.print("analog input ");
                client.print(analogChannel);
                client.print(" is ");
                client.print(sensorReading);
                client.println("<br />");       
              }
              */
              client.println("</html>");
              break;
            }
            if (c == '\n') 
            {
              // you're starting a new line
              currentLineIsBlank = true;
            } 
            else if (c != '\r') 
            {
              // you've gotten a character on the current line
              currentLineIsBlank = false;
            }
          }
        }
        // give the web browser time to receive the data
        delay(1);
        // close the connection:
        client.stop();
        Serial.println("client disonnected");
      }
    }
  }
} 


void getTemp() 
{
  int foo, bar;
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44,1);
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);

  for ( i = 0; i < 9; i++) {
    data[i] = ds.read();
  }
  
  LowByte = data[0];
  HighByte = data[1];
  TReading = (HighByte << 8) + LowByte;
  SignBit = TReading & 0x8000;  // test most sig bit
  
  if (SignBit) {
    TReading = -TReading;
  }
  Tc_100 = (6 * TReading) + TReading / 4;    // multiply by (100 * 0.0625) or 6.25
  Whole = Tc_100 / 100;          // separate off the whole and fractional portions
  Fract = Tc_100 % 100;
  if (Fract > 49) {
    if (SignBit) {
      --Whole;
    } else {
      ++Whole;
    }
  }

  if (SignBit) {
    bar = -1;
  } else {
    bar = 1;
  }
  foo = ((Whole * bar) * 18);      // celsius to fahrenheit conversion section
  FWhole = (((Whole * bar) * 18) / 10) + 32;
  if ((foo % 10) > 4) {            // round up if needed
       ++FWhole;
  }
}

void printTemp(void) {
  Serial.println("Temp is: ");
   
  
  if (SignBit) {  
     Serial.println("-");
  }
  Serial.println(Whole);
  Serial.println(" C / ");
  Serial.println(FWhole);
  Serial.println(" F");
}

void checkstate()
{
  vorlauf1State ? vorlauf1State=false : vorlauf1State=true;
  //digitalWrite(FVdown,vorlauf1State);
}

void triggerstate()
{

}

