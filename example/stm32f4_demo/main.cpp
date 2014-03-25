/*
    Demo for experimental thread-safe RiteVM with FreeRTOS on STM32F4Discovery

    multi-thread support on the RiteVM:
    https://github.com/mruby/mruby/issues/1657

    Tasks(Threads) in this demo.
     -main task : main task. Spawn other tasks.
     -C task1   : blinking LED(green)
     -C task2   : blinking LED(orange)
     -Ruby task1: call into mruby, blinking LED(red).
     -Ruby task2: call into mruby, blinking LED(blue).

     Ruby task1/2 share one VM(mrb_state).

    Check:
        1. Two C task works.
        2. Two Ruby task works and does not stuck or broken.

*/
#include <stdlib.h>

#include "wirish.h"
#include "MapleFreeRTOS.h"

#include <errno.h>
extern "C" {
#include <signal.h>
}
#include <string.h>


#include "mruby.h"
#include "mruby/class.h"
#include "mruby/value.h"
#include "mruby/irep.h"
#include "mruby/string.h"

#define LED_GREEN  Port2Pin('D',12)
#define LED_ORANGE Port2Pin('D',13)
#define LED_RED    Port2Pin('D',14)    
#define LED_BLUE   Port2Pin('D',15)

const int LED_C1 = LED_GREEN;       //LED controlled by C Task1
const int LED_C2 = LED_ORANGE;      //LED controlled by C Task2
const int LED_R1 = LED_RED;         //LED controlled by Ruby Task1
const int LED_R2 = LED_BLUE;        //LED controlled by Ruby Task2

char *const CCM_RAM_BASE = (char *)0x10000000;
char *g_ccm_heap_next;

const unsigned portBASE_TYPE TASK_PRIORITY_NORMAL = tskIDLE_PRIORITY + 2;
const unsigned portBASE_TYPE TASK_PRIORITY_EMERGENT = tskIDLE_PRIORITY + 4;

mrb_state *g_mrb;
int ai;
extern const uint8_t blinker[];
size_t total_size = 0;

xSemaphoreHandle g_heap_mutex = NULL;

static void
p(mrb_state *mrb, mrb_value obj)
{
  obj = mrb_funcall(mrb, obj, "inspect", 0);
  fwrite(RSTRING_PTR(obj), RSTRING_LEN(obj), 1, stdout);
  putc('\n', stdout);
  Serial2.flush();
}

// stubs for newlib
extern "C" {
    void _exit(int rc){
        Serial2.println("_exit");
        while(1){

        }
    }
    int _getpid(){
        return 1;
    }

    
    int _kill(int pid, int sig){
        errno = EINVAL;
        return -1;
    }

    //http://todotani.cocolog-nifty.com/blog/2010/05/mbed-gccprintf-.html
    int _write_r(struct _reent *r, int file, const void *ptr, size_t len){
        size_t i;
        unsigned char *p = (unsigned char *)ptr;
        for (i = 0 ; i < len ; i++){
            Serial2.write(*p++);
        }
        Serial2.flush();
        return len;
    }

    int _gettimeofday(struct timeval *tv, struct timezone *tz){
        Serial2.println("_gettimeofday called but not supported!!");
        delay(5000);
        return 0;
    }

    //workaround for broken stdlib.h include in Maple IDE
    //http://forums.leaflabs.com/topic.php?id=906
    extern void free(void *ptr);
    extern void *realloc(void *ptr, size_t size);

    void abort(void){
        digitalWrite(LED_RED, HIGH);
        Serial2.println("!!!!!!!!!!!aborted!!!!!!!!!!");
        for(;;){

        }
    }

    //lock and unlock stub to make newlib's malloc() thread safe.
    void __malloc_lock (struct _reent *reent){
        if ( g_heap_mutex ){
            if( pdFALSE == xSemaphoreTakeRecursive( g_heap_mutex, 10*1000/portTICK_RATE_MS)){
                Serial2.println("BUG!! failed to lock heap!");
                delay(1000);
            }
        }
    }

    void __malloc_unlock(struct _reent *reent){
        if ( g_heap_mutex ){
            if ( pdFALSE == xSemaphoreGiveRecursive( g_heap_mutex)){
                Serial2.println("BUG! failed to unlock the heap!");
                delay(1000);
            }
        }
    }

}

//abort signal handler
void abort_handler(int signo)
{
    digitalWrite(LED_ORANGE, HIGH);
    Serial2.println("abort_handler");
    delay(200);
    for(;;){

    }
}

const char *freertos_stack_overflow_mes = "STACK OVERFLOW DETECTED at task: ";
extern "C" {

void vApplicationStackOverflowHook(xTaskHandle *pxTask,
                                   signed char *pcTaskName) {
     // This function will get called if a task overflows its stack.
     // * If the parameters are corrupt then inspect pxCurrentTCB to find
     // * which was the offending task. 

    // (void) pxTask;
    // (void) pcTaskName;

    // while (1)
    //     ;
    Serial2.print(freertos_stack_overflow_mes);
    Serial2.println((const char *)pcTaskName);
    delay(500);
}
}

const char *freertos_heap_fail_mes = "FreeRTOS_HEAP_ALLOC_FAILE!";
extern "C" {
    void vApplicationMallocFailedHook( void ){
        Serial2.println(freertos_heap_fail_mes);
        delay(1000);
        for(;;){
            
        }
    }
}

unsigned int high = NULL;
int failcount = 0;

void *myallocf(mrb_state *mrb, void *p, size_t size, void *ud){

    //never use printf. 

    Serial2.print('.');
    if (size == 0){
        Serial2.print('x');
        free(p);
        return NULL;
    }

    void *ret = realloc(p, size);
    if (!ret){
        digitalWrite(LED_BLUE, HIGH);
        Serial2.print("\n!!!memory allocation error for size:");
        Serial2.print(size,DEC);
        Serial2.print("\n\tcurrent total :");
        Serial2.print(total_size,DEC);
        Serial2.print("\n\tfail count : ");
        Serial2.println(++failcount, DEC);

        return NULL;
    }
    Serial2.print('o');
    unsigned int point = (unsigned int)ret + size;
    if (point > high){
        high = point;
    }
    total_size += size;
    return ret;
}


/* 

custom allocator

*/
void *myallocfCCM(mrb_state *mrb, void *p, size_t size, void *ud)
{
    Serial2.print('.');
    if (size == 0){
        if (CCM_RAM_BASE <= p && (unsigned int)p <= (unsigned int)CCM_RAM_BASE + 50*1024){

        }else{
            Serial2.print('-');
            free(p);
        }

        return NULL;
    }

    void *ret = NULL;
    if ((unsigned int)g_ccm_heap_next < (unsigned int)CCM_RAM_BASE + 50*1024){ //first 46kb is special ccm heap
        Serial2.print('C');
        ret = g_ccm_heap_next;
        g_ccm_heap_next += size;

        //4 byte alignment
        if ((unsigned int)g_ccm_heap_next & 0x00000003){
            g_ccm_heap_next += 4-((unsigned int)g_ccm_heap_next & 0x00000003);
        }

    }else{
        Serial2.print('o');
        ret = realloc(p, size);
        if (!ret){
            digitalWrite(LED_BLUE, HIGH);
            Serial2.print("\n!!!memory allocation error for size:");
            Serial2.print(size, DEC);
            Serial2.print("\n\tcurrent total :");
            Serial2.print(total_size, DEC);
            Serial2.print("\n\tfail count : ");
            Serial2.println(++failcount, DEC);

            return NULL;
        }

        unsigned int point = (unsigned int)ret + size;
        if (point > high){
            high = point;
        }

    }
    total_size += size;
    return ret;
}




//utility to make instance of Blinker
static mrb_value make_blinker_obj(int pin, int interval_ms) 
{
    RClass *blinker_class = mrb_class_get(g_mrb, "Blinker");
    if (g_mrb->exc){
        p(g_mrb, mrb_obj_value(g_mrb->exc));
        Serial2.println("failed to get Blinker class");
        g_mrb->exc = 0;
        return mrb_nil_value();
    }

    mrb_value args[2];
    args[0] = mrb_fixnum_value(pin);
    args[1] = mrb_fixnum_value(interval_ms);

    mrb_value blinker_obj = mrb_class_new_instance(g_mrb, 2, args, blinker_class);
    if (g_mrb->exc){
        p(g_mrb, mrb_obj_value(g_mrb->exc));
        Serial2.println("failed to create new Blinker instance");
        g_mrb->exc = 0;
        return mrb_nil_value();
    }

    return blinker_obj;
}


static void c_task(void *pvParameters)
{
    Serial2.print("new C Task started : handle = 0x");
    Serial2.println((unsigned int)xTaskGetCurrentTaskHandle(), HEX);

    int pin = (int)pvParameters;
    int interval_ms = 999;
    switch(pin){
    case LED_C1:
        interval_ms = 220;
        break;
    case LED_C2:
        interval_ms = 300;
        break;
    default:
        Serial2.println("unknown pin");
    }

    for (;;) {
        digitalWrite(pin, HIGH);
        vTaskDelay(interval_ms);
        digitalWrite(pin, LOW);
        vTaskDelay(interval_ms);
    } 
}

static void ruby_task(void *pvParameters)
{
    Serial2.print("new Ruby Task started : handle = 0x");
    Serial2.println((unsigned int)xTaskGetCurrentTaskHandle(), HEX);

    mrb_value *blinker = (mrb_value *)pvParameters; 

    for(;;) {

        mrb_funcall(g_mrb, *blinker, "blink_once", 0);

        //exception check
        if (g_mrb->exc) {
            p(g_mrb, mrb_obj_value(g_mrb->exc));
            Serial2.println("failed to blink_once!");
            g_mrb->exc = 0;

            //prevent too much serial output 
            vTaskDelay(10000);
        }
        mrb_gc_arena_restore(g_mrb, ai);

    }
}

// static void emergent_task(void *pvParameters)
// {

// }

bool init_mruby();

static void main_task(void *pvParameters) 
{
    // Serial2.println("testing mutex timeout-----");
    // xSemaphoreHandle mutex = xSemaphoreCreateMutex();
    // if (pdFALSE == xSemaphoreTake(mutex, 0)){
    //     Serial2.println("failed to 1st take");
    // }
    // if (pdFALSE == xSemaphoreGive(mutex)){
    //     Serial2.println("failed to 1st give");
    // }

    // if (pdFALSE == xSemaphoreTake(mutex, 0)){
    //     Serial2.println("failed to 2nd take");
    // }
    // Serial2.println("trying to take with timeout = 5000ms");
    // if (pdFALSE == xSemaphoreTake(mutex, 5000 / portTICK_RATE_MS)){
    //     Serial2.println("timeout. this is expected");
    // }else{
    //     Serial2.println("success to take..wrong!");
    // }
    // Serial2.println("testing mutext timeout----done");
    // goto wait;

    mrb_value blinker1;
    mrb_value blinker2;
    Serial2.println("main task started.");

    g_heap_mutex = xSemaphoreCreateRecursiveMutex();
    if (NULL == g_heap_mutex){
        Serial2.println("oops, failed to make lock for malloc()!!");
        delay(1000);
        goto wait;
    }

    //initialize mruby stuff.
    if (!init_mruby()) {
        Serial2.println("failed to init mruby!!!");
        delay(1000);
        goto wait;
    }else{
        Serial2.println("init_mruby() return success");
    }

    blinker1 = make_blinker_obj(LED_R1, 400);
    blinker2 = make_blinker_obj(LED_R2, 400);
    ai = mrb_gc_arena_save(g_mrb);


    //launch the threads
    portBASE_TYPE ret;
    ret = xTaskCreate( c_task,
                        (signed portCHAR *)"C Task1",
                        configMINIMAL_STACK_SIZE,
                        (void *)LED_C1,
                        TASK_PRIORITY_NORMAL,
                        NULL );
    if (ret != pdPASS){
        Serial2.println("failed to create C Task1");
        goto wait;
    }
    vTaskDelay(100);

    ret = xTaskCreate( c_task,
                        (signed portCHAR *)"C Task2",
                        configMINIMAL_STACK_SIZE,
                        (void *)LED_C2,
                        TASK_PRIORITY_NORMAL,
                        NULL );
    if (ret != pdPASS){
        Serial2.println("failed to create C Task2");
        goto wait;
    }

    vTaskDelay(100);

    ret = xTaskCreate( ruby_task,
                        (signed portCHAR *)"Ruby Task1",
                        configMINIMAL_STACK_SIZE + 2048,
                        (void *)&blinker1,
                        TASK_PRIORITY_NORMAL,
                        NULL );
    if (ret != pdPASS){
        Serial2.println("failed to create Ruby Task1");
        goto wait;
    }

    vTaskDelay(1234);

    ret = xTaskCreate( ruby_task,
                        (signed portCHAR *)"Ruby Task2",
                        configMINIMAL_STACK_SIZE + 2048,
                        (void *)&blinker2,
                        TASK_PRIORITY_NORMAL,
                        NULL );
    if (ret != pdPASS){
        Serial2.println("failed to create Ruby Task2");
        goto wait;
    }

    // Emergent task
    //Task which have highest priority for emergent task
    // ret = xTaskCreate( emergent_task,
    //                     (signed portCHAR *)"Emergent Task",
    //                     configMINIMAL_STACK_SIZE + 128,
    //                     NULL,
    //                     TASK_PRIORITY_EMERGENT,
    //                     NULL );
    // if (ret != pdPAss){
    //     Serial2.println("failed to create Emergent Task");
    //     return;       
    // }

    vTaskDelay(200);
    Serial2.println("main task done.");

wait:
    for(;;)
    {
        vTaskDelay(200);
    }

}

mrb_value mrb_freertos_sleep(mrb_state *mrb, mrb_value self);

bool init_mruby()
{
    Serial2.println("init_mruby enter");
    g_mrb = mrb_open_allocf(myallocfCCM, NULL);
    // g_mrb = mrb_open_allocf(myallocf, NULL);
    Serial2.print("mrb_open done. total allocated : ");
    Serial2.println(total_size, DEC);

    mrb_load_irep(g_mrb, blinker);

    RClass *freeRTOSModule = mrb_define_module(g_mrb, "FreeRTOS");
    mrb_define_module_function(g_mrb, freeRTOSModule, "sleep", mrb_freertos_sleep, ARGS_REQ(1));

    Serial2.println("mruby initialized");

    return true;
}

void setup() {

    Serial2.begin(38400);
    Serial2.println("Hello mruby thread safe RiteVM demo.");

    //setup pins
    pinMode(LED_C1, OUTPUT);
    pinMode(LED_C2, OUTPUT);
    pinMode(LED_R1, OUTPUT);
    pinMode(LED_R2, OUTPUT);

    //DO NOT INIT MRUBY BEFORE FreeRTOS start!

    //start the FreeRTOS.
    xTaskCreate(main_task,
                (signed portCHAR *)"main",
                configMINIMAL_STACK_SIZE  + 1024 ,
                NULL,
                TASK_PRIORITY_NORMAL,
                NULL);

    vTaskStartScheduler();
    
    //never reach.
    return;
}

void loop() {

}


// Force init to be called *first*, i.e. before static object allocation.
// Otherwise, statically allocated objects that need libmaple may fail.
__attribute__((constructor)) void premain() {
    init();
}

int main(void) {
    g_ccm_heap_next = CCM_RAM_BASE;
    setup();

    while (true) {
        loop();
    }
    return 0;
}


mrb_value mrb_freertos_sleep(mrb_state *mrb, mrb_value self) {
  mrb_int ms;
  mrb_get_args(mrb, "i", &ms);
  printf("now got into vTaskDelay with %d[ms]\n",ms);
  vTaskDelay(ms);   //equivalent to pthread's sleep
  return mrb_nil_value();
}
