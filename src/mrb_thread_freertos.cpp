#include <mruby.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <mruby/variable.h>
#include <mruby/thread.h>

/* Target version and port */
/* 
  FreeRTOS V7.0.1 
  at https://github.com/AeroQuad/AeroQuad/tree/master/Libmaple/libmaple/libraries/FreeRTOS
*/
#include "MapleFreeRTOS.h"

#ifdef ENABLE_THREAD

void debugc(char c)
{
  putchar(c);
}

int
mrb_freertos_rwlock_init(mrb_state *mrb, mrb_rwlock_t *lock)
{
  debugc('i');
  xSemaphoreHandle mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    return -1;
  }
  lock->rwlock = (mrb_gem_rwlock_t)mutex;
  return RWLOCK_STATUS_OK;
}

int
mrb_freertos_rwlock_destroy(mrb_state *mrb, mrb_rwlock_t *lock)
{
  xSemaphoreHandle mutex = (xSemaphoreHandle)lock->rwlock;
  if (mutex == NULL) {
    return RWLOCK_STATUS_INVALID_ARGUMENTS;
  }
  /*my FreeRTOS port somehow does not have SemaphoreDelete
  /*vSemaphoreDelete(mutex);*/
  xSemaphoreGive(mutex);

  return RWLOCK_STATUS_OK;
}

int
mrb_freertos_rwlock_wrlock(mrb_state *mrb, mrb_rwlock_t *lock, uint32_t timeout_ms)
{
  debugc('l');
  xSemaphoreHandle mutex = (xSemaphoreHandle)lock->rwlock;
  if (mutex == NULL) {
    return RWLOCK_STATUS_INVALID_ARGUMENTS;
  }

  portTickType timeout_tick = timeout_ms / portTICK_RATE_MS;
  if (pdTRUE == xSemaphoreTake(mutex, timeout_tick)) {
    return RWLOCK_STATUS_OK;
  }else{
    debugc('x');
    return RWLOCK_STATUS_TIMEOUT;
  }
}

int
mrb_freertos_rwlock_rdlock(mrb_state *mrb, mrb_rwlock_t *lock, uint32_t timeout_ms)
{
  return mrb_freertos_rwlock_wrlock(mrb, lock, timeout_ms);
}

int
mrb_freertos_rwlock_unlock(mrb_state *mrb, mrb_rwlock_t *lock)
{
  debugc('u');
  xSemaphoreHandle mutex = (xSemaphoreHandle)lock->rwlock;
  if (mutex == NULL) {
    return RWLOCK_STATUS_INVALID_ARGUMENTS;
  }
  if (pdTRUE == xSemaphoreGive(mutex)) {
    return RWLOCK_STATUS_OK;
  }else{
    debugc('X');
    return RWLOCK_STATUS_UNKNOWN; /*maybe give(unlock) without take(lock) called*/
  } 
}

void
mrb_freertos_rwlock_deadlock_handler(mrb_state *mrb, mrb_rwlock_t *lock)
{
  debugc('D');
  vTaskDelay(1000);
}

mrb_thread_lock_api const lock_api_entry = {
  mrb_freertos_rwlock_init,
  mrb_freertos_rwlock_destroy,
  mrb_freertos_rwlock_rdlock,
  mrb_freertos_rwlock_wrlock,
  mrb_freertos_rwlock_unlock,
  mrb_freertos_rwlock_deadlock_handler
};

mrb_gem_thread_t
mrb_freertos_thread_get_self(mrb_state *mrb)
{
  xTaskHandle task = xTaskGetCurrentTaskHandle();
  return (mrb_gem_thread_t)task;
}

int
mrb_freertos_thread_equals(mrb_state *mrb, mrb_gem_thread_t t1, mrb_gem_thread_t t2)
{
  xTaskHandle task1 = (xTaskHandle)t1;
  xTaskHandle task2 = (xTaskHandle)t2;
  
  if ((task1 == NULL) || (task2 == NULL)) {
    return 0;
  }

  if (task1 == task2) {
    return 1;
  }

  return 0;
}

mrb_value
mrb_freertos_thread_join(mrb_state *mrb, mrb_gem_thread_t t)
{
  /*
    There are no 'join' in FreeRTOS.
    task should never return.
    Only things task can do is to ask other task to kill, or kill by self.
  */
  /* I think it is OK for now because no one call */
  return mrb_nil_value();

}

void
mrb_freertos_thread_free(mrb_state *mrb, mrb_gem_thread_t t)
{

  xTaskHandle task = (xTaskHandle)t;
  vTaskDelete(task);

}

mrb_thread_api const thread_api_entry = {
  mrb_freertos_thread_get_self,
  mrb_freertos_thread_equals,
  mrb_freertos_thread_join,
  mrb_freertos_thread_free,
};


extern "C"{

void
mrb_mruby_thread_freertos_gem_init(mrb_state* mrb)
{

  mrb_vm_thread_api_set(mrb, &thread_api_entry);
  mrb_vm_lock_api_set(mrb, &lock_api_entry);

  //TODO : implement Thread class.
  // struct RClass* _class_thread = mrb_define_class(mrb, "Thread", mrb->object_class);
  // mrb_define_method(mrb, _class_thread, "initialize", mrb_thread_init, ARGS_OPT(1));
  // mrb_define_method(mrb, _class_thread, "join", mrb_thread_join, ARGS_NONE());

}

void
mrb_mruby_thread_freertos_gem_final(mrb_state* mrb)
{
}
}

#endif /* ENABLE_THREAD */
