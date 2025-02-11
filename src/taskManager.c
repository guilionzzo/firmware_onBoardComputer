/*
 * taskManager.c
 *
 * Copyright (C) 2019, Universidade de Brasilia - FGA
 *
 * This file is part of Firmware_OBC.
 *
 * Firmware_OBC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Firmware_OBC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Firmware_OBC.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * \file taskManager.c
 *
 * \brief taskManager file
 *
 * \author Guilherme Lion�o
 *
 */

#ifndef SRC_TASKMANAGER_C_
#define SRC_TASKMANAGER_C_

#include "taskManager.h"

void updateBatteryLevel(void);
void setOperationMode(void);
void killAllTasks(void);
void prvTaskCreate(void);
void prvWatchDogEventGroupCreate(void);
void prvQueueDataCreate(void);
void prvQueueCommunicationCreate(void);
void prvResumeAllTasks(void);
void prvSuspendAllTasks(void);
void hibernate(uint32_t time_ms);

extern void *aodcsTask(void *pvParameters);
extern void *cameraTask(void *pvParameters);
extern void *dataStorage(void *pvParameters);
extern void *houseKeeping(void *pvParameters);
extern void *pptTask(void *pvParameters);
extern void *ttcTask(void *pvParameters);
extern void *watchDogTask(void *pvParameters);

/***** HANDLES *****/
QueueHandle_t xQueueIMU = NULL;
QueueHandle_t xQueueDataObc = NULL;
QueueHandle_t xQueueSystem = NULL;
QueueHandle_t xQueueDeletedTasks = NULL;
SemaphoreHandle_t semaphoreIMU = NULL;
SemaphoreHandle_t semaphoreSystem = NULL;
/*
 TaskHandle_t taskHandlerAodcs;
 TaskHandle_t taskHandlerCamera;
 TaskHandle_t taskHandlerDataStorage;
 TaskHandle_t taskHandlerHouseKeeping;
 TaskHandle_t taskHandlerPpt;
 TaskHandle_t taskHandlerTtc;
 TaskHandle_t taskHandlerWatchDog;
 */

uint16_t batteryValue = 0;

uint8_t adcInitDelay = 0;
volatile bool lastMode = 1;
volatile uint32_t clock;

void *taskManager(void *pvParameters)
{
    memset(obcData, 0x00, sizeof(obcData));

    memcpy((void*) obcData.system_status, "NM", sizeof("NM"));

    prvTaskCreate();

    portTickType xLastWakeTimeTaskManager = xTaskGetTickCount();

    while (1)
    {

        updateBatteryLevel();

        if (lastMode != flag_systemMode)
        {
            //setOperationMode();
        }

        if (taskHandlerTTC == NULL)
        {
            xTaskCreate((TaskFunction_t) ttcTask, "TT&C Task",
            configMINIMAL_STACK_SIZE,
                        NULL, 1, &taskHandlerTTC);
        }

        (flag_lowBattery) ? vTaskDelayUntil(&xLastWakeTimeTaskManager,
                            TASK_MANAGER_TICK_PERIOD_LOW_BATTERY) :
                            vTaskDelayUntil(&xLastWakeTimeTaskManager,
                            TASK_MANAGER_TICK_PERIOD);

        prvTaskRestart();

    }

}

void prvTaskCreate(void)
{

    /*create queue to exchange data between the tasks*/
    prvQueueDataCreate();

    prvWatchDogEventGroupCreate();

    /* xTaskCreate((TaskFunction_t) aodcsTask, "AODCS Task",
     configMINIMAL_STACK_SIZE,
     NULL, 1, &taskHandlerAodcs);
     xTaskCreate((TaskFunction_t) cameraTask, "CAMERA Task",
     configMINIMAL_STACK_SIZE,
     NULL, 1, &taskHandlerCamera);
     xTaskCreate((TaskFunction_t) houseKeeping, "House Keeping", 1024, NULL, 2,
     &taskHandlerDataStorage);
     xTaskCreate((TaskFunction_t) dataStorage, "Data Storage", 1024, NULL, 1,
     &taskHandlerHouseKeeping);
     xTaskCreate((TaskFunction_t) pptTask, "PPT Task", configMINIMAL_STACK_SIZE,
     NULL, 1, &taskHandlerPpt);*/
    xTaskCreate((TaskFunction_t) ttcTask, "TT&C Task", configMINIMAL_STACK_SIZE,
                NULL, 1, &taskHandlerTTC);
    xTaskCreate((TaskFunction_t) watchDogTask, "WTD Task",
    configMINIMAL_STACK_SIZE,
                NULL, 5, &taskHandlerWatchDog);

}

void setOperationMode(void)
{

    MAP_CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_48);
    clock = CS_getMCLK();

    switch (flag_systemMode)
    {
    case NM_MODE:

#if DEGUB_SESSION_TRACEALYZER
        vTracePrintF(stateChanel, "NM_MODE");
#endif
        //prvResumeAllTasks();

        break;
    case BLLM1_MODE:

#if DEGUB_SESSION_TRACEALYZER
        vTracePrintF(stateChanel, "BLLM1_MODE");
#endif

        //prvSuspendAllTasks();

        break;
    case HM_MODE:

#if DEGUB_SESSION_TRACEALYZER
        vTracePrintF(stateChanel, "HM_MODE");
#endif
        //prvSuspendAllTasks();
        //hibernate(10);

        break;
    case SM_MODE:
        //TODO
        break;
    default:
        break;
    }

}

void hibernate(uint32_t time_ms)
{

    uint32_t clockFrequency = 48000000;  //48MHz
    uint32_t tickRateHZ = 1000;           //1KHz

    uint32_t timerCountsForOneTick = (clockFrequency / tickRateHZ); // timer for one tick 20,8ms
    uint32_t maximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER
            / timerCountsForOneTick;
    uint32_t iterations = time_ms / maximumPossibleSuppressedTicks;
    uint32_t i = 0;

    for (i = 0; i < iterations; i++)
    {
        vPortSuppressTicksAndSleep(maximumPossibleSuppressedTicks); // loop to complete the time
    }

}

void killAllTasks(void)
{

    //TODO:

}

void updateBatteryLevel(void)
{
    lastMode = flag_systemMode;

    if (adcInitDelay < 6)
    {
        adcInitDelay++;
        memcpy((void*) obcData.system_status, "INIT", sizeof("INIT"));

        flag_systemMode = NM_MODE;
        flag_lowBattery = NM_MODE;

    }
    else
    {

        batteryValue = ADC14_getResult(ADC_MEM5);

        //BATERY LEVEL 1
        if (batteryValue >= 4000 && batteryValue < 14500)
        {

            //DEBUG SESSION
#if DEBUG_SESSION
            MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P2, GPIO_PIN0); // PIN RED
#endif

            memcpy((void*) obcData.system_status, "BLLM1", sizeof("BLLM1"));
            flag_systemMode = BLLM1_MODE;
            flag_lowBattery = BATTERY_LEVEL_1;

        }
        //HIBERNATE MODE
        else if (batteryValue < 4000)
        {

            //DEBUG SESSION
#if DEBUG_SESSION
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0); // PIN RED
#endif
            memcpy((void*) obcData.system_status, "HM_MODE", sizeof("HM_MODE"));
            flag_systemMode = HM_MODE;
            flag_lowBattery = BATTERY_LEVEL_1;
        }
        //NORMAL MODE
        else
        {

            //DEBUG SESSION
#if DEBUG_SESSION
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0); // PIN RED
#endif
            memcpy((void*) obcData.system_status, "NM_MODE", sizeof("NM_MODE"));
            flag_systemMode = NM_MODE;
            flag_lowBattery = NM_MODE;

        }
    }
}

void prvWatchDogEventGroupCreate(void)
{

    //create the event group for wtd
    WATCHDOG_EVENT_GROUP = xEventGroupCreate();
}

/* CREATE THE QUEUES TO SUPPORT THE DATA EXCHANGE BETWEEN THE TASKS*/
void prvQueueDataCreate(void)
{

    //creating a Queue to handle the data between the tasks
    xQueueIMU = xQueueCreate(9, SIZE_OF_IMU_DATA*8);
    //xQueueDataObc=xQueueCreate( 1,sizeof(obcData));
    //xQueueIMU = xQueueCreate( 1,sizeof(obcData));
    xQueueSystem = xQueueCreate(1, sizeof(int16_t));

    //creating a queue to handle the tasks that were deleted by wtdTask
    xQueueDeletedTasks = xQueueCreate(6, sizeof(uint8_t));

    semaphoreSystem = xSemaphoreCreateMutex();

    semaphoreIMU = xSemaphoreCreateMutex();

}

void prvQueueCommunicationCreate(void)
{

}

void prvSuspendAllTasks()
{

    switch (flag_systemMode)
    {
    case BLLM1_MODE:

        vTaskSuspend(taskHandlerAodcs);
        vTaskSuspend(taskHandlerCamera);
        vTaskSuspend(taskHandlerPPT);
        vTaskSuspend(taskHandlerTTC);
        MAP_WDT_A_holdTimer();

        break;
    case HM_MODE:

        vTaskSuspend(taskHandlerAodcs);
        vTaskSuspend(taskHandlerCamera);
        vTaskSuspend(taskHandlerDataStorage);
        vTaskSuspend(taskHandlerHouseKeeping);
        vTaskSuspend(taskHandlerPPT);
        vTaskSuspend(taskHandlerTTC);
        vTaskSuspend(taskHandlerWatchDog);
        MAP_WDT_A_holdTimer();
        break;

    default:
        break;
    }
}

void prvResumeAllTasks()
{

    vTaskResume(taskHandlerAodcs);
    vTaskResume(taskHandlerCamera);
    vTaskResume(taskHandlerDataStorage);
    vTaskResume(taskHandlerHouseKeeping);
    vTaskResume(taskHandlerPPT);
    vTaskResume(taskHandlerTTC);
    vTaskResume(taskHandlerWatchDog);
    MAP_WDT_A_startTimer();

}

void prvTaskRestart()
{
    uint8_t deletedTask;

    xQueueReceive(xQueueDeletedTasks, &deletedTask, 3);

    /*if (deletedTask == 1 || taskHandlerAodcs == NULL)
    {
        xTaskCreate((TaskFunction_t) aodcsTask, "AODCS Task",
        configMINIMAL_STACK_SIZE,
                    NULL, 1, &taskHandlerAodcs);

    }
    if (deletedTask == 2 || taskHandlerCamera == NULL)
    {

        xTaskCreate((TaskFunction_t) cameraTask, "CAMERA Task",
        configMINIMAL_STACK_SIZE,
                    NULL, 1, &taskHandlerCamera);

    }
    if (deletedTask == 3 || taskHandlerDataStorage == NULL)
    {
        xTaskCreate((TaskFunction_t) houseKeeping, "House Keeping", 1024, NULL,
                    2, &taskHandlerDataStorage);

    }
    if (deletedTask == 4 || taskHandlerHouseKeeping == NULL)
    {
        xTaskCreate((TaskFunction_t) dataStorage, "Data Storage", 1024, NULL, 1,
                    &taskHandlerHouseKeeping);

    }
    if (deletedTask == 5 || taskHandlerPPT == NULL)
    {
        xTaskCreate((TaskFunction_t) pptTask, "PPT Task",
        configMINIMAL_STACK_SIZE,
                    NULL, 1, &taskHandlerPPT);

    }*/
    if (deletedTask == 6 || taskHandlerTTC == NULL)
    {
        xTaskCreate((TaskFunction_t) ttcTask, "TT&C Task",
        configMINIMAL_STACK_SIZE,
                    NULL, 1, &taskHandlerTTC);
    }

}

void ADC14_IRQHandler(void)
{
    updateBatteryLevel();
    void setOperationMode(void);
}

#endif /* SRC_TASKMANAGER_C_ */
