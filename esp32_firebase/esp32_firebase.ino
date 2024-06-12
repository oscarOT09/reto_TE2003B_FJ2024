/*
 * Reto: Sistema de Infoentretenimiento
 * Código para ESP32
 * Autores: Oscar Ortiz Torres A01769292 y Yonathan Romero Amador A01737244
 * Diseño de sistemas en chip (TE2003B)
 * Última fecha de moficación: 12/06/2024
 */

//Librerias necesarias para internet y Firebase
#include <WiFi.h>
#include <Keypad.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "Arduino.h"
#include "addons/RTDBHelper.h"

//Definiciones de pines para el uso de UART2
#define RXD2 16
#define TXD2 17

//Definición de filas y columnas del teclado matricial a usar
#define ROW_NUM     4
#define COLUMN_NUM  4

//Matriz de caracteres del teclado matricial
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

//Definiciones de pines de conexión física del teclado matricial
byte pin_rows[ROW_NUM]      = {13, 12, 14, 27}; // GPIO13, GPIO12, GPIO14, GPIO27 connect to the row pins
byte pin_column[COLUMN_NUM] = {26, 25, 33, 32};   // GPIO26, GPIO25, GPIO33, GPI32 connect to the column pins

//Mapeo de pines y caracteres
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

//API del proyecto de Firebase
#define API_KEY "AIzaSyDrw7n_MdXaYChhVK7LF3vLuObCytTL1jc"
//URL de base de datos en tiempo real
#define DATABASE_URL "https://esp32-soc-default-rtdb.firebaseio.com"

//Datos de objeto Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//Datos de acceso WiFi
char ssid[] = "VICTUS 8582";
char pass[] = "7177L5-j";

//Declaración de variables
unsigned long prevMillis = 0;
bool signupOK = false;
char key;
int repFlag = -1,
    mbotFlag = -1,
    //numMode = 0,
    contC = 0,
    musicNum1,
    mbotControl,
    intValue,
    inputFlag;
String uartInput;

void setup(){
  Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);   //Inicio de Serial2 para comunicación con el MBot
  
  //Definiciones de pines de entrada para LEDs
  pinMode(19, OUTPUT);   //LED Verde
  pinMode(21, OUTPUT);  //LED Rojo
  pinMode(22, OUTPUT);  //LED Azul
  
  Serial.begin(19200);  //Inicio del puerto Serial

  //Impresión de SSID a conectar
  Serial.print("Connecting to ");
  Serial.println(ssid);

  Serial.println("Connecting to Wi-Fi...");
  // Loop para lograr una conexión exitosa
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    delay(5000);
    Serial.println( WiFi.status() ); 
  }
  Serial.println( "Connected to Wi-Fi." );  

  //Asignación de credenciales de la base de datos en tiempo real
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Connected to Firebase");
    signupOK = true;
  }else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("System ready");
  digitalWrite(19, HIGH);  //Encendido de LED verde para indicar la finalización de sección "setup"
}

void loop(){
  //Monitoreo de entrada del teclado matricial
  key = keypad.getKey();

  //Control de la variable repFlag con la tecla de entrada
  if(key == 'A' || key == 'B' || key == 'C' || key == 'D'){
    if(key == 'A'){   //Play
      repFlag = 1;
    }else if(key == 'B'){   //Pause
      repFlag = 0;
    }else if(key == 'C'){   //Next
      repFlag = 2;
    }else if(key == 'D'){   //Past
      repFlag = 3;
    }  
  }

  //Envio de un número de 3 dígitos al presionar '#'
  if(key == '#'){
    char musicNum[4];
    digitalWrite(22, HIGH);   //Encendido del LED azul para indicar el modo de envio de número  
    while(contC < 3){
      key = keypad.getKey();
      if (key) {
        musicNum[contC] = key; // Almacenado del caracter en el vector
        contC++;
      }
    }
    contC = 0;    //Reinicio del contador "contC"
    musicNum[3] = '\0';
    musicNum1 = atoi(musicNum);   //Conversión a dato entero
    Serial.println(musicNum1);
  }

  //Control manual del MBot con el teclado matricial
  if(key == '*'){
    key = 'F';
    Serial.println("Modo MBot");
    digitalWrite(21, HIGH);
    Serial.println("LED ROJO");
    
    while(key != '*'){
      key = keypad.getKey();
      //Serial.println(key);
      
      if((key == '2') || (key == '8') || (key == '4') || (key == '6') || (key == '1') || (key == '3') || (key == '0')){
        mbotControl = key - '0';
        Serial.println(mbotControl);
        if(mbotControl == 0 ){
          mbotFlag = 7;
        }else if(mbotControl == 1 ){
          mbotFlag = 5;
        }else if(mbotControl == 2 ){
          mbotFlag = 1;
        }else if(mbotControl == 3 ){
          mbotFlag = 6;
        }else if(mbotControl == 4 ){
          mbotFlag = 3;
        }else if(mbotControl == 6 ){
          mbotFlag = 4;
        }else if(mbotControl == 8 ){
          mbotFlag = 2;
        }
        Serial2.print(mbotFlag);
        mbotFlag = -1;
      }
    }
    digitalWrite(21, LOW);    //Apagado de LED rojo para indicar salida del modo de control manual
  }
  
  if(Firebase.ready() && signupOK && ((millis() - prevMillis > 1000) || prevMillis == 0)){
    if(repFlag != -1){
      if(Firebase.RTDB.setInt(&fbdo, "music1/flag", repFlag)){
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
      }else{
        Serial.println("FAILED");
      }
      repFlag = -1;
      inputFlag = 0;
    }

    if(Firebase.RTDB.getInt(&fbdo,"mbot1/flag")){
      mbotFlag = fbdo.intData();
      if(mbotFlag != -1){
        
        Serial2.print(mbotFlag);
        if(Firebase.RTDB.setInt(&fbdo, "mbot1/flag", -1)){
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
        }else{
          Serial.println("FAILED");
        }
      }
      mbotFlag = -1;
    }

    if(musicNum1 != 0){
      if(Firebase.RTDB.setInt(&fbdo, "music1/num", musicNum1)){
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
      }else{
        Serial.println("FAILED");
      }
      musicNum1 = 0;
      inputFlag = 0;
      digitalWrite(22, LOW);
    }
    prevMillis = millis();   
  }

  if((Serial2.available() > 0)){        //Comprueba si hay datos nuevos disponibles en Serial2
    if(inputFlag == 0){
      //Serial.print("Sensor: ");
      uartInput = Serial2.readString();   //Lectura del dato
      uartInput.trim();                   //Filtra el dato, removiendo cualquier \r \n al final de la lectura
      Serial.println(uartInput);
      
      intValue = uartInput.toInt();       //Conversión del dato recibido a una varible entera
      Serial.print("Converted to int: ");
      Serial.println(intValue);

      if(intValue <=3 ){
        repFlag = intValue;
      }else{
        musicNum1 = random(1,100);
      }
      inputFlag = 1;
    }
  }
}
