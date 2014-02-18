/**
 ******************************************************************************
 * @file       pios_mutex.c
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @addtogroup PIOS PIOS Core hardware abstraction layer
 * @{
 * @addtogroup PIOS_Mutex Mutex Abstraction
 * @{
 * @brief Abstracts the concept of a mutex to hide different implementations
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
#include "pios_mutex.h"

#if !defined(PIOS_INCLUDE_FREERTOS) && !defined(PIOS_INCLUDE_CHIBIOS)
#error pios_mutex.c requires either PIOS_INCLUDE_FREERTOS or PIOS_INCLUDE_CHIBIOS
#endif

#if defined(PIOS_INCLUDE_FREERTOS)

struct pios_mutex *PIOS_Mutex_Create(void)
{
	struct pios_mutex *mtx = PIOS_malloc(sizeof(struct pios_mutex));

	if (mtx == NULL)
		return NULL;

	mtx->mtx_handle = xSemaphoreCreateMutex();

	return mtx;
}

bool PIOS_Mutex_Lock(struct pios_mutex *mtx, uint32_t timeout_ms)
{
	PIOS_Assert(mtx != NULL);

	portTickType timeout_ticks;
	if (timeout_ms == PIOS_MUTEX_TIMEOUT_MAX)
		timeout_ticks = portMAX_DELAY;
	else
		timeout_ticks = MS2TICKS(timeout_ms);

	return xSemaphoreTake(mtx->mtx_handle, timeout_ticks) == pdTRUE;
}

bool PIOS_Mutex_Unlock(struct pios_mutex *mtx)
{
	PIOS_Assert(mtx != NULL);

	return xSemaphoreGive(mtx->mtx_handle) == pdTRUE;
}

bool PIOS_Mutex_Lock_FromISR(struct pios_mutex *mtx, bool *woken)
{
	PIOS_Assert(mtx != NULL);
	PIOS_Assert(woken != NULL);

	signed portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	bool result = xSemaphoreTakeFromISR(mtx->mtx_handle, &xHigherPriorityTaskWoken) == pdTRUE;

	*woken = *woken || xHigherPriorityTaskWoken == pdTRUE;

	return result;
}

bool PIOS_Mutex_Unlock_FromISR(struct pios_mutex *mtx, bool *woken)
{
	PIOS_Assert(mtx != NULL);
	PIOS_Assert(woken != NULL);

	signed portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	bool result = xSemaphoreGiveFromISR(mtx->mtx_handle, &xHigherPriorityTaskWoken) == pdTRUE;

	*woken = *woken || xHigherPriorityTaskWoken == pdTRUE;

	return result;
}

struct pios_recursive_mutex *PIOS_Recursive_Mutex_Create(void)
{
	struct pios_recursive_mutex *mtx = PIOS_malloc(sizeof(struct pios_recursive_mutex));

	if (mtx == NULL)
		return NULL;

	mtx->mtx_handle = xSemaphoreCreateRecursiveMutex();

	return mtx;
}

bool PIOS_Recursive_Mutex_Lock(struct pios_recursive_mutex *mtx, uint32_t timeout_ms)
{
	PIOS_Assert(mtx != NULL);

	portTickType timeout_ticks;
	if (timeout_ms == PIOS_MUTEX_TIMEOUT_MAX)
		timeout_ticks = portMAX_DELAY;
	else
		timeout_ticks = MS2TICKS(timeout_ms);

	return xSemaphoreTakeRecursive(mtx->mtx_handle, timeout_ticks) == pdTRUE;
}

bool PIOS_Recursive_Mutex_Unlock(struct pios_recursive_mutex *mtx)
{
	PIOS_Assert(mtx != NULL);

	return xSemaphoreGiveRecursive(mtx->mtx_handle) == pdTRUE;
}

#elif defined(PIOS_INCLUDE_CHIBIOS)

struct pios_mutex *PIOS_Mutex_Create(void)
{
	struct pios_mutex *mtx = PIOS_malloc(sizeof(struct pios_mutex));

	if (mtx == NULL)
		return NULL;

	chMtxInit(&mtx->mtx);

	return mtx;
}

bool PIOS_Mutex_Lock(struct pios_mutex *mtx, uint32_t timeout_ms)
{
	PIOS_Assert(mtx != NULL);

	chMtxLock(&mtx->mtx);

	return true;
}

bool PIOS_Mutex_Unlock(struct pios_mutex *mtx)
{
	PIOS_Assert(mtx != NULL);

	chMtxUnlock();

	return true;
}

struct pios_recursive_mutex *PIOS_Recursive_Mutex_Create(void)
{
	struct pios_recursive_mutex *mtx = PIOS_malloc(sizeof(struct pios_recursive_mutex));

	if (mtx == NULL)
		return NULL;

	chMtxInit(&mtx->mtx);
	mtx->count = 0;

	return mtx;
}

bool PIOS_Recursive_Mutex_Lock(struct pios_recursive_mutex *mtx, uint32_t timeout_ms)
{
	PIOS_Assert(mtx != NULL);

	chSysLock();

	if (chThdSelf() != mtx->mtx.m_owner)
		chMtxLockS(&mtx->mtx);

	++mtx->count;

	chSysUnlock();

	return true;
}

bool PIOS_Recursive_Mutex_Unlock(struct pios_recursive_mutex *mtx)
{
	PIOS_Assert(mtx != NULL);

	chSysLock();

	--mtx->count;

	if (mtx->count == 0)
		chMtxUnlockS();

	chSysUnlock();

	return true;
}

bool PIOS_Recursive_Mutex_Lock_FromISR(struct pios_recursive_mutex *mtx, bool *woken)
{
	/* Locking a mutex within an interrupt is not supported by ChibiOS. */
	PIOS_Assert(false);
	return false;
}

bool PIOS_Recursive_Mutex_Unlock_FromISR(struct pios_recursive_mutex *mtx, bool *woken)
{
	/* Unlocking a mutex within an interrupt is not supported by ChibiOS. */
	PIOS_Assert(false);
	return false;
}

#endif

