
/* Copyright (C) 2025 Paul Winwood G8KIG - All Rights Reserved. */

#include <memory.h>

#include "main.h"
#include "DS3231.h"
#include "gen_ft8.h"
#include "PskInterface.h"
#include "workqueue.h"

struct RTC_Time
{
	uint8_t seconds; // 00-59 in BCD
	uint8_t minutes; // 00-59 in BCD
	uint8_t hours;	 // 00-23 in BCD
	uint8_t dayOfWeek;
	uint8_t day;
	uint8_t month;
	uint8_t year;
};
static const int MAX_SYNCTIME_REQUESTS = 10;

static bool syncTime = true;
static int syncTimeCounter = 0;
static bool senderSync = false;

void requestTimeSync(void)
{
	syncTime = true;
	// also cause the sender information to be resent
	senderSync = false;
	syncTimeCounter = 0;
}

void updateTime(void)
{
	if (syncTime && syncTimeCounter++ < MAX_SYNCTIME_REQUESTS)
	{
		RTC_Time rtcTime;
		memset(&rtcTime, 0, sizeof(rtcTime));
		bool status = requestTime(&rtcTime, sizeof(rtcTime));
		if (status)
		{
			if (rtcTime.year > 24 && rtcTime.year < 99)
			{
				char buffer[256];
				sprintf(buffer, "%u %2.2u:%2.2u:%2.2u %2.2u/%2.2u/%2.2u (%u)",
						status,
						rtcTime.hours, rtcTime.minutes, rtcTime.seconds,
						rtcTime.day, rtcTime.month, rtcTime.year,
						rtcTime.dayOfWeek);
				logger(buffer, __FILE__, __LINE__);

				RTC_setTime(rtcTime.hours, rtcTime.minutes, rtcTime.seconds, 0, 0);
				RTC_setDate(rtcTime.dayOfWeek, rtcTime.day, rtcTime.month, rtcTime.year);
				syncTime = false;
			}
		}
		else
		{
			char buffer[256];
			sprintf(buffer, "Time sync request failed: %d", status);
			logger(buffer, __FILE__, __LINE__);
			syncTime = false;
		}
	}
}

bool addSenderRecord(const char *callsign, const char *gridSquare, const char *software)
{
	bool result = false;
	uint8_t buffer[32];
	size_t callsignLength = strlen(callsign);
	size_t gridSquareLength = strlen(gridSquare);

	size_t bufferSize = sizeof(uint8_t) + callsignLength + sizeof(uint8_t) + gridSquareLength;
	if (bufferSize < sizeof(buffer))
	{
		uint8_t *ptr = buffer;

		// Add callsign as length-delimited
		*ptr++ = callsignLength;
		memcpy(ptr, callsign, callsignLength);
		ptr += callsignLength;

		// Add gridSquare as length-delimited
		*ptr++ = gridSquareLength;
		memcpy(ptr, gridSquare, gridSquareLength);
		ptr += gridSquareLength;

		result = addWorkQueueItem(OP_SENDER_RECORD, buffer, ptr - buffer);
	}

	if (result)
	{
		size_t softwareLength = strlen(software);
		bufferSize = sizeof(uint8_t) + softwareLength;
		if (bufferSize < sizeof(buffer))
		{
			uint8_t *ptr = buffer;

			// Add software description as length-delimited
			*ptr++ = softwareLength;
			memcpy(ptr, software, softwareLength);
			ptr += softwareLength;

			result = addWorkQueueItem(OP_SENDER_SOFTWARE_RECORD, buffer, ptr - buffer);
		}
	}
	return result;
}

bool addReceivedRecord(const char *callsign, uint32_t frequency, uint8_t snr)
{
	bool result = true;
	if (!senderSync)
	{
		result = addSenderRecord(Station_Call, Station_Locator_Full, Software);
		senderSync = true;
	}

	if (result)
	{
		uint8_t buffer[32];
		size_t callsignLength = strlen(callsign);
		size_t bufferSize = sizeof(uint8_t) + callsignLength + sizeof(uint32_t) + sizeof(uint8_t);
		if (bufferSize < sizeof(buffer))
		{
			uint8_t *ptr = buffer;

			// Add callsign as length-delimited
			*ptr++ = callsignLength;
			memcpy(ptr, callsign, callsignLength);
			ptr += callsignLength;

			// Add frequency
			memcpy(ptr, &frequency, sizeof(frequency));
			ptr += sizeof(frequency);

			// Add SNR (1 byte)
			*ptr++ = snr;

			result = addWorkQueueItem(OP_RECEIVER_RECORD, buffer, ptr - buffer);
		}
	}
	return result;
}

bool sendRequest(void)
{
	return addWorkQueueItem(OP_SEND_REQUEST, NULL, 0);
}
