/* 
 * Reto: Sistema de Infoentretenimiento
 * Código para MBot Mega (ATMega 2560)
 * Autores: Oscar Ortiz Torres A01769292 y Yonathan Romero Amador A01737244
 * Diseño de sistemas en chip (TE2003B)
 * Última fecha de moficación: 12/06/2024
 */

//Librerias y definiciones necesarias
#include <Arduino_FreeRTOS.h>
#include "queue.h"
#include <avr/io.h>
#include <util/delay.h>

#define F_CPU 16000000
#define USART_BAUDRATE 19200
#define UBRR_VALUE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

//Declaración de variables
char RX_Byte;
unsigned char mybuffer[256];

//handle para un queue
QueueHandle_t UARTqueue;

//Declaración de funciones para el movimiento del MBot
void runF();
void runL();
void runR();
void stopM();
void rotateR();
void rotateL();
void runB();
void danceFloor();

void setup(){
  //Serial.begin(19200);
  //Creación de tareas
  xTaskCreate(vUSARTTask,"USART MANAGER TASK",100,NULL,2,NULL);
  xTaskCreate(vMBotSensorsTask,"MBot SENSORS TASK",100,NULL,1,NULL);

  //creación de la queue
  UARTqueue = xQueueCreate(10, sizeof(char));
  if(UARTqueue != NULL){
    //configuración UART
    UBRR2H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR2L = (uint8_t)UBRR_VALUE;
    //UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01); // 8-bit data
    UCSR2C = 0x06;       // Set frame format: 8data, 1stop bit
    UCSR2B |= (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);   // TX and RX enables and RX interrupt enabled
    UCSR2A = 0x00; // Limpia banderas
    sei();
  }

  //Declaración de salidas para el uso de los motores del MBot
  DDRB = 0xFF;
  DDRC = 0xFF;
  DDRG = 0xFF;
  DDRH = 0xFF;

  //Declaración de PWM a 25% de velocidad para los 4 motores
  OCR1A = 100; //PB5 (M1)
  OCR1B = 100; //PB6 (M2)
  OCR4B = 100; //PH4 (M3)
  OCR4C = 100; //PH5 (M4)
  
  TCCR1A = 0b10100001; //COM1A1:0 = 10 (non-inverted), COM1B1:0 = 10 (non-inverted)
  TCCR1B = 0b00001011; //WGM = mode 5, clock = clk/64 (from prescaler)
  TCCR4A = 0b00101001; //COM4B1:0 = 10 (non-inverted), COM4C1:0 = 10 (non-inverted)
  TCCR4B = 0b00001011; //WGM = mode 5, clock = clk/64 (from prescaler)
  
  stopM();

  //Declaración de los pines de los sensores como entrada
  DDRF &= ~(1 << DDF6); // Barrier Sensor Left (Pin 60)
  DDRF &= ~(1 << DDF7); // Barrier Sensor Middle (Pin 61)
  DDRK &= ~(1 << DDK0); // Barrier Sensor Right (Pin 62)
  DDRK &= ~(1 << DDK3); // Collision Sensor Right (Pin 65)
  DDRK &= ~(1 << DDK4); // Collision Sensor Left (Pin 66)
}

//Tarea para procesar el valor recibido por el UART
void vUSARTTask(void *pvParameters){
  char valueReceived;
  BaseType_t qStatus;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
  while(1){
    qStatus = xQueueReceiveFromISR(UARTqueue, &valueReceived, pdTRUE);
    if(qStatus == pdPASS){
      //Serial.println(valueReceived);
      if(valueReceived == 48){ //0
        stopM();
      }else if(valueReceived == 49){ //1
        runF();
        _delay_ms(1000);
        stopM();
      }else if(valueReceived == 50){ //2
        runB();
        _delay_ms(1000);
        stopM();
      }else if(valueReceived == 51){ //3
        runL();
        _delay_ms(1000);
        stopM();
      }else if(valueReceived == 52){ //4
        runR();
        _delay_ms(1000);
        stopM();
      }else if(valueReceived == 53){ //5
        rotateL();
        _delay_ms(770);
        stopM();
      }else if(valueReceived == 54){ //6
        rotateR();
        _delay_ms(770);
        stopM();
      }else if(valueReceived == 55){ //7
        danceFloor();
        _delay_ms(4000);
        stopM();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void vMBotSensorsTask(void *pvParameters){
  TickType_t xLastWakeTIme;
  while(1){
    if (!(PINF & (1 << PINF6))){  //Barrier Sensor Left
      sprintf(mybuffer, "\n3");
      USART_Transmit_String((unsigned char *)mybuffer);
    }
    if (!(PINF & (1 << PINF7))){  //Barrier Sensor Middle
      sprintf(mybuffer, "\n18");
      USART_Transmit_String((unsigned char *)mybuffer);
    }
    if (!(PINK & (1 << PINK0))){  //Barrier Sensor Right
      sprintf(mybuffer, "\n2");
      USART_Transmit_String((unsigned char *)mybuffer);
    }
    if (!(PINK & (1 << PINK3))){  //Collision Sensor Left
      sprintf(mybuffer, "\n0");
      USART_Transmit_String((unsigned char *)mybuffer);
    }
    if (!(PINK & (1 << PINK4))){  //Collision Sensor Right
      sprintf(mybuffer, "\n1");
      USART_Transmit_String((unsigned char *)mybuffer);
    }
    
    xTaskDelayUntil(&xLastWakeTIme,(500/portTICK_PERIOD_MS));   //Retardo para tarea periodica
  }
}

ISR(USART2_RX_vect){
  //lee dato recibido
  RX_Byte = UDR2;
  //Envia el dato recibido
  xQueueSendToBackFromISR(UARTqueue, &RX_Byte, pdTRUE);
}

//////////funciones de transmisión del UART///////////////
void USART_Transmit(unsigned char data){
  //wait for empty transmit buffer
  while(!(UCSR2A & (1 << UDRE0)));
  //put data into buffer, send data
  UDR2 = data;
}
void USART_Transmit_String(unsigned char * pdata){
  unsigned char i;
  //calculate string length
  unsigned char len = strlen(pdata);
  //transmit byte for byte
  for(i=0; i < len; i++)
  {
    //wait for empty transmit buffer
    while(!(UCSR2A & (1 << UDRE0)));
    //put data into buffer, send data
    UDR2 = pdata[i];
  }
}
//////////////////////////////////////////////////////////////

void stopM(){
  //Serial.println(0);
  //M1 (1A)
  PORTB |= (1<<PB5); //PWM
  PORTC &= ~(1<<PC4); //I1
  PORTC &= ~(1<<PC5); //I2
  //M2 (1B)
  PORTB |= (1<<PB6); //PWM
  PORTC &= ~(1<<PC2); //I1
  PORTC &= ~(1<<PC3); //I2
  //M3 (2A)
  PORTH |= (1<<PH4); //PWM
  PORTG &= ~(1<<PG0); //I1
  PORTG &= ~(1<<PG1); //I2
  //M4 (2B)
  PORTH |= (1<<PH5); //PWM
  PORTC &= ~(1<<PC0); //I1
  PORTC &= ~(1<<PC1); //I2
}

void runB(){
  //Serial.println(2);
  //M1 (1A)
  PORTB |= (1<<PB5); //PWM
  PORTC |= (1<<PC4); //I1
  PORTC &= ~(1<<PC5); //I2
  //M2 (1B)
  PORTB |= (1<<PB6); //PWM
  PORTC |= (1<<PC2); //I1
  PORTC &= ~(1<<PC3); //I2
  //M3 (2A)
  PORTH |= (1<<PH4); //PWM
  PORTG |= (1<<PG0); //I1
  PORTG &= ~(1<<PG1); //I2
  //M4 (2B)
  PORTH |= (1<<PH5); //PWM
  PORTC |= (1<<PC0); //I1
  PORTC &= ~(1<<PC1); //I2
  //_delay_ms(1000);
  //stopM();
}

void runF(){
  //Serial.println(1);
  //M1 (1A)
  PORTB |= (1<<PB5); //PWM
  PORTC &= ~(1<<PC4); //I1
  PORTC |= (1<<PC5); //I2
  //M2 (1B)
  PORTB |= (1<<PB6); //PWM
  PORTC &= ~(1<<PC2); //I1
  PORTC |= (1<<PC3); //I2
  //M3 (2A)
  PORTH |= (1<<PH4); //PWM
  PORTG &= ~(1<<PG0); //I1
  PORTG |= (1<<PG1); //I2
  //M4 (2B)
  PORTH |= (1<<PH5); //PWM
  PORTC &= ~(1<<PC0); //I1
  PORTC |= (1<<PC1); //I2
  //_delay_ms(1000);
  //stopM();
}

void runR(){
  //Serial.println(4);
  //M1 (1A)
  PORTB |= (1<<PB5); //PWM
  PORTC |= (1<<PC5); //I2
  PORTC &= ~(1<<PC4); //I1
  //M2 (1B)
  PORTB |= (1<<PB6); //PWM
  PORTC |= (1<<PC2); //I1
  PORTC &= ~(1<<PC3); //I2
  //M3 (2A)
  PORTH |= (1<<PH4); //PWM
  PORTG |= (1<<PG1); //I2
  PORTG &= ~(1<<PG0); //I1
  //M4 (2B)
  PORTH |= (1<<PH5); //PWM
  PORTC |= (1<<PC0); //I1
  PORTC &= ~(1<<PC1); //I2
  //_delay_ms(1000);
  //stopM();
}

void runL(){
  //Serial.println(3);
  //M1 (1A)
  PORTB |= (1<<PB5); //PWM
  PORTC |= (1<<PC4); //I1
  PORTC &= ~(1<<PC5); //I2
  //M2 (1B)
  PORTB |= (1<<PB6); //PWM
  PORTC |= (1<<PC3); //I2
  PORTC &= ~(1<<PC2); //I1
  //M3 (2A)
  PORTH |= (1<<PH4); //PWM
  PORTG |= (1<<PG0); //I1
  PORTG &= ~(1<<PG1); //I2
  //M4 (2B)
  PORTH |= (1<<PH5); //PWM
  PORTC |= (1<<PC1); //I2
  PORTC &= ~(1<<PC0); //I1
  //_delay_ms(1000);
  //stopM();
}



void rotateR(){
  //Serial.println(6);
  //M1 (1A)
  PORTB |= (1<<PB5); //PWM
  PORTC |= (1<<PC5); //I2
  PORTC &= ~(1<<PC4); //I1
  //M2 (1B)
  PORTB |= (1<<PB6); //PWM
  PORTC |= (1<<PC3); //I2
  PORTC &= ~(1<<PC2); //I1
  //M3 (2A)
  PORTH |= (1<<PH4); //PWM
  PORTG |= (1<<PG0); //I1
  PORTG &= ~(1<<PG1); //I2
  //M4 (2B)
  PORTH |= (1<<PH5); //PWM
  PORTC |= (1<<PC0); //I1
  PORTC &= ~(1<<PC1); //I2
  //_delay_ms(1000);
  //stopM();
}

void rotateL(){
  //Serial.println(5);
  //M1 (1A)
  PORTB |= (1<<PB5); //PWM
  PORTC |= (1<<PC4); //I1
  PORTC &= ~(1<<PC5); //I2
  //M2 (1B)
  PORTB |= (1<<PB6); //PWM
  PORTC |= (1<<PC2); //I1
  PORTC &= ~(1<<PC3); //I2
  //M3 (2A)
  PORTH |= (1<<PH4); //PWM
  PORTG |= (1<<PG1); //I2
  PORTG &= ~(1<<PG0); //I1
  //M4 (2B)
  PORTH |= (1<<PH5); //PWM
  PORTC |= (1<<PC1); //I2
  PORTC &= ~(1<<PC0); //I1
  //_delay_ms(1000);
  //stopM();
}



void danceFloor(){
  //Serial.println(7);
  rotateL();
  _delay_ms(100);
  rotateR();
  _delay_ms(100);
  rotateL();
  _delay_ms(500);
  rotateR();
  _delay_ms(500);
  rotateL();
  _delay_ms(100);
  rotateR();
  _delay_ms(200);
  rotateL();
  _delay_ms(100);
}

void loop(){}
