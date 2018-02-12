/**
 * @file rtos.c
 * @author ITESO
 * @date Feb 2018
 * @brief Implementation of rtos API
 *
 * This is the implementation of the rtos module for the
 * embedded systems II course at ITESO
 */

#include "rtos.h"
#include "rtos_config.h"
#include "clock_config.h"

#ifdef RTOS_ENABLE_IS_ALIVE
#include "fsl_gpio.h"
#include "fsl_port.h"
#endif
/**********************************************************************************/
// Module defines
/**********************************************************************************/

#define FORCE_INLINE 	__attribute__((always_inline)) inline

#define STACK_FRAME_SIZE			8
#define STACK_LR_OFFSET				2
#define STACK_PSR_OFFSET			1
#define STACK_PSR_DEFAULT			0x01000000

/**********************************************************************************/
// IS ALIVE definitions
/**********************************************************************************/

#ifdef RTOS_ENABLE_IS_ALIVE
#define CAT_STRING(x,y)  		x##y
#define alive_GPIO(x)			CAT_STRING(GPIO,x)
#define alive_PORT(x)			CAT_STRING(PORT,x)
#define alive_CLOCK(x)			CAT_STRING(kCLOCK_Port,x)
static void init_is_alive(void);
static void refresh_is_alive(void);
#endif

/**********************************************************************************/
// Type definitions
/**********************************************************************************/

typedef enum
{
	S_READY = 0, S_RUNNING, S_WAITING, S_SUSPENDED
} task_state_e;
typedef enum
{
	kFromISR = 0, kFromNormalExec
} task_switch_type_e;

typedef struct
{
	uint8_t priority;
	task_state_e state;
	uint32_t *sp;
	void (*task_body)();
	rtos_tick_t local_tick;
	uint32_t reserved[10];
	uint32_t stack[RTOS_STACK_SIZE];
} rtos_tcb_t;

/**********************************************************************************/
// Global (static) task list
/**********************************************************************************/

struct
{
	uint8_t nTasks; // cuantas tareas se han creado hasta el momento, si se llama al task create 3 veces son 3 tareas
	rtos_task_handle_t current_task; // Identificador de la tarea actual, es un indice
	rtos_task_handle_t next_task;
	rtos_tcb_t tasks[RTOS_MAX_NUMBER_OF_TASKS + 1];
	rtos_tick_t global_tick; //cuantas veces entramos a la interrupción del sistema operativo
} task_list =
{ 0 };

/**********************************************************************************/
// Local methods prototypes
/**********************************************************************************/

static void reload_systick(void);
static void dispatcher(task_switch_type_e type);
static void activate_waiting_tasks();
FORCE_INLINE static void context_switch(task_switch_type_e type);
static void idle_task(void);

/**********************************************************************************/
// API implementation
/**********************************************************************************/


//inicar el reloj en cero, crear un idle task
//falta inicializar el current task en algo
void rtos_start_scheduler(void)
{
#ifdef RTOS_ENABLE_IS_ALIVE
	init_is_alive();
#endif
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk
	        | SysTick_CTRL_ENABLE_Msk;
	reload_systick();
	for (;;)
		;
}

rtos_task_handle_t rtos_create_task(void (*task_body)(), uint8_t priority,
		rtos_autostart_e autostart)
{

if(RTOS_MAX_NUMBER_OF_TASKS >task_list.nTasks) // para que no se creen má tareas de las que se puede
	{
		task_list.tasks[task_list.nTasks].local_tick = 0;
		task_list.tasks[task_list.nTasks].priority = priority;
		task_list.tasks[task_list.nTasks].reserved= NULL;
		task_list.tasks[task_list.nTasks].sp =
				&(task_list.tasks[task_list.nTasks].stack[RTOS_STACK_SIZE - 1 - STACK_FRAME_SIZE]);
		task_list.tasks[task_list.nTasks].state=
				kStartSuspended == autostart ? S_SUSPENDED: S_READY;
		task_list.tasks[task_list.nTasks].task_body = task_body;



	}
}

rtos_tick_t rtos_get_clock(void)
{
	return 0;
}

void rtos_delay(rtos_tick_t ticks)
{

}

void rtos_suspend_task(void)
{

}

void rtos_activate_task(rtos_task_handle_t task)
{

}

/**********************************************************************************/
// Local methods implementation
/**********************************************************************************/

static void reload_systick(void) // para que sea periódica la interrupción
{
	SysTick->LOAD = USEC_TO_COUNT(RTOS_TIC_PERIOD_IN_US,
	        CLOCK_GetCoreSysClkFreq());
	SysTick->VAL = 0;
}


/*elige la tarea que sige en base a las prioridades y hace un cambio de contexto ,
 * depende de la política de scheduling puede ser Round Robin */
static void dispatcher(task_switch_type_e type)
{
	rtos_task_handle_t next_task = RTOS_INVALID_TASK;
	rtos_task_handle_t index;
	int8_t highest = -1;
	for(index = 0 ; index < task_list.nTasks ; index++) // chdcamos cual es la tarea de mayor prioridad
	{
		if(highest < task_list.tasks[index].priority &&
				(S_READY == task_list.tasks[index].state ||
				S_RUNNING == task_list.tasks[index].state))
		{
			next_task = index;
			highest = task_list.tasks[index].priority;
		}
	}
	if(task_list.current_task != next_task)
	{
		task_list.next_task = next_task;
	}

}

//Done
FORCE_INLINE static void context_switch(task_switch_type_e type)
{
	register uint32_t sp asm("sp");
	task_list.current_task = task_list.next_task;
task_list.tasks[task_list.current_task].state = S_RUNNING;
task_list.tasks[task_list.current_task].sp = sp;
SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

static void activate_waiting_tasks()
{

}

/**********************************************************************************/
// IDLE TASK
/**********************************************************************************/

static void idle_task(void)
{
	for (;;)
	{

	}
}

/**********************************************************************************/
// ISR implementation
/**********************************************************************************/

void SysTick_Handler(void)
{
#ifdef RTOS_ENABLE_IS_ALIVE
	refresh_is_alive();
#endif
	activate_waiting_tasks();
	reload_systick();
}
//interrupción de más baja prioridad la Pendsv, es para hacer cambio de contexto, tenemos que poner un registro en 1 para invocarla
// que está en el documento del arm el pendSV va a guardar la información del contexto, por lo tanto para hacer cambio de contexto llamamos al PendSV
// y switcheamos el apuntador sp, DOne
void PendSV_Handler(void)
{
	register uint32_t r0 asm("r0");
	SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;
	r0 = task_list.tasks[task_list.current_task].sp;
	asm("mov r7, r0");
}

/**********************************************************************************/
// IS ALIVE SIGNAL IMPLEMENTATION
/**********************************************************************************/

#ifdef RTOS_ENABLE_IS_ALIVE
static void init_is_alive(void)
{
	gpio_pin_config_t gpio_config =
	{ kGPIO_DigitalOutput, 1, };

	port_pin_config_t port_config =
	{ kPORT_PullDisable, kPORT_FastSlewRate, kPORT_PassiveFilterDisable,
	        kPORT_OpenDrainDisable, kPORT_LowDriveStrength, kPORT_MuxAsGpio,
	        kPORT_UnlockRegister, };
	CLOCK_EnableClock(alive_CLOCK(RTOS_IS_ALIVE_PORT));
	PORT_SetPinConfig(alive_PORT(RTOS_IS_ALIVE_PORT), RTOS_IS_ALIVE_PIN,
	        &port_config);
	GPIO_PinInit(alive_GPIO(RTOS_IS_ALIVE_PORT), RTOS_IS_ALIVE_PIN,
	        &gpio_config);
}

static void refresh_is_alive(void)
{
	static uint8_t state = 0;
	static uint32_t count = 0;
	SysTick->LOAD = USEC_TO_COUNT(RTOS_TIC_PERIOD_IN_US,
	        CLOCK_GetCoreSysClkFreq());
	SysTick->VAL = 0;
	if (RTOS_IS_ALIVE_PERIOD_IN_US / RTOS_TIC_PERIOD_IN_US - 1 == count)
	{
		GPIO_WritePinOutput(alive_GPIO(RTOS_IS_ALIVE_PORT), RTOS_IS_ALIVE_PIN,
		        state);
		state = state == 0 ? 1 : 0;
		count = 0;
	} else
	{
		count++;
	}
}
#endif
