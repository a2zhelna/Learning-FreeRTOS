//* Challenge (https://www.digikey.ca/en/maker/projects/introduction-to-rtos-solution-to-part-10-deadlock-and-starvation/872c6a057901432e84594d79fcb2cc5d)
// * Your challenge is to solve the dining philosophers problem using the demo code found here ^

//When you run this program on your ESP32, you should see it immediately enter into a deadlock state. Implement both the “hierarchy” solution and the “arbitrator” solution to solve it.
//Note that the "eat" task function does not loop forever. If deadlock can be avoided, all tasks will delete themselves, and you should see a "Done! No deadlock occurred!" message appear in the serial terminal.

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Settings
enum { NUM_TASKS = 5 };           // Number of tasks (philosophers)
enum { TASK_STACK_SIZE = 2048 };  // Bytes in ESP32, words in vanilla FreeRTOS

// Globals
static SemaphoreHandle_t bin_sem;   // Wait for parameters to be read
static SemaphoreHandle_t done_sem;  // Notifies main task when done
static SemaphoreHandle_t chopstick[NUM_TASKS];

//*****************************************************************************
// Tasks

// The only task: eating
void eat(void *parameters) {

  int num;
  int right_num;
  char buf[50];

  // Copy parameter and increment semaphore count (cast to integer pointer and get value at address)
  num = *(int *)parameters;
  if (num == 4) {
    right_num = 0;
  } else {
    right_num = num + 1;
  }
  xSemaphoreGive(bin_sem);

  //Figure out which adjacent chopstick is lower/higher numbered
  char low_num;
  char high_num;

  if (num > right_num) {
    low_num = right_num;
    high_num = num;
  } else {
    low_num = num;
    high_num = right_num;
  }


  //Hierarchical solution:
  //A philosopher can pick up an adjacent lower-numbered chopstick, and 
  //then only after that he can pick up the higher-numbered one, and eat.

  // Take lower-numbered chopstick
  xSemaphoreTake(chopstick[low_num], portMAX_DELAY);
  sprintf(buf, "Philosopher %i took chopstick %i", num, low_num);
  Serial.println(buf);

  // Add some delay to force deadlock
  vTaskDelay(1 / portTICK_PERIOD_MS);

  // Take higher-numbered chopstick (only after lower-numbered one has been picked up)
  xSemaphoreTake(chopstick[high_num], portMAX_DELAY);
  sprintf(buf, "Philosopher %i took chopstick %i", num, high_num);
  Serial.println(buf);

  // Do some eating
  sprintf(buf, "Philosopher %i is eating", num);
  Serial.println(buf);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  // Put down high-numbered chopstick
  xSemaphoreGive(chopstick[high_num]);
  sprintf(buf, "Philosopher %i returned chopstick %i", num, high_num);
  Serial.println(buf);

  // Put down low-numbered chopstick
  xSemaphoreGive(chopstick[low_num]);
  sprintf(buf, "Philosopher %i returned chopstick %i", num, low_num);
  Serial.println(buf);

  //Not quite sure why the mutexes/chopsticks are recommended to be given back in this, reverse order of acquisition.
  //(In Shawn Hymel's "Introduction to RTOS Part 10" at 9:05)
  //I believe that since every task follows the same order of acquisition, there are no inconsistencies
  //and no ways for a task to be locked while relesing the mutexes it took.

  // Notify main task and delete self
  xSemaphoreGive(done_sem);
  vTaskDelete(NULL);
}

//*****************************************************************************
// Main (runs as its own task with priority 1 on core 1)

void setup() {

  char task_name[20];

  // Configure Serial
  Serial.begin(115200);

  // Wait a moment to start (so we don't miss Serial output)
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println();
  Serial.println("---FreeRTOS Dining Philosophers Challenge---");

  // Create kernel objects before starting tasks
  bin_sem = xSemaphoreCreateBinary();

  done_sem = xSemaphoreCreateCounting(NUM_TASKS, 0);
  for (int i = 0; i < NUM_TASKS; i++) {
    chopstick[i] = xSemaphoreCreateMutex();
  }



  // Have the philosphers start eating
  for (int i = 0; i < NUM_TASKS; i++) {
    sprintf(task_name, "Philosopher %i", i);
    xTaskCreatePinnedToCore(eat,
                            task_name,
                            TASK_STACK_SIZE,
                            (void *)&i,
                            1,
                            NULL,
                            app_cpu);
    xSemaphoreTake(bin_sem, portMAX_DELAY);
  }



  // Wait until all the philosophers are done
  for (int i = 0; i < NUM_TASKS; i++) {
    xSemaphoreTake(done_sem, portMAX_DELAY);
  }

  // Say that we made it through without deadlock
  Serial.println("Done! No deadlock occurred!");
}

void loop() {
  // Do nothing in this task
}
