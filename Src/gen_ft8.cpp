/*
 * gen_ft8.c
 *
 *  Created on: Oct 24, 2019
 *      Author: user
 */

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "constants.h"
#include "pack.h"
#ifdef __cplusplus
}
#endif
#include "encode.h"

#include "gen_ft8.h"

#include "diskio.h" /* Declarations of device I/O functions */
#include "ff.h"		/* Declarations of FatFs API */
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_sd.h"

#include "stm32746g_discovery_lcd.h"

#include "ff_gen_drv.h"
#include "sd_diskio.h"

#include "ADIF.h"
#include "Display.h"
#include "arm_math.h"
#include "decode_ft8.h"
#include "log_file.h"
#include "traffic_manager.h"

#include "autoseq_engine.h"
#include "button.h"
#include "ini.h"

// INI file section names
#define INI_SECTION_STATION "Station"
#define INI_SECTION_FREETEXT "FreeText"
#define INI_SECTION_BANDDATA "BandData"
#define INI_SECTION_MISC "Miscellaneous"
#define INI_SECTION_AUTOSEQ "AutoSequence"

// INI file key names
#define INI_KEY_CALL "Call"
#define INI_KEY_LOCATOR "Locator"
#define INI_KEY_FREETEXT1 "1"
#define INI_KEY_FREETEXT2 "2"
#define INI_KEY_COMMENT "Comment"
#define INI_KEY_MAX_TX_RETRIES "MaxTXRetries"

// Default INI file content
#define DEFAULT_INI_CONTENT "[" INI_SECTION_STATION "]\n"				\
							INI_KEY_CALL " = \"FT8DX\"\n" 				\
							INI_KEY_LOCATOR " = \"FN20\"\n"           	\
							"[" INI_SECTION_AUTOSEQ "]\n" 				\
							INI_KEY_MAX_TX_RETRIES " = 5\n"

static const char *INI_KEY_BANDS[NumBands] = {"40", "30", "20", "17","15", "12", "10"};

char Station_Call[CALLSIGN_SIZE];			  // seven character call sign (e.g. 3DA0XYZ) + optional /P + null terminator
char Station_Locator_Full[LOCATOR_FULL_SIZE]; // six character locator + null terminator (e.g. FN20fn)
char Station_Locator[LOCATOR_SIZE];			  // four character locator + null terminator (e.g. FN20)
char Target_Call[CALLSIGN_SIZE];			  // seven character call sign (e.g. 3DA0XYZ) + optional /P + null terminator
char Target_Locator[LOCATOR_SIZE];			  // four character locator  + null terminator (e.g. FN20)
int Station_RSL;

static uint8_t isInitialized = 0;

/* Fatfs structure */
static FATFS FS;
static FIL fil2;
static const char *ini_file_name = "StationData.ini";

char Free_Text1[MESSAGE_SIZE] = "Free text 1";
char Free_Text2[MESSAGE_SIZE] = "Free text 2";
char Comment[MESSAGE_SIZE] = "DX FT8 Transceiver";

void update_stationdata(void);

static void set_text(char *text, const char *source, int field_id)
{
	strcpy(text, source);
	for (int i = 0; i < (int)strlen(text); ++i)
	{
		if (!isprint((int)text[i]))
		{
			text[0] = 0;
			break;
		}
	}

	if (field_id >= 0)
	{
		sButtonData[field_id].text0 = text;
		sButtonData[field_id].text1 = text;
	}
}

static void validate_call()
{
	for (size_t i = 0; i < strlen(Station_Call); ++i)
	{
		if (!isprint((int)Station_Call[i]))
		{
			Station_Call[0] = 0;
			break;
		}
	}
}

static int setup_station_call(const char *call_part)
{
	int result = 0;
	if (call_part != NULL)
	{
		size_t i = strlen(call_part);
		result = i > 0 && i < sizeof(Station_Call) ? 1 : 0;
		if (result != 0)
		{
			strcpy(Station_Call, call_part);
			validate_call();
		}
	}
	return result;
}

static int setup_locator(const char *locator_part)
{
	int result = 0;
	if (locator_part != NULL)
	{
		size_t i = strlen(locator_part);
		result = i > 0 && i < sizeof(Station_Locator_Full) ? 1 : 0;
		if (result != 0)
		{
			set_text(Station_Locator_Full, locator_part, -1);
			memcpy(Station_Locator, Station_Locator_Full, LOCATOR_SIZE - 1);
			Station_Locator[LOCATOR_SIZE - 1] = 0;
		}
	}
	return result;
}

static int setup_free_text(const char *free_text, int field_id)
{
	int result = 0;
	if (free_text != NULL)
	{
		size_t i = strlen(free_text);
		switch (field_id)
		{
		case FreeText1:
			result = i < sizeof(Free_Text1) ? 1 : 0;
			if (i > 0 && result != 0)
				set_text(Free_Text1, free_text, field_id);
			break;
		case FreeText2:
			result = i < sizeof(Free_Text2) ? 1 : 0;
			if (i > 0 && result != 0)
				set_text(Free_Text2, free_text, field_id);
			break;
		default:
			result = 1;
		}
	}
	return result;
}

extern uint8_t _ssdram; /* Symbol defined in the linker script */

void Read_Station_File(void)
{
	Station_Call[0] = 0;
	Station_Locator[0] = 0;
	Station_Locator_Full[0] = 0;
	Free_Text1[0] = 0;
	Free_Text2[0] = 0;

	f_mount(&FS, SDPath, 1);

	FILINFO filInfo;
	memset(&filInfo, 0, sizeof(filInfo));

	if (f_stat(ini_file_name, &filInfo) == FR_OK &&
		f_open(&fil2, ini_file_name, FA_OPEN_EXISTING | FA_READ) == FR_OK)
	{
		char read_buffer[filInfo.fsize];
		f_lseek(&fil2, 0);

		unsigned bytes_read;
		if (f_read(&fil2, read_buffer, filInfo.fsize, &bytes_read) == FR_OK)
		{
			ini_data_t ini_data;
			parse_ini(read_buffer, bytes_read, &ini_data);
			const ini_section_t *section =
				get_ini_section(&ini_data, INI_SECTION_STATION);
			if (section != NULL)
			{
				setup_station_call(get_ini_value_from_section(section, INI_KEY_CALL));
				setup_locator(get_ini_value_from_section(section, INI_KEY_LOCATOR));
			}
			section = get_ini_section(&ini_data, INI_SECTION_FREETEXT);
			if (section != NULL)
			{
				setup_free_text(get_ini_value_from_section(section, INI_KEY_FREETEXT1),
								FreeText1);
				setup_free_text(get_ini_value_from_section(section, INI_KEY_FREETEXT2),
								FreeText2);
			}
			section = get_ini_section(&ini_data, INI_SECTION_BANDDATA);
			if (section != NULL)
			{
				// see BandIndex
				for (int idx = _40M; idx <= _10M; ++idx)
				{
					const char *band_data =
						get_ini_value_from_section(section, INI_KEY_BANDS[idx]);
					if (band_data != NULL)
					{
						size_t band_data_size = strlen(band_data) + 1;
						if (band_data_size < BAND_DATA_SIZE)
						{
							sBand_Data[idx].Frequency = (uint16_t)(atof(band_data) * 1000);
							memcpy(sBand_Data[idx].display, band_data, band_data_size);
						}
					}
				}
			}
			const char *value =
				get_ini_value(&ini_data, INI_SECTION_MISC, INI_KEY_COMMENT);
			if (value != NULL)
			{
				strcpy(Comment, value);
			}
			value =
				get_ini_value(&ini_data, INI_SECTION_AUTOSEQ, INI_KEY_MAX_TX_RETRIES);
			if (value != NULL)
			{
				max_tx_retries = atoi(value);
				if (max_tx_retries < 1)
					max_tx_retries = 1;
				if (max_tx_retries > 5)
					max_tx_retries = 5;
			}

			f_close(&fil2);
		}
	}
	else
	{
		FRESULT fres = f_open(&fil2, ini_file_name, FA_OPEN_ALWAYS | FA_WRITE);
		if (fres == FR_OK)
		{
			fres = f_lseek(&fil2, 0);
			if (fres == FR_OK)
			{
				f_puts(DEFAULT_INI_CONTENT, &fil2);
			}
		}
		f_close(&fil2);
	}
}

void SD_Initialize(void)
{
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillRect(0, 0, 480, 272);

	BSP_LCD_SetFont(&Font16);
	BSP_LCD_SetTextColor(LCD_COLOR_RED);

	if (isInitialized == 0)
	{
		if (BSP_SD_Init() == MSD_OK)
		{
			BSP_SD_ITConfig();
			isInitialized = 1;
			FATFS_LinkDriver(&SD_Driver, SDPath);
		}
		else
		{
			BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"Insert SD.", LEFT_MODE);
			while (BSP_SD_IsDetected() != SD_PRESENT)
			{
				HAL_Delay(100);
			}
			BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"Reboot Now.", LEFT_MODE);
		}
	}

	uint8_t result = BSP_SDRAM_Init();
	if (result == SDRAM_ERROR)
	{
		BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"Failed to initialise SDRAM",
								LEFT_MODE);
		for (;;)
		{
			HAL_Delay(100);
		}
	}
}

// Needed by autoseq_engine
void queue_custom_text(const char *tx_msg)
{
	uint8_t packed[K_BYTES];

	pack77(tx_msg, packed);
	genft8(packed, tones);
}

void update_stationdata(void)
{
	char write_buffer[64];

	FRESULT fres = f_mount(&FS, SDPath, 1);
	if (fres == FR_OK)
		fres = f_open(&fil2, ini_file_name, FA_CREATE_ALWAYS | FA_WRITE);
	if (fres == FR_OK)
	{
		fres = f_lseek(&fil2, 0);
		if (fres == FR_OK)
		{
			f_puts("[" INI_SECTION_STATION "]\n", &fil2);
			sprintf(write_buffer, INI_KEY_CALL "=%s\n" INI_KEY_LOCATOR "=%s\n", Station_Call, Station_Locator_Full);
			f_puts(write_buffer, &fil2);
			f_puts("[" INI_SECTION_FREETEXT "]\n", &fil2);
			sprintf(write_buffer, "1=%s\n", Free_Text1);
			f_puts(write_buffer, &fil2);
			sprintf(write_buffer, "2=%s\n", Free_Text2);
			f_puts(write_buffer, &fil2);
			f_puts("[" INI_SECTION_BANDDATA "]\n", &fil2);
			for (int idx = _40M; idx <= _10M; ++idx)
			{
				sprintf(write_buffer, "%s=%u.%03u\n", INI_KEY_BANDS[idx], sBand_Data[idx].Frequency / 1000, sBand_Data[idx].Frequency % 1000);
				f_puts(write_buffer, &fil2);
			}
			f_puts("[" INI_SECTION_MISC "]\n", &fil2);
			sprintf(write_buffer, INI_KEY_COMMENT "=\"%s\"\n", Comment);
			f_puts(write_buffer, &fil2);
		}
	}
	f_close(&fil2);
}
