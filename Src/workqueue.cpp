/* Copyright (c) 2025 Paul Winwood, G8KIG - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPL-Version 3 license.
 *
 * There is a copy of the GPL-Version 3 license in the same folder as this file.
 */

#include <stdint.h>
#include <memory.h>
#include "workqueue.h"
#include "stm32746g_discovery.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_i2c.h"

// in stm32746g_discovery.c
extern I2C_HandleTypeDef hI2cExtHandler;

static const int I2C_TIMEOUT = 1000;
static const uint8_t ESP32_I2C_ADDRESS = 0x2A;

#define MAX_WORK_ITEMS 20

static WorkItem workItems[MAX_WORK_ITEMS];

static int allocateWorkItem()
{
    int index = -1;
    for (int i = 0; i < MAX_WORK_ITEMS; i++)
    {
        if (workItems[i].state == ITEM_FREE)
        {
            workItems[i].state = ITEM_ALLOCATED;
            index = i;
            break;
        }
    }
    return index;
}

bool addWorkQueueItem(I2COperation operation, const uint8_t *buffer, int bufferSize)
{
    int index = allocateWorkItem();
    if (index >= 0)
    {
        WorkItem *workItem = workItems + index;
        workItem->operation = operation;
        if (buffer != NULL && bufferSize > 0)
        {
            memcpy(workItem->buffer, buffer, bufferSize);
            workItem->bufferSize = bufferSize;
        }
    }
    return (index >= 0);
}

void initialiseWorkQueue()
{
    memset(workItems, 0, sizeof(workItems));
}

void processWorkQueue()
{
    static uint8_t tempBuffer[1] = {0};

    HAL_StatusTypeDef status;
    for (int idx = 0; idx < MAX_WORK_ITEMS; idx++)
    {
        WorkItem *workItem = workItems + idx;
        if (workItem->state == ITEM_ALLOCATED)
        {
            I2COperation operation = workItem->operation;
            switch (operation)
            {
            case OP_TIME_REQUEST:
                break;
            case OP_SENDER_RECORD:
                // fall through intended
            case OP_SENDER_SOFTWARE_RECORD:
                // fall through intended
            case OP_RECEIVER_RECORD:
		        status = HAL_I2C_Mem_Write(&hI2cExtHandler,
										   ESP32_I2C_ADDRESS << 1,
										   operation,
										   I2C_MEMADD_SIZE_8BIT,
										   workItem->buffer,
                                           workItem->bufferSize,
										   I2C_TIMEOUT);
                break;
            case OP_SEND_REQUEST:
			    status = HAL_I2C_Mem_Write(&hI2cExtHandler,
                                           ESP32_I2C_ADDRESS << 1,
                                           OP_SENDER_SOFTWARE_RECORD,
                                           I2C_MEMADD_SIZE_8BIT,
                                           tempBuffer,
                                           sizeof(tempBuffer),
                                           I2C_TIMEOUT);
                break;
            }
            workItem->state = ITEM_FREE; // Mark the item as free after processing
        }
    }
}

bool requestTime(void *buffer, size_t bufferSize)
{    
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hI2cExtHandler,
                                                ESP32_I2C_ADDRESS << 1,
                                                OP_TIME_REQUEST,
                                                I2C_MEMADD_SIZE_8BIT,
                                                (uint8_t*)buffer,
                                                bufferSize,
                                                I2C_TIMEOUT);
    return (status == HAL_OK);
}
