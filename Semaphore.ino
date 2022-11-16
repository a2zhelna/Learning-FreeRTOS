/**
 * FreeRTOS Counting Semaphore Challenge
 * 
 * Challenge: use a mutex and counting semaphores to protect the shared buffer 
 * so that each number (0 throguh 4) is printed exactly 3 times to the Serial 
 * monitor (in any order). Do not use queues to do this!
 * 
 * Hint: you will need 2 counting semaphores in addition to the mutex, one for 
 * remembering number of filled slots in the buffer and another for 
 * remembering the number of empty slots in the buffer.
 * 
 * Date: January 24, 2021
 * Author: Shawn Hymel
 * License: 0BSD
 */
//https://www.digikey.ca/en/maker/projects/introduction-to-rtos-solution-to-part-7-freertos-semaphore-example/51aa8660524c4daba38cba7c2f5baba7

// You'll likely need this on vanilla FreeRTOS
//#include semphr.h

//Using a queue:
//If the queue isn't full, add data to the queue,
//If the queue has at least one piece of data, take it from the queue and print it.

//A queue automatically makes it so no other task can add its data to the queue
//while it adds data (this behaviour can be replicated with a mutex to give
//ownership to the critical section, giving it sole permission to control some shared resource).
//A queue also doesn't allow to data to be taken when there's nothing in it and doesn't allow
//data to be added if it's full. So, using semaphores, we can permit tasks to attempt adding/removing
//data depending on what the value of the counting semaphore is.
//E.G. If a task wants to take: it attempts to take from the counting semaphore and if its count suffices, data is removed, 
//and the counting semaphore is decremented. 

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Settings
enum {BUF_SIZE = 5};                  // Size of buffer array
static const int num_prod_tasks = 5;  // Number of producer tasks
static const int num_cons_tasks = 2;  // Number of consumer tasks
static const int num_writes = 3;      // Num times each producer writes to buf

// Globals
static int buf[BUF_SIZE];             // Shared buffer
static int head = 0;                  // Writing index to buffer
static int tail = 0;                  // Reading index to buffer
static SemaphoreHandle_t bin_sem;     // Waits for parameter to be read


//My semaphores                        
static SemaphoreHandle_t empty_sem;    
static SemaphoreHandle_t full_sem;     
//My mutex                
//The processes of reading and writing data 
//are mutually exclusive, and you can only
//read/write with one task at a time.
//Thus this mutex lock will be used on both
//tasks that write, and tasks that read             
static SemaphoreHandle_t mutex;        

//*****************************************************************************
// Tasks

// Producer: write a given number of times to shared buffer
void producer(void *parameters) {

  // Copy the parameters into a local variable
  int num = *(int *)parameters;

  // Release the binary semaphore
  xSemaphoreGive(bin_sem);

  // Fill shared buffer with task number
  for (int i = 0; i < num_writes; i++) {
    //Here, we have to take from a semaphore since when giving, there is no delay.
    //(An original idea I had was to only give and take from full_sem to determine
    //which task should actually do something in the moment, but when you give,
    //the OS can't wait until there's enough room in the counting semaphore to
    //add to it - it only waits until there's enough count to give away to a task)
    xSemaphoreTake(empty_sem, portMAX_DELAY) ;
    
    // Putting mutex in the loop since the task number isn't printed in a particular order
    // (Other tasks can preempt the current running task after, or before, one loop occurs)
    // Critical section (accessing shared buffer)
    xSemaphoreTake(mutex, portMAX_DELAY);
    buf[head] = num;                    //If a tick starts immediately after this line, the head value will not be updated
                                        //and the next task that runs will overwrite this num value in the buffer. 
                                        //THUS we want to remove the possibility of this happening (its a critical section)
    head = (head + 1) % BUF_SIZE;
    xSemaphoreGive(mutex);
    //This goes last to finally tell the other "consumer" tasks that its ok to read the buffer
    xSemaphoreGive(full_sem);
  }
  // Delete self task (after sending its values to the buffer)
  vTaskDelete(NULL); 
}

// Consumer: continuously read from shared buffer
void consumer(void *parameters) {

  int val;

  // Read from buffer
  while (1) {
    xSemaphoreTake(full_sem, portMAX_DELAY);
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    // Critical section (accessing shared buffer and Serial)
    val = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    Serial.println(val);
    xSemaphoreGive(mutex);
    //This finally tells the "producer" tasks that its ok to fill up the buffer (a spot has been freed)
    xSemaphoreGive(empty_sem);
  }
}

//*****************************************************************************
// Main (runs as its own task with priority 1 on core 1)

void setup() {

  char task_name[12];
  
  // Configure Serial
  Serial.begin(115200);

  // Wait a moment to start (so we don't miss Serial output)
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println();
  Serial.println("---FreeRTOS Semaphore Alternate Solution---");

  // Create mutexes and semaphores before starting tasks
  bin_sem = xSemaphoreCreateBinary();

  //Semaphore counting the filled slots
  full_sem = xSemaphoreCreateCounting(5, 0);
  //Semaphore counting the empty slots
  empty_sem = xSemaphoreCreateCounting(5, 5);

  mutex = xSemaphoreCreateMutex();

  // Start producer tasks (wait for each to read argument)
  for (int i = 0; i < num_prod_tasks; i++) {
    sprintf(task_name, "Producer %i", i);
    xTaskCreatePinnedToCore(producer,
                            task_name,
                            1024,
                            (void *)&i,
                            1,
                            NULL,
                            app_cpu);
    xSemaphoreTake(bin_sem, portMAX_DELAY);
  }

  // Start consumer tasks
  for (int i = 0; i < num_cons_tasks; i++) {
    sprintf(task_name, "Consumer %i", i);
    xTaskCreatePinnedToCore(consumer,
                            task_name,
                            1024,
                            NULL,
                            1,
                            NULL,
                            app_cpu);
  }

  // Notify that all tasks have been created
  Serial.println("All tasks created");
}

void loop() {
  
  // Do nothing but allow yielding to lower-priority tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}