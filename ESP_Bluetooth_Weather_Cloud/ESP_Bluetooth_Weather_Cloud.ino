#include <ESP8266WiFi.h>

// the character with which each data transmission ends
#define DATA_END 192

#define WIFI_NAME "your network name"
#define WIFI_PASSWORD "your network password"

// the hostname where the data is sent
// change to your host
const char* host = "example.com";

// structure to hold the weather data
// add/delete as many fields as you want but be careful at the memory
// each float or long takes up 4 bytes
// my example: 3 temperatures (from 2 DS18B20 and one BME280), 1 pressure (from BME) and 1 humidity (from BME)
typedef struct {
	////// weather data fields
	float temp1;
	float temp2;
	float temp3;
	float pres;
	float hum;
	//////

	// number of milliseconds since esp power on when the data was received
	long tmillis;
}WSdata;

// array to hold values in case of inability to connect to host
// this can be caused by host offline, wifi down, etc.
// my example: each structure is 24 bytes, so an array of 1000 structures will be 24000 bytes (~24kbytes)
// this is enough to fit inside the RAM of the ESP (approximately 50kbytes available for the user)
// however, this is not a good programming practice on embedded hardware as RAM is limited
// until I figure out how safe is it to use the SPIFFS flash memory available on the ESP
// and how many write cycles it can support, this is the easiest way to save some data
// future versions will move to SPIFFS or an external SD card as data can be perserved
// after a power down
// at a rate of one value every two minutes, 1000 values are about 1 and a half days
WSdata vect[1000];

// returns the index of the first free position in the array 
// where a new value can be inserted
int getFirstIndex()
{
	for (int i = 0; i < 1000; ++i)
	{
		if (vect[i].temp1 == 0 && vect[i].temp2 == 0 && vect[i].temp3 == 0
			&& vect[i].pres == 0 && vect[i].hum == 0 && vect[i].tmillis == 0)
			return i;
	}
	return 1000;
}

void sendPost(float temp1, float temp2, float temp3, float hum, float pres, long tmillis)
{
	// a WiFi client to connect to the server
	WiFiClient client;

	// the port, 80 for HTTP
	const int httpPort = 80;

	// if we cannot connect to the server, we add the data to the vector
	// and return from the function
	if (!client.connect(host, httpPort))
	{
		int i = getFirstIndex();
		vect[i].temp1 = temp1;
		vect[i].temp2 = temp2;
		vect[i].temp3 = temp3;
		vect[i].pres = pres;
		vect[i].hum = hum;
		vect[i].tmillis = tmillis;
		return;
	}

	// if we are connected, we check if there is data in the vector 
	// that has to be sent
	// if the first free slot is not 0, then we have data to send
	int idx = getFirstIndex();
	if (idx)
	{
		// send all values from the array (up to the first free slot)
		for (int i = 0; i < idx; ++i)
		{
			// if the client disconnected we attempt to connect
			// if it fails, we return from the function
			if (!client.connected()) {
				if (!client.connect(host, httpPort))
					return;
			}

			// the number of milliseconds since we received the data
			// can be used on the server to find out the time 
			// when the data was received
			long mil = millis() - vect[i].tmillis;

			// create the string to send
			// change to your own URL parameter names and number of parameters
			String data = "temp1=" + String(vect[i].temp1) + "&temp2=" + String(vect[i].temp2)
				+ "&temp3=" + String(vect[i].temp3) + "&hum=" + String(vect[i].hum)
				+ "&pres=" + String(vect[i].pres) + "&millis=" + String(mil);

			// send a POST request to the server
			// change "your/endpoint/name"
			client.println("POST /your/endpoint/name?" + data + " HTTP/1.1");
			client.println("Host: " + String(host));
			client.println("Accept: */*");
			client.println("Content-Type: application/x-www-form-urlencoded");
			client.println("Content-Length: " + data.length());
			client.println();
			client.print(data);

			// after sending the data we free the current position in the array
			// by setting all of its values to 0
			vect[i].temp1 = 0;
			vect[i].temp2 = 0;
			vect[i].temp3 = 0;
			vect[i].pres = 0;
			vect[i].hum = 0;
			vect[i].tmillis = 0;

			// delay between each request, can be changed
			delay(500);

			// disconnect and reconnect after each post request
			// if we do not do this, only a few requests will be sent
			// (most of the times only 2 request will be sent)
			// TO DO: find out why
			if (client.connected()) {
				// disconnect from the server
				client.stop();
			}
		}
	}

	// if we didn't add the current data to the array before
	// but the client disconnected since then
	// we add the data now if we cannot connect
	// and return from the function
	if (!client.connected())
	{
		if (!client.connect(host, httpPort))
		{
			int i = getFirstIndex();
			vect[i].temp1 = temp1;
			vect[i].temp2 = temp2;
			vect[i].temp3 = temp3;
			vect[i].pres = pres;
			vect[i].hum = hum;
			vect[i].tmillis = tmillis;
			return;
		}
	}
	// millis is 0 because the data is sent as it is received, without being
	// saved in the array for a period of time
	String data = "temp1=" + String(temp1) + "&temp2=" + String(temp2)
		+ "&temp3=" + String(temp3) + "&hum=" + String(hum) + "&pres=" + String(pres) + "&millis=0";

	// send a POST request to the server
	// change "your/endpoint/name"
	client.println("POST /your/endpoint/name?" + data + " HTTP/1.1");
	client.println("Host: " + String(host));
	client.println("Accept: */*");
	client.println("Content-Type: application/x-www-form-urlencoded");
	client.println("Content-Length: " + data.length());
	client.println();
	client.print(data);

	delay(500);

	//disconnect from the server
	if (client.connected()) {
		client.stop();
	}
}

void setup() {
	// a HC-05 bluetooth module is connected to the serial pins (0, 1)
	// as the device only receives data over bluetooth only the receive 
	// pin can be connected
	// a free transmit pin means that we can still send messages through Serial
	Serial.begin(9600);

	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_NAME, WIFI_PASSWORD);

	//attempt to connect to WiFi for 100 seconds
	long startMillis = millis();
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		if (millis() - startMillis > 100000)
		{
			break;
		}
	}
}


void loop() {
	// if WiFi disconnects, attempt to connect to WiFi for 20 seconds
	// if connection fails, the data received will be saved in the data array
	if (WiFi.status() != WL_CONNECTED)
	{
		WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
		long startMillis = millis();
		while (WiFi.status() != WL_CONNECTED) {
			delay(1000);
			if (millis() - startMillis > 20000)
			{
				break;
			}
		}
	}

	// if there are at least 14 bytes received
	// this number must be modified if you modify the number of values you receive
	if (Serial.available() >= 14)
	{
		///// RECEIVING A VALUE THAT CAN BE NEGATIVE, IN THE RANGE -99.99 : 99.99 /////
		// example: outside temperature
		// to receive a value that can be negative we use 3 bytes
		// 1 for the sign (0 for positive, 1 for negative)
		// 1 for the integer part
		// 1 for the decimal part

		// byte used to read data from Serial (Bluetooth)
		byte b;

		// negTempChar - 1 if the value received is negative, 0 otherwise
		byte negTempByte = Serial.read();

		// the integer part of the value
		// example: for 21.31, it will be 21
		b = Serial.read();
		float temp1 = b;

		// the decimal part of the value
		// example: for 21.31, it will be 31
		b = Serial.read();
		temp1 += (float)b / 100;

		if (negTempByte == 1)
			temp1 = -temp1;

		/////

		///// RECEIVING A POSITIVE VALUE, IN THE RANGE 0 : 99.99 /////
		// example: inside temperature
		b = Serial.read();
		float temp2 = b;

		b = Serial.read();
		temp2 += (float)b / 100;

		/////

		///// RECEIVING A ONE BYTE VALUE /////
		// example: a boolean value from a PIR motion sensor
		b = Serial.read();
		int presence = b;

		/////

		//
		b = Serial.read();
		float temp3 = b;

		b = Serial.read();
		temp3 += (float)b / 100;

		//
		b = Serial.read();
		float hum = b;

		b = Serial.read();
		hum += (float)b / 100;


		///// RECEIVING A POSITIVE VALUE, IN THE RANGE 0 : 999.99 /////
		// example: atmospheric pressure

		// hundreds value
		// example: for 761.22, it will be 7
		b = Serial.read();
		float pres = b;

		// second and third digits of the number
		// example: for 761.22, it will be 61
		b = Serial.read();
		pres = pres * 100 + b;

		// the decimal part of the value
		// example: for 761.22, it will be 22
		b = Serial.read();
		pres += (float)b / 100;

		// a control character, each transmission ends with 192
		b = Serial.read();

		// if the data is valid (the control character was received) we send the post request
		if (b == DATA_END)
		{
			sendPost(temp1, temp2, temp3, hum, pres, millis());
		}

		//if there are any bytes left, clean the input buffer
		while (Serial.available())
		{
			Serial.read();
		}
	}

}



