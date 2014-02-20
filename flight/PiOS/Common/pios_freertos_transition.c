/**
 ******************************************************************************
 * @addtogroup PIOS PIOS Core hardware abstraction layer
 * @{
 * @addtogroup PIOS_FREERTOS_TRANSITION
 * @brief Transition layer for FreeRTOS primitives
 * @{
 *
 * @file       pios_freertos_transition.c
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @brief      FreeRTOS transision layer source
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "pios.h"
#include "pios_freertos_transition.h"

#include <string.h>

/* Delay */
void vTaskDelayUntil(systime_t * const pxPreviousWakeTime, systime_t xTimeIncrement)
{
    systime_t future = *pxPreviousWakeTime + xTimeIncrement;
    chSysLock();
    systime_t now = chTimeNow();
    int mustDelay =
        now < *pxPreviousWakeTime ?
        (now < future && future < *pxPreviousWakeTime) :
        (now < future || future < *pxPreviousWakeTime);
    if (mustDelay)
        chThdSleepS(future - now);
    chSysUnlock();
    *pxPreviousWakeTime = future;
}

/* Removed functions */
void vPortInitialiseBlocks(void)
{
}

void SystemInit(void)
{
}

void SystemCoreClockUpdate(void)
{
}

/* Heap */
static bool heap_failed;

void * pvPortMalloc(size_t n)
{
	void *result = chCoreAlloc(n);
	if (result == NULL)
		heap_failed = true;
	return result;
}

void vPortFree(void* p)
{
}

void * PIOS_malloc(size_t n)
{
	return pvPortMalloc(n);
}

void * PIOS_malloc_no_dma(size_t n)
{
	return pvPortMalloc(n);
}

bool PIOS_heap_malloc_failed_p(void)
{
	return heap_failed;
}

size_t xPortGetFreeHeapSize(void)
{
	return chCoreStatus();
}

/* Task */
#if CH_DBG_FILL_THREADS
unsigned long uxTaskGetStackHighWaterMark(Thread *thread)
{
	uint32_t *stack = (uint32_t*)((size_t)thread + sizeof(*thread));
	uint32_t *stklimit = stack;
	while (*stack ==
			((CH_STACK_FILL_VALUE << 24) |
			(CH_STACK_FILL_VALUE << 16) |
			(CH_STACK_FILL_VALUE << 8) |
			(CH_STACK_FILL_VALUE << 0)))
		++stack;
	return stack - stklimit;
}
#endif

unsigned long uxTaskGetRunTime(Thread *thread)
{
	chSysLock();

	uint32_t result = thread->ticks_total;
	thread->ticks_total = 0;

	chSysUnlock();

	return result;
}

long xTaskCreate(void (*pf)(void *), const signed char* name, size_t size, void *arg, tprio_t prio, xTaskHandle *handle_p)
{
	Thread *thread = chThdCreateFromHeap(NULL, size * 4, prio, (msg_t (*)(void *))pf, arg);
	if (thread == NULL)
	{
		heap_failed = true;
		return pdFAIL;
	}
	*handle_p = thread;
	return pdPASS;
}

void vTaskDelete(xTaskHandle task)
{
	// deleting yourself is not supported
	PIOS_Assert(task != NULL);
	chThdTerminate(task);
	chThdWait(task);
}

/* Queue */
#define QUEUE_MAX_WAITERS 1
xQueueHandle xQueueCreate(size_t max_queue_size, size_t obj_size)
{
	/* Create the xQueueHandle struct. */
	xQueueHandle queue = chCoreAlloc(sizeof(*queue));

	/* Create the memory pool. */
	chPoolInit(&queue->mp, obj_size, NULL);
	chPoolLoadArray(&queue->mp,
			chCoreAlloc(obj_size * (max_queue_size + QUEUE_MAX_WAITERS)),
			(max_queue_size + QUEUE_MAX_WAITERS));

	/* Now create the mailbox. */
	msg_t *mb_buf = chCoreAlloc(sizeof(msg_t) * max_queue_size);
	chMBInit(&queue->mb, mb_buf, max_queue_size);

	return queue;
}

signed long xQueueSendToBack(xQueueHandle queue, void* data, systime_t timeout)
{
	void* buf = chPoolAlloc(&queue->mp);
	if (buf == NULL)
		return errQUEUE_FULL;

	memcpy(buf, data, queue->mp.mp_object_size);

	msg_t result = chMBPost(&queue->mb, (msg_t)buf, timeout);

	if (result == RDY_OK)
		return pdTRUE;

	chPoolFree(&queue->mp, buf);

	return errQUEUE_FULL;
}

signed long xQueueReceive(xQueueHandle queue, void* data, systime_t timeout)
{
	msg_t buf;

	msg_t result = chMBFetch(&queue->mb, &buf, timeout);

	if (result != RDY_OK)
		return pdFALSE;

	memcpy(data, (void*)buf, queue->mp.mp_object_size);

	chPoolFree(&queue->mp, (void*)buf);

	return pdTRUE;
}

signed long xQueueSendToBackFromISR(xQueueHandle queue, void* data, signed long *woken)
{
	chSysLock();
	void *buf = chPoolAllocI(&queue->mp);
	if (buf == NULL)
	{
		chSysUnlock();
		return errQUEUE_FULL;
	}

	memcpy(buf, data, queue->mp.mp_object_size);

	msg_t result = chMBPostI(&queue->mb, (msg_t)buf);

	if (result == RDY_OK)
		return pdTRUE;

	chPoolFreeI(&queue->mp, buf);

	chSysUnlock();

	return errQUEUE_FULL;
}
