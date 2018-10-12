/********
 * 
 * ******/
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <String.h>
#include <HTTPClient.h>
#include <esp_log.h>
#include <ArduinoJson.h>


//WIFI CONFIG
#define SSID        "loza"
#define PASSWORD    "la123456"

#define TESTMODE
//#define SEND_DATA
//#define DIM_READ
#define SENSOR_READ

#define SENSOR_NUM  6
#define LIGHT_NUM   6

#define COMPARE_SENSOR 29

//LEDC SETUP - ANALOG WRITEE
#define LEDC_TIMER_13_BIT  13
#define LEDC_BASE_FREQ     5000
#define TEST_LIGHT         150



bool overriden;
uint8_t overridenBrightness;

uint32_t callTime = 0;
uint32_t callTime_REQ = 0;

uint8_t sensor_pins[LIGHT_NUM] = {36, 39, 34, 35, 32, 33};


uint8_t light_pins[SENSOR_NUM] = {0, 18, 2, 19, 4, 5};
uint8_t sensor_state[SENSOR_NUM];
uint8_t sensor_fail[SENSOR_NUM] = {0, 0, 0, 0, 0, 0};

uint8_t light_state[LIGHT_NUM][3] = {   {255, 0, 0}, //SET_STATE, CURRENT STATE 0-255, ALWAYS_ON
                                        {255, 0, 0},
                                        {255, 0, 0},
                                        {255, 0, 0},
                                        {255, 0, 0},
                                        {255, 0, 0}
};

void WifiConnect(char ssid[], char pass[]){

    #ifdef TESTMODE
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    #endif//TESTMODE

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    while (WiFi.waitForConnectResult() != WL_CONNECTED) {

        #ifdef TESTMODE
        Serial.println("Connection Failed! Rebooting system in 2 seconds.");
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

void setupPins_light(uint8_t num){
    
    uint8_t COUNTER;

    for(COUNTER = 0; COUNTER < num; COUNTER++){

        ledcSetup(COUNTER, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);

        ledcAttachPin(light_pins[COUNTER], COUNTER);
    }

}

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / valueMax) * _min(value, valueMax);
  // write duty to LEDC
  ledcWrite(channel, duty);
}

void testSensor(){

    uint8_t COUNTER;
    uint8_t sensorValue[SENSOR_NUM];

    for(COUNTER = 0; COUNTER < SENSOR_NUM; COUNTER++){

        sensorValue[COUNTER] = map(analogRead(sensor_pins[COUNTER]), 0, 255, 0, 32);

    }

    for(COUNTER = 0; COUNTER < SENSOR_NUM; COUNTER++){

        Serial.print(COUNTER); Serial.print("|"); Serial.print(sensorValue[COUNTER]); Serial.print("\t"); 

    }

    Serial.println("");

}
uint8_t sendGetRequest(char url[], uint8_t id, uint8_t event){ 
 
    uint32_t httpCode;
    HTTPClient http;

    String ID, EVENT, getData, link;

    ID = String(id);
    EVENT = String(event);

    getData = "?id=" + ID + "&eventId=" + EVENT; 
    link = url + getData;

    Serial.println(link);

    http.begin(link);
 
    http.end(); //Free the resources

    return 1; //GET REQUEST IS OK!
}

String readGetRequest(char url[], uint8_t ID){ 
 
    uint32_t httpCode;
    String payload;
    HTTPClient http;
    DynamicJsonBuffer jsonBuffer;

    Serial.println(url);

    http.begin(url);

    httpCode = http.GET();                             
 
    if (httpCode > 0) { 

        payload = http.getString();

        JsonObject& root = jsonBuffer.parseObject(payload);

        overriden = root["overriden"];
        overridenBrightness = root["overridenBrightness"];

        Serial.println(overriden);
        Serial.println(overridenBrightness);
        Serial.println(payload);

        light_state[ID][0] = overridenBrightness;
        if(overriden == true){ light_state[ID][2] = 1; } else { light_state[ID][2] = 0; }

    }
 
    else {

        #ifdef TESTMODE
        Serial.print("Error on HTTP request: "); Serial.println(httpCode);
        #endif //TESTMODE

        return String(httpCode);
    }
 
    http.end(); //Free the resources

    return payload; //GET REQUEST IS OK!
}

void readSensorData(){

    uint8_t COUNTER;
    uint8_t realValue;

    for(COUNTER = 0; COUNTER < SENSOR_NUM; COUNTER++){

        realValue = map(analogRead(sensor_pins[COUNTER]), 0, 255, 0, 31);

        if(realValue < COMPARE_SENSOR){
            sensor_state[COUNTER] = 1;
        }

        else {
            sensor_state[COUNTER] = 0;
        }
    }
}

void updateLightState(uint8_t ID){
    
    uint8_t COUNTER;
    uint8_t previousValuePLUS = light_state[ID][0];
    uint8_t previousValueMINUS = light_state[ID][0];
    
    if(light_state[ID][2] != 1){
        light_state[ID][1] = light_state[ID][0];
    }

    for(COUNTER = 1; COUNTER <= 2; COUNTER++){

        if((ID + COUNTER) <= 6){
            
            if(light_state[ID + COUNTER][1] + (light_state[ID][0] / 2) <= light_state[ID][0]){
                
                if(light_state[ID + COUNTER][2] != 1){
                    
                    light_state[ID + COUNTER][1] = previousValuePLUS / 2;
                }
                previousValuePLUS /= 2;

            }
        }
        if((ID - COUNTER) >= 0){

            if(light_state[ID + COUNTER][1] + (previousValueMINUS / 2) <= light_state[ID][0]){

                if(light_state[ID + COUNTER][2] != 1){
                    light_state[ID - COUNTER][1] = previousValueMINUS / 2;
                }
                previousValueMINUS /= 2;

            }
        }
    }
    
}

void updateTrafficLight(){
    
    uint8_t COUNTER;
    uint32_t currentTime = millis();

    #ifdef SENSOR_READ
    for(COUNTER = 0; COUNTER < LIGHT_NUM; COUNTER++){
        Serial.print(light_state[COUNTER][1]); Serial.print("\t");
    }
    Serial.println("");
    #endif

    for(COUNTER = 0; COUNTER < LIGHT_NUM; COUNTER++){

        if(light_state[COUNTER][2] != 1){
            ledcAnalogWrite(COUNTER, light_state[COUNTER][1]);
        }

        else {
            ledcAnalogWrite(COUNTER, light_state[COUNTER][0]);
        }

    }

    if(currentTime > callTime + 30){
        for(COUNTER = 0; COUNTER < LIGHT_NUM; COUNTER++){
            if((light_state[COUNTER][1] - 5) >= 5 && light_state[COUNTER][2] != 1 && sensor_state[COUNTER] != 1){
                light_state[COUNTER][1] -= 5;
            } 
        }
        callTime = millis();
    }
}

void updateTrafficState(){

    uint8_t COUNTER;
    uint32_t currentTime;

    currentTime = millis();

    readSensorData();

    for(COUNTER = 0; COUNTER < SENSOR_NUM; COUNTER++){
        if(sensor_state[COUNTER] != 0){

            #ifdef TESTMODE
            //Serial.print("State on device["); Serial.print(COUNTER); Serial.println("] changed!");
            #endif //TESTMODE

            updateLightState(COUNTER);

            #ifdef SEND_DATA
            sendGetRequest("https://simpleidentityloginapi20181011080358.azurewebsites.net/api/station/get-info", COUNTER, 1);
            #endif

        }
    }

    #ifdef DIM_READ
    if(callTime_REQ + 5000 <= currentTime){
        for(COUNTER = 0; COUNTER < LIGHT_NUM; COUNTER++){
            readGetRequest("https://simpleidentityloginapi20181011080358.azurewebsites.net/api/overriden-status", COUNTER);
        }
        callTime_REQ = millis();
    }
    #endif
    
}
void checkForSensorFaliure(){

    uint8_t COUNTER;

    for(COUNTER = 0; COUNTER < LIGHT_NUM; COUNTER++){
        if(sensor_state[COUNTER] == 255 && sensor_fail[COUNTER] != 1){
            sendGetRequest("https://simpleidentityloginapi20181011080358.azurewebsites.net/api/station/get-info", COUNTER, 2);
            Serial.print("Sensor faliure["); Serial.print(COUNTER); Serial.println("] !!!!");
            sensor_fail[COUNTER] = 1;
        }
        else if(sensor_state[COUNTER] != 255){
            sensor_fail[COUNTER] = 0;
        }
    }
}

void testLights(){

    uint8_t COUNTER;

    for(COUNTER = 0; COUNTER < LIGHT_NUM; COUNTER++){
        ledcAnalogWrite(COUNTER, TEST_LIGHT);
    }

    delay(1000);

    for(COUNTER = 0; COUNTER < LIGHT_NUM; COUNTER++){
        ledcAnalogWrite(COUNTER, 0);
    }
}

void setup() {
 
    #ifdef TESTMODE
    Serial.begin(115200);
    #endif //TESTMODE

    setupPins_light(LIGHT_NUM);
    testLights();

    //Connect to wifi network
    //WifiConnect(SSID, PASSWORD);
    delay(1000);
    WiFi.disconnect(true);
    //WifiConnect(SSID, PASSWORD);

}
 
void loop() {

    while(WiFi.status() != WL_CONNECTED){

        checkForSensorFaliure();
        
        updateTrafficLight(); //Update the devices lights

        updateTrafficState(); //Update the state of reading devices

    }

    #ifdef TESTMODE
    Serial.println("Lost internet connection. Rebooting system in 2 seconds.");
    #endif //TESTMODE

    delay(2000);

    ESP.restart();
}