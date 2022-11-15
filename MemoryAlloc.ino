#include <Arduino.h>

static TaskHandle_t task1_handle;
static TaskHandle_t task2_handle;

static char* heap_address;

void task1( void * pvParameters ) {
  while(1) {
    if (Serial.available()) {
      String strong = Serial.readStringUntil('\n');
      strong.trim();

      int len = strong.length();

      char *ptr = (char*)pvPortMalloc(sizeof(char) * (len + 1));
      ptr[len] = '\0';

      heap_address = ptr;

      for (int i = 0; i < len; i++)
      {
        ptr[i] = strong[i];
      }
      //vTaskDelay(100 / portTICK_PERIOD_MS);
      vTaskResume(task2_handle);
    }
  }
}

void task2( void * pvParameters ) {
  while(1) {
    if (heap_address != NULL) {
      for (int i = 0; heap_address[i] != '\0'; i++)
      {
        Serial.print(heap_address[i]); 
      }
      vPortFree(heap_address);
    }
    vTaskSuspend(NULL);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  xTaskCreatePinnedToCore(
    task1,
    "Task 1",
    1024,
    NULL,
    1,
    &task1_handle,
    1
  );
  xTaskCreatePinnedToCore(
    task2,
    "Task 2",
    1024,
    NULL,
    2,            //It seems that allocating memory (specifically, assigning an address to some variable), in a task sometimes 
                  //makes it so a task with an equal core & priority doesn't see the updated address. 
                  //(Adding a delay before going to the task which reads the address lets the task successfully read it) 
                  //(Setting the priority of the "address reading" task higher than that of the "assigner" task makes everything work fine)
                  //Shawn Hymel, Intro to RTOS Part 5, 8:15, "Its generally a good idea to assign one hardware periferal per task" - meaning you should have one task for all Serial.print functions
    &task2_handle,
    1
  );
}

void loop() {
  // put your main code here, to run repeatedly:
  vTaskSuspend(task2_handle);
  vTaskDelete(NULL);
}
