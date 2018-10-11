/********
 * 
 * ******/
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <String.h>
#include <HTTPClient.h>

//WIFI CONFIG
#define SSID        "Redmi"
#define PASSWORD    "11221122"

#define TESTMODE

#define SENSOR_NUM  10
#define LIGHT_NUM   10

uint8_t light_pins[LIGHT_NUM] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

uint8_t sensor_pins[SENSOR_NUM] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void WifiConnect(char ssid[], char pass[]){

    #ifdef TESTMODE
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    #endif//TESTMODE

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    while (WiFi.waitForConnectResult() != WL_CONNECTED) {

        #ifdef TESTMODE
        Serial.println("Connection Failed! Rebooting...");
        #endif//TESTMODE

        //Wait 2 seconds before restarting the device
        delay(2000);

        ESP.restart();
    }

    #ifdef TESTMODE
    Serial.println ( "" );
    Serial.print ( "Connected to " ); Serial.println ( ssid );
    Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );
    #endif //TEST_MODE

}

void setupPins_sensor(uint8_t num){
    
    uint8_t COUNTER;

    for(COUNTER = 0; COUNTER < num; COUNTER++){
        pinMode(sensor_pins[COUNTER], OUTPUT);
    }
}

void setupPins_light(uint8_t num){
    
    uint8_t COUNTER;

    for(COUNTER = 0; COUNTER < num; COUNTER++){
        pinMode(light_pins[COUNTER], INPUT);
    }

}

uint8_t sendGetRequest(char url[], uint8_t par_1 , uint8_t par_2 ){ 
 
    uint32_t httpCode;
    String payload;
    HTTPClient http;

    String STATE, STATION, getData, link;

    STATE = String(par_1);
    STATION = String(par_2);

    getData = "?id=" + STATION + "&test=" + STATE; 
    link = url + getData;

    Serial.println(link);

    http.begin(link);

    httpCode = http.GET(); 

    Serial.println(httpCode);                             
 
    if (httpCode > 0) { 

        payload = http.getString();

        #ifdef TESTMODE
        Serial.println(httpCode);
        Serial.println(payload);
        #endif //TESTMODE

        }
 
    else {

        #ifdef TESTMODE
        Serial.print("Error on HTTP request: "); Serial.println(httpCode);
        #endif //TESTMODE

        return 0; //ERROR ON HTTP REQUEST
    }
 
    http.end(); //Free the resources

    return 1; //GET REQUEST IS OK!
}

void setup() {
 
    #ifdef TESTMODE
    Serial.begin(115200);
    #endif //TESTMODE

    //Connect to wifi network
    WifiConnect(SSID, PASSWORD);

    setupPins_light(SENSOR_NUM);
    setupPins_sensor(LIGHT_NUM);
}
 
void loop() {
    while(WiFi.status() == WL_CONNECTED){
        sendGetRequest("https://simpleidentityloginapi20181011080358.azurewebsites.net/api/station/get-info", 1, 9);
        delay(5000);
    }
}