//* Challenge (https://www.digikey.ca/en/maker/projects/introduction-to-rtos-solution-to-part-9-hardware-interrupts/3ae7a68462584e1eb408e1638002e9ed)
// * Your challenge is to create a sampling, processing, and interface system using hardware interrupts 
// * and whatever kernel objects (e.g. queues, mutexes, and semaphores) you might need.
// *
// * You should implement a hardware timer in the ESP32
// * that samples from an ADC pin once every 100 ms. This sampled data should be copied to a 
// * double buffer (you could also use a circular buffer). Whenever one of the buffers is full, 
// * the ISR should notify Task A.
// *
// * Task A, when it receives notification from the ISR, should wake up and compute the average 
// * of the previously collected 10 samples. Note that during this calculation time, the ISR 
// * may trigger again. This is where a double (or circular) buffer will help: you can process 
// * one buffer while the other is filling up.
// *
// * When Task A is finished, it should update a global floating point variable that contains 
// * the newly computed average. Do not assume that writing to this floating point variable will 
// * take a single instruction cycle! You will need to protect that action as we saw in the queue episode.
// * 
// * Task B should echo any characters received over the serial port back to the same serial port. If the 
// * command “avg” is entered, it should display whatever is in the global average variable.

#include <Arduino.h>

static const uint16_t timer_divider = 80; //Max frequency is 80MHz
static const uint64_t timer_max_count = 100000; //When the interrupt occurs
//^^^ Currently, hw timer ticks 1 million times per second, and interrupt occurs every 100k ticks
//(interrupt has 10Hz frequency)

//12 bit embedded ADC pin value
static const int adc_pin = A0;

//Creating my kernel object handles
static TaskHandle_t task1_handle;
static TaskHandle_t task2_handle;


static hw_timer_t *timer = NULL;
static volatile uint16_t value;
static SemaphoreHandle_t binary_sem = NULL;

static SemaphoreHandle_t mutex = NULL;

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

static int circ_buf[10] = {};  
static int buf_idx = 0;
static float global_avg = 0;

void task1( void * pvParameters ) {
  while(1) {
    float loc_avg = 0;
    xSemaphoreTake(binary_sem, portMAX_DELAY);
    for (int i = 0; i < 10; i++) {
      loc_avg += circ_buf[i];
    }
    loc_avg /= 10;
    //Convert binary value to voltage (reference is 3.3V, resolution is 12 bits)
    loc_avg = loc_avg / 4095 * 3.3;
    //global_avg is accessed by both task1 and task2, thus it needs a lock
    xSemaphoreTake(mutex, portMAX_DELAY);
    global_avg = loc_avg;
    xSemaphoreGive(mutex);
  }
}

void task2( void * pvParameters ) {
  while(1) {
    if (Serial.available()) {
      String a = Serial.readStringUntil('\n');
      Serial.println(a);
      if (a == "avg") {
        //Make sure to read global_avg without interrupting task2 writing to global_avg
        xSemaphoreTake(mutex, portMAX_DELAY);
        Serial.println(global_avg);
        xSemaphoreGive(mutex);
      }
    }
  }
}
void IRAM_ATTR onTimer() {

  BaseType_t task_woken = pdFALSE;

  portENTER_CRITICAL_ISR(&spinlock);    //Enter a critical section, using "spinlock" to prevent tasks on other 
                                        //cores from running critical sections locked by that same "spinlock"
                                        //and to disable interrupts

  //CRITICAL SECTION:

  circ_buf[buf_idx] = analogRead(adc_pin);          // * Read ADC value 
  if (buf_idx == 9) { 
    buf_idx = 0;
    xSemaphoreGiveFromISR(binary_sem, &task_woken);   //task_woken is set to true if a task will be re-enabled
                                                      //due to this semaphore being given
  }
  else { buf_idx++; }

  //END CRITICAL SECTION

  portEXIT_CRITICAL_ISR(&spinlock);     //Leave critical section, release "spinlock"

  //Exit from ISR
  if (task_woken) { 
    portYIELD_FROM_ISR(); //This causes scheduler to run immediately after interrupt runs.
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //Create binary semaphore
  binary_sem = xSemaphoreCreateBinary();

  if (binary_sem == NULL) {
    Serial.println("Couldn't create binary semaphore");
    ESP.restart();
  }

  //Create mutex
  mutex = xSemaphoreCreateMutex();

  //Initialize some hardware timer!
  timer = timerBegin(0, timer_divider, true); //Timer #, divider, counts up?
  //"Attach"/attribute an interrupt "function" to this timer
  timerAttachInterrupt(timer, &onTimer, true); //Timer, function, [rising] edge - though docs say only LEVEL trigger is currently supported
  //At what count the ISR should trigger 
  timerAlarmWrite(timer, timer_max_count, true);  //Timer, time, autoreload?
  //Allow it to start triggering
  timerAlarmEnable(timer);

  //Tasks 1 & 2 have same core and priority meaning the scheduler
  //will try to alternate between them every tick
  xTaskCreatePinnedToCore(
    task1,
    "Task 1",
    1024,
    NULL,
    0,        
    &task1_handle,
    1
  );

  xTaskCreatePinnedToCore(
    task2,
    "Task 2",
    1024,
    NULL,
    0,        
    &task2_handle,
    1
  );

  //Delete the setup/loop tasks
  vTaskDelete(NULL);
}

void loop() {
  // put your main code here, to run repeatedly:
}

