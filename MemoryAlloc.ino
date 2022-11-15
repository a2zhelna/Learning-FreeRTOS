//Challenge (https://www.digikey.ca/en/maker/projects/introduction-to-rtos-solution-to-part-4-memory-management/6d4dfcaa1ff84f57a2098da8e6401d9c):
// Using FreeRTOS, create two separate tasks. One listens for input over UART (from the Serial Monitor). 
// Upon receiving a newline character (‘\n’), the task allocates a new section of heap memory (using pvPortMalloc()) 
// and stores the string up to the newline character in that section of heap. 
// It then notifies the second task that a message is ready.

// The second task waits for notification from the first task. When it receives that notification, 
// it prints the message in heap memory to the Serial Monitor.
// Finally, it deletes the allocated heap memory (using vPortFree()).


#include <Arduino.h>

static TaskHandle_t task1_handle;
static TaskHandle_t task2_handle;

//Realized it's ok to have a limit on how many chars you can receive
//(This minimizes compute)
static const uint8_t max_len = 255;

//Essential variables used every time the program executes its purpose
//Declaring them static because instead of constantly allocating and deallocating
//stack variables, this gives them one address, which can be accessed quicker
static uint16_t char_count = 0;
static char* heap_address;
static volatile uint8_t notif = 0;
static char current_char;

void task1( void * pvParameters ) {
  while(1) {
    while (notif == 0) {
      if (Serial.available()) {
        current_char = Serial.read();
        //When receiving the first character, allocate some memory
        if (char_count == 0) {
          heap_address = (char*)pvPortMalloc(sizeof(char) * max_len); //FreeRTOS-friendly dynamic memory allocation function
        }
        if (heap_address != NULL) {     //Safety check making sure there was enough heap memory
          if (current_char != '\n' && char_count + 1 < max_len) {
            heap_address[char_count] = current_char;
            char_count++;
          }
          else {      //Once we reach the final character in the serial buffer, notfiy the other task!
            heap_address[char_count] = '\0';
            notif = 1;
            char_count = 0;
          }
        }
      }
    }
  }
}

void task2( void * pvParameters ) {
  while(1) {
    if (notif == 1) {
      Serial.println(heap_address);
      vPortFree(heap_address); //FreeRTOS-friendly memory deallocation function
      notif = 0;
    }
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
    1,
    &task2_handle,
    1
  );
}

void loop() {
  // put your main code here, to run repeatedly:
  vTaskDelete(NULL);
}
