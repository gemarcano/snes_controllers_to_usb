// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <FreeRTOS.h>
#include <semphr.h>

extern "C"
{
	void vApplicationGetIdleTaskMemory(StaticTask_t **idle_task_tcb, StackType_t **idle_task_stack, uint32_t *idle_stack_size)
	{
		static StaticTask_t task_tcb;
		static StackType_t task_stack[configMINIMAL_STACK_SIZE];

		*idle_task_tcb = &task_tcb;
		*idle_task_stack = task_stack;
		*idle_stack_size = sizeof(task_stack)/sizeof(*task_stack);
	}

	void vApplicationGetPassiveIdleTaskMemory(StaticTask_t **idle_task_tcb, StackType_t **idle_task_stack, uint32_t *idle_stack_size, BaseType_t core_id)
	{
		static StaticTask_t task_tcb[configNUMBER_OF_CORES - 1];
		static StackType_t task_stack[configNUMBER_OF_CORES - 1][configMINIMAL_STACK_SIZE];

		*idle_task_tcb = &task_tcb[core_id];
		*idle_task_stack = task_stack[core_id];
		*idle_stack_size = sizeof(*task_stack)/sizeof(**task_stack);
	}

	void vApplicationGetTimerTaskMemory(StaticTask_t **timer_task_tcb, StackType_t **timer_task_stack, uint32_t *timer_stack_size)
	{
		static StaticTask_t task_tcb;
		static StackType_t task_stack[configTIMER_TASK_STACK_DEPTH];

		*timer_task_tcb = &task_tcb;
		*timer_task_stack = task_stack;
		*timer_stack_size = sizeof(task_stack)/sizeof(*task_stack);
	}

	void vApplicationStackOverflowHook(
		TaskHandle_t /*xTask*/, char * /*pcTaskName*/)
	{
		__asm volatile ("bkpt #0");
	}
}
