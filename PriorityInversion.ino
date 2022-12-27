// Challenge: https://www.digikey.ca/en/maker/projects/introduction-to-rtos-solution-to-part-11-priority-inversion/abf4b8f7cd4a4c70bece35678d178321
// * Start with the priority inversion code below. If you run it, you’ll find that Task H
// * is blocked for around 5 seconds waiting for a mutex to be released from Task L.
// * Task M interrupts task L for those 5 seconds, causing unbounded priority inversion.
// *
// * Your job is to use the critical section guards to prevent the scheduler from interrupting
// * during the critical section (empty while loop) in Task H and Task L.
// * These guards include portENTER_CRITICAL() and portEXIT_CRITICAL() for the ESP32
// * or taskENTER_CRITICAL() and taskEXIT_CRITICAL() for vanilla FreeRTOS.

// ****************************************************************************************************
// The idea for this solution for priority inversion is to completely disable the scheduler 
// while tasks L does its work
// This way, 
// task H can get blocked by task L, BUT
// task L can't get preempted by task M while doing its work,
// which makes sure that task H doesn't get preempted by task M... no priority inversion

// This solution almost has the same effect as when incorporating the priority ceiling protocol.
// In a priority ceiling solution, when task L is in a critical section its priority 
// changes to match that of task H, where afterwards, the scheduler can schedule
// the "in-critical section" task L and the high priority task H to run "in parallel".
// The difference in this solution is that the scheduler doesn't run while task L does 
// work in the critical section, and thus task H has to wait for this critical section
// to end before it can be scheduled.

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Settings
TickType_t cs_wait = 250;   // Time spent in critical section (ms)
TickType_t med_wait = 5000; // Time medium task spends working (ms)

//Declare spinlock. Initialize it as unlocked
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

//*****************************************************************************
// Tasks

// Task L (low priority)
void doTaskL(void *parameters) {

  TickType_t timestamp;

  // Do forever
  while (1) {
    //Serial.println(configMINIMAL_STACK_SIZE);

    // Take lock
    Serial.println("Task L trying to take lock...");
    timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    portENTER_CRITICAL(&spinlock);  //Enter critical section. Block other tasks from preempting

    // Say how long we spend waiting for a lock
    Serial.print("Task L got lock. Spent ");
    Serial.print((xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp);
    Serial.println(" ms waiting for lock. Doing some");  // Adding any more characters to this string causes a stack overflow... weird..
                                                          // Increasing the task's stack size doesn't fix it. This is the case with 
                                                          // Shawn's solution too.

    // Hog the processor for a while doing nothing
    timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    while ( (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < cs_wait);

    // Release lock
    Serial.println("Task L releasing lock.");

    portEXIT_CRITICAL(&spinlock); //Exit critical section.

    // Go to sleep
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Task M (medium priority)
void doTaskM(void *parameters) {

  TickType_t timestamp;

  // Do forever
  while (1) {

    // Hog the processor for a while doing nothing
    Serial.println("Task M doing some work...");
    timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    while ( (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < med_wait);

    // Go to sleep
    Serial.println("Task M done!");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Task H (high priority)
void doTaskH(void *parameters) {

  TickType_t timestamp;

  // Do forever
  while (1) {

    // Take lock
    Serial.println("Task H trying to take lock...");
    timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Say how long we spend waiting for a lock
    Serial.print("Task H got lock. Spent ");
    Serial.print((xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp);
    Serial.println(" ms waiting for lock. Doing some work...");

    // Hog the processor for a while doing nothing
    timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    while ( (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < cs_wait);

    // Release lock
    Serial.println("Task H releasing lock.");
    
    // Go to sleep
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

//*****************************************************************************
// Main (runs as its own task with priority 1 on core 1)

void setup() {

  // Configure Serial
  Serial.begin(115200);

  // Wait a moment to start (so we don't miss Serial output)
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println();
  Serial.println("---FreeRTOS Priority Inversion Demo---");

  // The order of starting the tasks matters to force priority inversion

  // Start Task L (low priority)
  xTaskCreatePinnedToCore(doTaskL,
                          "Task L",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu);
  
  // Introduce a delay to force priority inversion
  vTaskDelay(1 / portTICK_PERIOD_MS);

  // Start Task H (high priority)
  xTaskCreatePinnedToCore(doTaskH,
                          "Task H",
                          1024,
                          NULL,
                          3,
                          NULL,
                          app_cpu);

  // Start Task M (medium priority)
  xTaskCreatePinnedToCore(doTaskM,
                          "Task M",
                          1024,
                          NULL,
                          2,
                          NULL,
                          app_cpu);

  // Delete "setup and loop" task
  vTaskDelete(NULL);
}

void loop() {
  // Execution should never get here
}
