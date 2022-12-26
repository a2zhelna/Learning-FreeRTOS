//* Challenge: (https://www.digikey.ca/en/maker/projects/introduction-to-rtos-solution-to-part-10-deadlock-and-starvation/872c6a057901432e84594d79fcb2cc5d)

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

static SemaphoreHandle_t arbitrator_mtx;

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

  //Wait for the arbitrator mutex if another philosopher is currently
  //picking up / putting down chop sticks or eating.
  xSemaphoreTake(arbitrator_mtx, portMAX_DELAY);

  // Take left chopstick
  xSemaphoreTake(chopstick[num], portMAX_DELAY);
  sprintf(buf, "Philosopher %i took chopstick %i", num, num);
  Serial.println(buf);

  // Add some delay to force deadlock
  // This is due to all the other philosopher's picking up their left chopsticks,
  // leaving no right chopsticks to pick up
  vTaskDelay(1 / portTICK_PERIOD_MS);

  // Take right chopstick
  // The deadlock occurs here. The philosophers wait forever to pick this up.
  xSemaphoreTake(chopstick[right_num], portMAX_DELAY);
  sprintf(buf, "Philosopher %i took chopstick %i", num, right_num);
  Serial.println(buf);

  // Do some eating
  sprintf(buf, "Philosopher %i is eating", num);
  Serial.println(buf);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  // Put down right chopstick
  xSemaphoreGive(chopstick[right_num]);
  sprintf(buf, "Philosopher %i returned chopstick %i", num, right_num);
  Serial.println(buf);

  // Put down left chopstick
  xSemaphoreGive(chopstick[num]);
  sprintf(buf, "Philosopher %i returned chopstick %i", num, num);
  Serial.println(buf);

  //"Tell arbitrator" you are done with the chopsticks
  //This makes an arbitrary task unblock and do the 
  //stuff with the chopsticks.
  xSemaphoreGive(arbitrator_mtx);

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

  //Create the "arbitrator mutex"
  //This locks the pick up / eat / drop actions of the philosophers,
  //so only the philosopher which has the mutex will be able to perform those actions,
  //and no deadlocking will occur.
  arbitrator_mtx = xSemaphoreCreateMutex();

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
