/* SPDX-License-Identifier: Apache-2.0 */
#include "input_queue.h"

#include <stdatomic.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

static StaticQueue_t command_queue_control;
static uint8_t command_queue_storage[CONFIG_MOL_INPUT_QUEUE_LENGTH * sizeof(mol_command_t)]
    __attribute__((aligned(portBYTE_ALIGNMENT)));
static QueueHandle_t command_queue;
static atomic_uint_least32_t queued_count;
static atomic_uint_least32_t dropped_count;
static atomic_uint_least32_t rejected_count;
static atomic_uint_least32_t high_water;
enum {
  MOL_INPUT_REQUEST_CONFIG_MODE = 1u << 0u,
  MOL_INPUT_REQUEST_CLEAR_PAIRING = 1u << 1u,
  MOL_INPUT_REQUEST_FACTORY_RESET = 1u << 2u
};

static atomic_uint_least32_t pending_requests;

void mol_input_queue_init(void) {
  command_queue = xQueueCreateStatic(CONFIG_MOL_INPUT_QUEUE_LENGTH, sizeof(mol_command_t),
                                     command_queue_storage, &command_queue_control);
  atomic_store_explicit(&queued_count, 0u, memory_order_relaxed);
  atomic_store_explicit(&dropped_count, 0u, memory_order_relaxed);
  atomic_store_explicit(&rejected_count, 0u, memory_order_relaxed);
  atomic_store_explicit(&high_water, 0u, memory_order_relaxed);
  atomic_store_explicit(&pending_requests, 0u, memory_order_relaxed);
}

bool mol_input_submit(const mol_command_t* command) {
  UBaseType_t waiting;
  uint_least32_t observed;
  if (command_queue == NULL || command == NULL ||
      xQueueSendToBack(command_queue, command, 0u) != pdPASS) {
    atomic_fetch_add_explicit(&dropped_count, 1u, memory_order_relaxed);
    return false;
  }
  atomic_fetch_add_explicit(&queued_count, 1u, memory_order_relaxed);
  waiting = uxQueueMessagesWaiting(command_queue);
  observed = atomic_load_explicit(&high_water, memory_order_relaxed);
  while ((uint_least32_t)waiting > observed &&
         !atomic_compare_exchange_weak_explicit(&high_water, &observed, (uint_least32_t)waiting,
                                                memory_order_relaxed, memory_order_relaxed)) {
  }
  return true;
}

uint32_t mol_input_drain(mol_engine_t* engine) {
  mol_command_t command;
  uint32_t drained = 0u;
  if (command_queue == NULL || engine == NULL) {
    return 0u;
  }
  while (xQueueReceive(command_queue, &command, 0u) == pdPASS) {
    if (mol_engine_submit(engine, &command) != MOL_OK) {
      atomic_fetch_add_explicit(&rejected_count, 1u, memory_order_relaxed);
    }
    ++drained;
  }
  return drained;
}

void mol_input_request_config_mode(void) {
  atomic_fetch_or_explicit(&pending_requests, MOL_INPUT_REQUEST_CONFIG_MODE, memory_order_release);
}

bool mol_input_take_config_mode_request(void) {
  return (atomic_fetch_and_explicit(&pending_requests, ~MOL_INPUT_REQUEST_CONFIG_MODE,
                                    memory_order_acq_rel) &
          MOL_INPUT_REQUEST_CONFIG_MODE) != 0u;
}

void mol_input_request_clear_pairing(void) {
  atomic_fetch_or_explicit(&pending_requests, MOL_INPUT_REQUEST_CLEAR_PAIRING,
                           memory_order_release);
}

bool mol_input_take_clear_pairing_request(void) {
  return (atomic_fetch_and_explicit(&pending_requests, ~MOL_INPUT_REQUEST_CLEAR_PAIRING,
                                    memory_order_acq_rel) &
          MOL_INPUT_REQUEST_CLEAR_PAIRING) != 0u;
}

void mol_input_request_factory_reset(void) {
  atomic_fetch_or_explicit(&pending_requests, MOL_INPUT_REQUEST_FACTORY_RESET,
                           memory_order_release);
}

bool mol_input_take_factory_reset_request(void) {
  return (atomic_fetch_and_explicit(&pending_requests, ~MOL_INPUT_REQUEST_FACTORY_RESET,
                                    memory_order_acq_rel) &
          MOL_INPUT_REQUEST_FACTORY_RESET) != 0u;
}

mol_input_queue_stats_t mol_input_queue_stats(void) {
  mol_input_queue_stats_t stats;
  stats.queued = (uint32_t)atomic_load_explicit(&queued_count, memory_order_relaxed);
  stats.dropped = (uint32_t)atomic_load_explicit(&dropped_count, memory_order_relaxed);
  stats.rejected = (uint32_t)atomic_load_explicit(&rejected_count, memory_order_relaxed);
  stats.high_water = (uint32_t)atomic_load_explicit(&high_water, memory_order_relaxed);
  return stats;
}
