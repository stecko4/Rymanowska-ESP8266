/*Connecting the BME280 Sensor:
Sensor              ->  Board
-----------------------------
Vin (Voltage In)    ->  3.3V
Gnd (Ground)        ->  Gnd
SDA (Serial Data)   ->  D2 on NodeMCU / Wemos D1 PRO
SCK (Serial Clock)  ->  D1 on NodeMCU / Wemos D1 PRO */

//BME280 definition and Mutichannel_Gas_Sensor
#include <EnvironmentCalculations.h>
#include <Wire.h>
#include "cactus_io_BME280_I2C.h"
//Create BME280 object
BME280_I2C bme(0x76);			//I2C using address 0x76

//Wyswietlacz OLED
#include <Arduino.h>
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display

//#define BLYNK_DEBUG			//Optional, this enables lots of prints
//#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
WidgetBridge bridge1(V20);		//Initiating Bridge Widget on V20 of Device A
WidgetTerminal terminal(V40);	//Attach virtual serial terminal to Virtual Pin V40
WidgetLED LED_CO(V8);			//Inicjacja diody LED dla załączania CO
SimpleTimer TimerBlynkCheck;	//Do sprawdzana połączenia z Blynkiem uruchamiany do 30s
SimpleTimer TimerMainFunction;	//dla MainFunction uruchamiany do 3s
WidgetRTC rtc;					//Inicjacja widgety zegara czasu rzeczywistego RTC

int Tryb_Sterownika			= 0;		//Tryb_Sterownika 0 = AUTO, 1 = ON, 2 = OFF, 3 = MANUAL
float SetTempManual			= 21;		//Temperatura nastawiana manulnie z aplikacji Blynka 
float SetTempActual			= 18.5;		//Temperatura według której sterowana jest temperatura (auto lub manual)
float SetTempSchedule[7][24] = {
//00:00 01:00 02:00 03:00 04:00 05:00 06:00 07:00 08:00 09:00 10:00 11:00 12:00 13:00 14:00 15:00 16:00 17:00 18:00 19:00 20:00 21:00 22:00 23:00
  {18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 18.5 },  //Niedziela
  {18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 18.5 },  //Poniedziałek
  {18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 18.5 },  //Wtorek
  {18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 18.5 },  //Środa
  {18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 18.5 },  //Czwartek
  {18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 20.8, 18.5 },  //Piątek
  {18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5, 18.5 }   //Sobota
}; //Ustawienia temperatury dla poszczeguknych dnii godzin, temperatura musi być pomiędzy 14 a 26 stopni celcjusza z dokładnością do 2 miejsc po przecinku

int		OLED_ON				= 1;		//Przyjmuje wartość '1' dla OLED włączony i '0' dla OLED wyłączony
boolean	Podlane				= true;		//Przyjmuje wartość true gdy podlane czyli wilgotność >80% i false gdy sucho wilgotność <60%
boolean	SoilNotification60	= false;	//Przyjmuje wartość true gdy wysłano notyfikacje o wilgotności 60%, true po podlaniu
boolean	SoilNotification50	= false;	//Przyjmuje wartość true gdy wysłano notyfikacje o wilgotności 50%, true po podlaniu
boolean	SoilNotification40	= false;	//Przyjmuje wartość true gdy wysłano notyfikacje o wilgotności 40%, true po podlaniu
float	RH 					= 0;		//Soil Relative Humidity in % 
long	CZAS_START_MANUAL	= 0;		//Ustawienie czasu przejścia sterowania w trym MANUAL (wartość w sekundach)
long	CZAS_START_AUTO		= 0;		//Ustawienie czasu przejścia sterowania w trym AUTO (wartość w sekundach)
int		dzien				= 0;		// day of the week (1-7), Sunday is day 1
int		godz				= 1;		// the hour now  (0-23)
float	temp(NAN), hum(NAN), pres(NAN), dewPoint(NAN), absHum(NAN), heatIndex(NAN);	//Zmienne dla danych z czujnika BME280

//STAŁE
const char	ssid[]			= "XXXX";
const char	pass[]			= "XXXX";
const char	auth[]			= "XXXX";	//Token Pokój Rymanowska
const int	checkInterval	= 30000;	//Co 30s zostanie sprawdzony czy jest sieć Wi-Fi i czy połączono z serwererem Blynk
const int	HeatCO			= D6;		//Pin do włączania CO w sterowniku w łazience
const int	MinTemp			= 14;		//Najniższa możliwa temperatura do ustawienia
const int	MaxTemp			= 26;		//Najwyższa możliwa temperatura do ustawienia
const float	TemtHist		= 0.2;		//histereza dla temperatury
const float	HumidHist		= 5;		//histereza dla wilgotności

BLYNK_CONNECTED() {			//Informacja że połączono z serwerem Blynk, synchronizacja danych
	Serial.println("Reconnected, syncing with cloud.");
	bridge1.setAuthToken("c1614814b4b64afb8ab15c23620ed60d"); // Token of the hardware B (Łazienka)
	rtc.begin();
	Blynk.syncAll();
}

void blynkCheck() {					//Sprawdza czy połączone z serwerem Blynk
	if (WiFi.status() == 3) {
		if (!Blynk.connected()) {
			Serial.println("WiFi OK, trying to connect to the Blynk server...");
			Blynk.connect();
		}
	}

	if (WiFi.status() == 1) {
		Serial.println("No WiFi connection, offline mode.");
	}
}

void MainFunction() {			//Robi wszystko co powinien
	Read_BME280_Values();		//Odczyt danych z czujnika BME280
	TrybManAuto();				//Ustawienie trybów sterowania i temperatury do załączenia pieca CO
	RH = ReadSoilMoisture();	//Odczyt z czujnika wilgotności gleby i konwersja do wartości 0 - 100%
	Room_Temp_Control();		//kontrola temperatury na podstawie odczytów z BME280
	OLED_Display();				//Wyświetlanie na ekranie OLED 0.96"
	Wyslij_Dane();				//Wysyła dane do serwera Blynk
}

void TrybManAuto() {			//Ustawienie trybów sterowania i temperatury do załączenia pieca CO
	//Start Manual za pomocą Time Input
	if (CZAS_START_MANUAL > 0){				//Tryb_Sterownika 0 = AUTO, 1 = ON, 2 = OFF, 3 = MANUAL
		if(hour(now()) * 60 + minute(now()) == CZAS_START_MANUAL / 60){
			CZAS_START_MANUAL = 0;		//Zerowanie czasu uruchomienia tryby MANUALNEGO, czyli wyłączenie załączanie czasowego
			Blynk.virtualWrite(V11, 4);	//Ustawienie widgetu BLYNK w tryb MAN
			Tryb_Sterownika = 3;		//Tryb_Sterownika 0 = AUTO, 1 = ON, 2 = OFF, 3 = MANUAL
			//reset Time Input widget in app
			int startAt = - 1;
			int stopAt;
			if (CZAS_START_AUTO == 0) {
				stopAt = - 1;
			}
			else {
				stopAt = CZAS_START_AUTO;
			}
			Blynk.virtualWrite(V13, startAt, stopAt, "Europe/Warsaw");
			//Notyfikacje
			String notif = "Tryb Manual ON, CZAS_START_MANUAL=" + String(CZAS_START_MANUAL) + ", minuty=" + String(hour(now()) * 60 + minute(now()));
			Blynk.notify(notif);
		}
	}
	// Start Auto za pomocą Time Input
	if (CZAS_START_AUTO > 0){		//Tryb_Sterownika 0 = AUTO, 1 = ON, 2 = OFF, 3 = MANUAL
		if(hour(now()) * 60 + minute(now()) == CZAS_START_AUTO / 60){
			CZAS_START_AUTO = 0;		//Zerowanie czasu uruchomienia tryby AUTOMATYCZNEGO, czyli wyłączenie załączanie czasowego
			Blynk.virtualWrite(V11, 1); //Ustawienie widgetu BLYNK w tryb AUTO
			Tryb_Sterownika = 0;		//Tryb_Sterownika 0 = AUTO, 1 = ON, 2 = OFF, 3 = MANUAL
			//reset Time Input widget in app
			int startAt = - 1;
			int stopAt = - 1;
			Blynk.virtualWrite(V13, startAt, stopAt, "Europe/Warsaw");
			//Notyfikacje
			String notif = "Tryb Auto ON, CZAS_START_AUTO=" + String(CZAS_START_AUTO) + ", minuty=" + String(hour(now()) * 60 + minute(now()));
			Blynk.notify(notif);
		}
	}

	dzien = weekday(now()) - 1;			//day of the week (1-7), Sunday is day 1
	godz = hour(now());					//the hour now  (0-23)
	//Ustawienie temperatury sterowania
	if (Tryb_Sterownika == 0){			//Tryb AUTO,  temperatura w zmiennej SetTempActual ustawianay tylko jeśli różna od zadanej w SetTempSchedule
		if(SetTempActual != SetTempSchedule[dzien][godz]){
			SetTempActual = SetTempSchedule[dzien][godz]; 
			}
		}
	else if (Tryb_Sterownika == 1) {	//Tryb ON,  temperatura w zmiennej SetTempActual ustawianay tylko jeśli różna od zadanej w SetTempSchedule
		if(SetTempActual != temp){
		SetTempActual = temp;
		}
	}
	else if (Tryb_Sterownika == 2) {	//Tryb OFF,  temperatura w zmiennej SetTempActual ustawianay tylko jeśli różna od 0 (zera)
		if(SetTempActual != 0) {
		SetTempActual = 0;
		}
	}
	else if(Tryb_Sterownika == 3) {		//Tryb MAN,  temperatura w zmiennej SetTempActual ustawianay tylko jeśli różna od zadanej w SetTempManual
		if(SetTempActual != SetTempManual){
		SetTempActual = SetTempManual;
		}
	}
}

void Read_BME280_Values() {	//Odczyt wskazań z czujnika BME280
	bme.readSensor();		//Odczyt wskazań z czujnika BME280
	pres = bme.getPressure_MB();
	hum = bme.getHumidity();
	temp = bme.getTemperature_C();
	EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
	EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
	//Dane obliczane na podstawie danych z czujnika
	dewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
	absHum = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit);
	heatIndex = EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit);
}

float ReadSoilMoisture() {	//Odczyt z czujnika wilgotności gleby i konwersja do wartości 0 - 100%
	int i;
	float sval = 0;

	for (i = 0; i < 5; i++) {
		sval = sval + analogRead(A0);//sensor on analog pin 0
	}
	sval = sval / 5;				//average (row walue min=427, max=818)
	sval = 100- ((sval - 427) / ((818 - 427) * 0.01));	//Convert to Relative Humidity in % RH% REF: https://raspberryblog.de/?p=1972&page=2

	//informacja o podlaniu
	if (sval > 80 && Podlane == false) {
		Podlane = true;				//Ustawia wartość na true czyli podlano kwiatka
		SoilNotification40 = false;	//Ustawia wartość na true czyli zeruje ilość powiadomień po podlaniu kwiatka
		SoilNotification50 = false;	//Ustawia wartość na true czyli zeruje ilość powiadomień po podlaniu kwiatka
		SoilNotification60 = false;	//Ustawia wartość na true czyli zeruje ilość powiadomień po podlaniu kwiatka
		Blynk.notify("Kwiatek został podlany :)");
	}
	else if (sval < 60 && Podlane == true) {
		Podlane = false;			//Ustawia wartość na false czyli kwiatek ma sucho
	}
	//Powiadomienia na smartfona
	if (sval < 40 && Podlane == false && SoilNotification40 == false) {
		Blynk.notify("Podlej tego kwiatka bo uschnie! Wilgotnośc spadła już do 40%.");
		SoilNotification40 = true;	//Ustawia wartość na true czyli powiadomienie przy wilgotności 40% poszło
	}
	else if (sval < 50 && Podlane == false  && SoilNotification50 == false) {
		Blynk.notify("Wilgotność w kwiatku spadła poniżej 50%. Terazjuż trzeba podlać!");
		SoilNotification50 = true;	//Ustawia wartość na true czyli powiadomienie przy wilgotności 50% poszło
	}
	else if (sval < 60 && Podlane == false && SoilNotification60 == false) {
		Blynk.notify("Wilgotność w kwiatku spadła poniżej 60%. Może trzeba podlać?");
		SoilNotification60 = true;	//Ustawia wartość na true czyli powiadomienie przy wilgotności 60% poszło
	}

	return sval;
}

void Room_Temp_Control() {	//Sterowanie piecem w zależności od temperatury
	if (Tryb_Sterownika == 1) {
		bridge1.digitalWrite(HeatCO, 0);	//Wysłąnie sygnały do włączenia pieca
		LED_CO.on();						//Piec CO grzeje
		digitalWrite(BUILTIN_LED, LOW);		//Niebieska dioda WEMOSA gaśnie
	}
	else if (temp < SetTempActual - TemtHist) {
		bridge1.digitalWrite(HeatCO, 0);	//Wysłąnie sygnały do włączenia pieca
		LED_CO.on();						//Piec CO grzeje
		digitalWrite(BUILTIN_LED, LOW);		//Niebieska dioda WEMOSA gaśnie
	}
	else if (temp > SetTempActual + TemtHist) {
		bridge1.digitalWrite(HeatCO, 1023);	//Wysłąnie sygnały do wyłączenia pieca (1023 bo piny obsługują PWM i nadanie "1" nie działa)
		LED_CO.off();						//Piec CO nie grzeje
		digitalWrite(BUILTIN_LED, HIGH);	//Niebieska dioda WEMOSA świeci
	}
}

void Wyslij_Dane() {			//Wysyłanie danych na serwer Blynka
	Blynk.virtualWrite(V0, temp);				//Temperatura [deg C]
	Blynk.virtualWrite(V1, hum);				//Wilgotność [%]
	Blynk.virtualWrite(V2, pres);				//Ciśnienie [hPa]
	Blynk.virtualWrite(V3, dewPoint);			//Temperatura punktu rosy [deg C]
	Blynk.virtualWrite(V4, absHum);				//Wilgotność bezwzględna [g/m³]
	Blynk.virtualWrite(V5, heatIndex);			//Temperatura odczuwalna [deg C] 
	Blynk.virtualWrite(V6, RH);					//Wilgotność gleby [%]  
	Blynk.virtualWrite(V18, SetTempActual);		//Temperatura zadana [deg C]
	Blynk.virtualWrite(V25, map(WiFi.RSSI(), -105, -40, 0, 100) ); //Siła sygnału Wi-Fi [%]
	bridge1.virtualWrite(V21, hum);				//Wilgotność w pokoju wysyłana do sterownika w łazience [%]      
	//TESTOWANIE TYMCZOSOWE
	Blynk.virtualWrite(V30, CZAS_START_MANUAL); 
	Blynk.virtualWrite(V31, CZAS_START_AUTO); 
}

BLYNK_WRITE(V40) {						//Obsluga terminala
	if (String("Ports") == param.asStr()) {
		terminal.clear();
		terminal.println("Port      Description        Type");
		terminal.println("V0        Temperature        Value");
		terminal.println("V1        Humdity            Value");
		terminal.println("V2        Pressure           Value");
		terminal.println("V3        DewPoint           Value");
		terminal.println("V4        Abs Humdity        Value");
		terminal.println("V5        Heat Index         Value");
		terminal.println("V6        Soil Moisture      Value");
		terminal.println("V25       WiFi Signal        Value");
		terminal.println("V40       Terminal           Terminal");
	}
	else if (String("Hello") == param.asStr()) {
		terminal.clear();
		terminal.println("Hi Lukasz. Have a great day!");
	}
	else {
	terminal.clear();
		terminal.println("Type 'Ports' to show list") ;
		terminal.println("or 'Hello' to say hello!") ;
	}

	// Ensure everything is sent
	terminal.flush();
}

void OLED_Display() {		//Wyświetlanie na ekranie OLED 0.96"
	if (OLED_ON == 1) {
		//Wyświetlanie dane na OLED
		String DispTemp = "Temp:               " + String(temp) + "     "+ char(176) + "C";
		String DispPress = "Press:            " + String(pres) +"   hPa";
		String DispHum = "Hum:               "+String(hum) + "      %";
		String DispdewPoint = "DewPoint:       " + String(dewPoint) + "     " + char(176) + "C";
		String DispabsHum = "AbsHum:         " + String(absHum) +"   g/m" + char(179);

		String currentTime = String(hour()) + ":" + minute() + ":" + second();
		String currentDate = String(day()) + " " + month() + " " + year();

		u8g2.clearBuffer();
		u8g2.setFontMode(1);
		u8g2.setFont(u8g2_font_helvR08_tf);
		u8g2.drawStr(0,9,"Set:");

		if (Tryb_Sterownika != 2) {
			u8g2.setCursor(40,28);
			u8g2.print("°C");
		}
		u8g2.setCursor(40+64,28);
		u8g2.print("°C");
		u8g2.drawStr(64,9,"Actual:");
		u8g2.setFont(u8g2_font_logisoso16_tf); 
		u8g2.setCursor(3,35);

		if (Tryb_Sterownika != 2) {
			u8g2.print(SetTempActual,1);
		}
		else {
			u8g2.print("OFF");
		}
		//u8g2.print(SetTempActual,1);
		u8g2.setCursor(64,35);
		u8g2.print(temp,1);
		//Wyświetlanie aktualnego czasu
		u8g2.setFont(u8g2_font_helvR08_tf);   
		u8g2.setCursor(0,64);
		u8g2.print(currentTime);
		//Wyświetlanie aktualnej daty  
		u8g2.setCursor(75,64);
		u8g2.print(currentDate);
		u8g2.sendBuffer();
	}
	else if (OLED_ON == 0) {
		u8g2.clearBuffer();
		u8g2.sendBuffer();
	}
}

BLYNK_WRITE(V10) {			//Włączanie i wyłączanie wyświetlacza
	OLED_ON = param.asInt(); 
}

BLYNK_WRITE(V11) {			//Sterowanie ogrzewaniem z aplikacji (AUTO, ON, OFF, MAN)
	switch (param.asInt()) {
		case 1:				//AUTO
			Tryb_Sterownika = 0;
			break;
		case 2:				//ON
			Tryb_Sterownika = 1;
			break;
		case 3:				//OFF
			Tryb_Sterownika = 2;
			break;
		case 4:				//MAN
			Tryb_Sterownika = 3;
			break;
			default:		//Wartość domyślna AUTO
			Tryb_Sterownika = 0;
	}
}

BLYNK_WRITE(V12) {			//Ustawienie progu temperatury poniżej której załączy się CO (plus próg)
	SetTempManual = param.asFloat();
}

BLYNK_WRITE(V13) {			//Obsługa timera Start Manual i Start Auto (Time Input Widget)
	CZAS_START_MANUAL = param[0].asLong();		//Ustawienie czasu przejścia sterowania w trym MANUAL (wartość w sekundach)
	CZAS_START_AUTO = param[1].asLong();		//Ustawienie czasu przejścia sterowania w trym AUTO (wartość w sekundach)
}

/***********************************************************************************************/

void setup() {
	Serial.begin(115200);
	WiFi.begin(ssid, pass);
	Blynk.config(auth);

	//Inicjalizacja Timerów
	TimerBlynkCheck.setInterval(checkInterval, blynkCheck);		//Multiple timer https://codebender.cc/example/SimpleTimer/SimpleTimerAlarmExample#SimpleTimerAlarmExample.ino
	TimerMainFunction.setInterval(3000, MainFunction);			//1000 = 1s

	Wire.begin();

	//Ustawianie pinów
	pinMode(BUILTIN_LED, OUTPUT);		//Będzie mrógał diodą

	//inicjowanie wyświetlacza
	u8g2.begin();
	u8g2.enableUTF8Print();
	u8g2.setDisplayRotation(U8G2_R0);
	u8g2.setFlipMode(1);				//Odwrócenie ekrany o 180stopnie jeśli parapetr = 1

	u8g2.clearBuffer();
	u8g2.setFontMode(1);
	u8g2.setFont(u8g_font_helvB24);
	u8g2.setCursor(0,33);
	u8g2.print(F("BME280"));
	u8g2.setFont(u8g_font_helvR08);
	//u8g2.setCursor(0, 50);
	//u8g2.print(clock.dateFormat("d-m-Y H:i:s", dt));
	u8g2.setCursor(0, 64);
	u8g2.print("Connecting to: "+String(ssid));
	u8g2.sendBuffer();

	//inicjowanie czujnika BME280
	if (!bme.begin()) {
		Serial.println("Could not find a valid BME280 sensor, check wiring!");
		while (1);
	}

	bme.setTempCal(-5.8);			// Temp was reading high so subtract 5.8 degree 
}

void loop() {
	TimerBlynkCheck.run();
	TimerMainFunction.run();
	if (Blynk.connected()) Blynk.run();
}