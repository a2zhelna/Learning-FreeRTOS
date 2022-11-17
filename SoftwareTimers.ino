//* Challenge (https://www.digikey.ca/en/maker/projects/introduction-to-rtos-solution-to-part-8-software-timers/0f64cf758da440a29476165a5b2e577e):
//* Your challenge is to create an auto-dim feature using the onboard LED. 
//* We’ll pretend that the onboard LED (LED_BUILTIN) is the backlight to an LCD. 
//*
//* Create a task that echoes characters back to the serial terminal (as we’ve done in previous challenges). 
//* When the first character is entered, the onboard LED should turn on. 
//* It should stay on so long as characters are being entered.
//*
//* Use a timer to determine how long it’s been since the last character was entered 
//* (hint: you can use xTimerStart() to restart a timer’s count, even if it’s already running). 
//* When there has been 5 seconds of inactivity, your timer’s callback function should turn off the LED.

#include <Arduino.h>

//Creating my kernel object handles
static TaskHandle_t task1_handle;
static TimerHandle_t timer1_handle;

//Function called when timer expires
void myTimerCallback( TimerHandle_t xTimer ) {
  Serial.print("OFF");
}

void task1( void * pvParameters ) {
  while(1) {
    if (Serial.available()) {
      Serial.read();
      Serial.print("ON");
      xTimerStart(timer1_handle, portMAX_DELAY);  //This function acts as the xTimerReset() function when called before timer expires 
                                                  //hence the "LED" stays on while characters are being entered.
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  timer1_handle = xTimerCreate (
    "Timer 1",
    5000 / portTICK_PERIOD_MS,  //Ticks until timer expires
    pdFALSE,                    //Does timer repeat?
    0,                          //Timer ID
    myTimerCallback             //Timer callback function
  );

  xTaskCreatePinnedToCore(
    task1,
    "Task 1",
    1024,
    NULL,
    0,        
    &task1_handle,
    1
  );

  //Delete the setup/loop tasks
  vTaskDelete(NULL);
}

void loop() {
  // put your main code here, to run repeatedly:
}

