/*
 * dataStorage.c
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
 * \file dataStorage.c
 *
 * \brief dataStorage file
 *
 * \author Guilherme Lion�o
 *
 */
#include "dataStorage.h"
#include "stdlib.h"

//------- SD VARIABLES --------------//
FATFS   sdDriver;           /*sd driver pointer*/
DIR     DI;
FILINFO FI;                 /**/
FIL     file;               /*file pointer*/
FIL     fileBin;
char    buffer[SIZE_OF_IMU_DATA];        /*buffer*/
UINT    br, bw;             /* File read/write count */
FRESULT fResult;            /*message handle*/
FILINFO FI;


//------- LOCAL VARIABLES --------------//

char buffer_timer[10]="";
char CTime;
char buffer[];
extern QueueHandle_t xQueueIMU;
extern SemaphoreHandle_t semaphoreIMU;
extern volatile dataPacket obcData;

ImuData imuData;
LocalTime localTime;

void getLocalTime(void);
void setWatchDogBit_DATASTORAGE(void);

void *dataStorage(void *pvParameters){

    /* The xLastWakeTime variable needs to be initialized with the current tick
     count. Note that this is the only time the variable is written to explicitly.
     After this xLastWakeTime is updated automatically internally within
     vTaskDelayUntil(). */
    portTickType xLastWakeTimeDataStorage = xTaskGetTickCount();

    //initializing the SPI protocol and mounting the SD card
    while(!initSD());

    memset(localTime.hour, 0x00, sizeof(localTime.hour));
    memset(localTime.minutes, 0x00, sizeof(localTime.hour));
    memset(localTime.seconds, 0x00, sizeof(localTime.hour));

    TickType_t tick;

    //try to open the telemetry file
    fResult = f_open(&file, "T_DATA.TXT",FA_OPEN_APPEND | FA_WRITE | FA_READ);

    //TODO: error handling

    while(1){

        //DEBUG SESSION
        #if DEBUG_SESSION
        MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P2, GPIO_PIN1); // PIN GREEN
        #endif

        //TODO: control of SPI interface

        //TODO: queue control

        //TIME STAMP
        getLocalTime();
        fResult = f_write(&file, "L_T:", sizeof("L_T:"), NULL);
        fResult=f_write(&file, localTime.hour, sizeof(localTime.hour), NULL);
        fResult=f_write(&file, ":", sizeof(":"), NULL);
        fResult=f_write(&file, localTime.minutes, sizeof(localTime.minutes), NULL);
        fResult=f_write(&file, ":", sizeof(":"), NULL);
        fResult=f_write(&file, localTime.seconds, sizeof(localTime.seconds), NULL);
        //xTaskGetTickCount();

        //MSP STATUS
        fResult = f_write(&file, " |MODE:", sizeof(" |MODE:"), NULL);
        fResult = f_write(&file, obcData.system_status, sizeof(obcData.system_status), &bw);

        tick=xTaskGetTickCount();
        fResult = f_write(&file, " |TEMP: ", sizeof("|TEMP: "), NULL);
        fResult = f_write(&file,itoa(obcData.internalTemperature,&buffer,DECIMAL), 2*sizeof(char), &bw);

        //SUBSYSTEM STATUS
        fResult = f_write(&file, " |ADC: ", sizeof("|ADC: "), NULL);
        fResult = f_write(&file,itoa(obcData.obc_sensors[0],&buffer,DECIMAL), 6*sizeof(char), &bw);
        fResult = f_write(&file,itoa(obcData.obc_sensors[1],&buffer,DECIMAL), 6*sizeof(char), &bw);
        fResult = f_write(&file,itoa(obcData.obc_sensors[2],&buffer,DECIMAL), 6*sizeof(char), &bw);
        fResult = f_write(&file,itoa(obcData.obc_sensors[3],&buffer,DECIMAL), 6*sizeof(char), &bw);
        fResult = f_write(&file,itoa(obcData.obc_sensors[4],&buffer,DECIMAL), 6*sizeof(char), &bw);

        //DATA
        if(xSemaphoreTake(semaphoreIMU,2000)){

            xQueueReceive(xQueueIMU,&imuData,200);

            xSemaphoreGive(semaphoreIMU);
        }
        fResult = f_write(&file, " |IMU:", sizeof(" |IMU:"), NULL);
        fResult = f_write(&file, imuData.ax, sizeof(imuData.ax), &bw);
        fResult = f_write(&file, imuData.ay, sizeof(imuData.ay), &bw);
        fResult = f_write(&file, imuData.az, sizeof(imuData.az), &bw);
        fResult = f_write(&file, imuData.gx, sizeof(imuData.gx), &bw);
        fResult = f_write(&file, imuData.gy, sizeof(imuData.gy), &bw);
        fResult = f_write(&file, imuData.gz, sizeof(imuData.gz), &bw);
        /* fResult = f_write(&file, imuData.mx, sizeof(imuData.az), &bw);
        fResult = f_write(&file, imuData.my, sizeof(imuData.az), &bw);
        fResult = f_write(&file, imuData.mz, sizeof(imuData.az), &bw);
         */

        //PAYLOAD

        fResult = f_write(&file,"\n", sizeof("\n"), &bw);
        f_sync(&file);

        //DEBUG SESSION
        #if DEBUG_SESSION
        //GPIO_toggleOutputOnPin(GPIO_PORT_P2, GPIO_PIN1);
        #endif

        setWatchDogBit_DATASTORAGE();
        // IF THE SYSTEM HAS LOW BATTERY THE TASK WILL USE A LONGER DELAY, OTHERWISE IT WILL USE A SHOETTER PERIOD

        (flag_lowBattery) ?
                vTaskDelayUntil(&xLastWakeTimeDataStorage,DATA_STORAGE_TICK_PERIOD_LOW_BATTERY) :
                vTaskDelayUntil(&xLastWakeTimeDataStorage, DATA_STORAGE_TICK_PERIOD);            //


    }

    //vTaskDelete( NULL );

}

dataPacket readPacket(void){

    dataPacket data ={0};

    data = obcData;

    return data;
}

/*get the current time on MSP432*/
void getLocalTime(void){

    itoa(RTC_C_getCalendarTime().hours, &localTime.hour, 10);
    itoa(RTC_C_getCalendarTime().minutes, &localTime.minutes, 10);
    itoa(RTC_C_getCalendarTime().seconds, &localTime.seconds, 10);

}


/*initialize the SD CARD*/

int initSD(){

    //CS_Init();              //init someting that I have to know, TODO

    SPI_Init(EUSCI_B0_BASE, SPI0MasterConfig);

    GPIO_Init(GPIO_PORT_P6, GPIO_PIN7);

    MAP_Interrupt_enableMaster();

    //mount the SD card
    fResult=f_mount(&sdDriver,"0",1);      //mount on the area 0 of the sd card

    if(fResult!=FR_OK){return FAILURE;}

    return SUCCESS;
}

void setWatchDogBit_DATASTORAGE(void){

    xEventGroupSetBits(WATCHDOG_EVENT_GROUP, DATASTORAGE_TASK_ID);

}
