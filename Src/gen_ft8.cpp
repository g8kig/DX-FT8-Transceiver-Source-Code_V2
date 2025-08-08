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

// INI file values
#define INI_VALUE_FREETEXT1 "Free text 1"
#define INI_VALUE_FREETEXT2 "Free text 2"
#define INI_VALUE_COMMENT "DX FT8 Transceiver"
#define INI_VALUE_MAX_TX_RETRIES 3
#define STR_FROM_VALUE(val) #val

// Default INI file content
#define DEFAULT_INI_CONTENT "[" INI_SECTION_STATION "]\n" INI_KEY_CALL " = \"FT8DX\"\n" INI_KEY_LOCATOR " = \"FN20\"\n" \
							"[" INI_SECTION_AUTOSEQ "]\n" INI_KEY_MAX_TX_RETRIES " = " STR_FROM_VALUE(INI_VALUE_MAX_TX_RETRIES) "\n"

// same order as BandIndex enum.
static const char *INI_KEY_BANDS[NumBands] = {"40", "30", "20", "17", "15", "12", "10"};
static uint16_t INI_VALUE_BANDS[NumBands] = {7074, 10136, 14075, 18101, 21075, 24916, 28075};

char Station_Call[CALLSIGN_SIZE];			  // seven character call sign (e.g. 3DA0XYZ) + optional /P + null terminator
char Station_Locator_Full[LOCATOR_FULL_SIZE]; // six character locator + null terminator (e.g. FN20fn)
char Station_Locator[LOCATOR_SIZE];			  // four character locator + null terminator (e.g. FN20)
char Target_Call[CALLSIGN_SIZE];			  // seven character call sign (e.g. 3DA0ABC) + optional /P + null terminator
char Target_Locator[LOCATOR_SIZE];			  // four character locator  + null terminator (e.g. FN20)
int Station_RSL;

static uint8_t isInitialized = 0;

/* Fatfs structure */
static FATFS FS;
static FIL fil2;
static const char *ini_file_name = "StationData.ini";

/* public */
char Free_Text1[MESSAGE_SIZE] = INI_VALUE_FREETEXT1;
char Free_Text2[MESSAGE_SIZE] = INI_VALUE_FREETEXT2;
char Comment[MESSAGE_SIZE] = INI_VALUE_COMMENT;

extern uint8_t _ssdram; /* Symbol defined in the linker script */

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
	for (size_t idx = 0; idx < strlen(Station_Call); ++idx)
	{
		if (!isprint((int)Station_Call[idx]))
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
		size_t idx = strlen(call_part);
		result = idx > 0 && idx < sizeof(Station_Call);
		if (result)
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
		size_t idx = strlen(locator_part);
		result = idx > 0 && idx < sizeof(Station_Locator_Full);
		if (result)
		{
			set_text(Station_Locator_Full, locator_part, -1);
			idx = LOCATOR_SIZE - 1;
			memcpy(Station_Locator, Station_Locator_Full, idx);
			Station_Locator[idx] = 0;
		}
	}
	return result;
}

static int setup_free_text(const char *free_text, int field_id)
{
	int result = 0;
	if (free_text != NULL)
	{
		size_t idx = strlen(free_text);
		switch (field_id)
		{
		case FreeText1:
			if (idx > 0 && idx < sizeof(Free_Text1))
			{
				set_text(Free_Text1, free_text, field_id);
				result = 1;
			}
			break;
		case FreeText2:
			if (idx > 0 && idx < sizeof(Free_Text2))
			{
				set_text(Free_Text2, free_text, field_id);
				result = 1;
			}
			break;
		default:
			result = 0;
		}
	}
	return result;
}

static void set_comment(const char *comment)
{
	if (comment != NULL && strlen(comment) > 0 && strlen(comment) < sizeof(Comment))
	{
		if (strcmp(comment, INI_VALUE_COMMENT) != 0)
		{
			set_text(Comment, comment, -1);
		}
		else
		{
			set_text(Comment, INI_VALUE_COMMENT, -1);
		}
	}
}

void Read_Station_File(void)
{
	Station_Call[0] = 0;
	Station_Locator[0] = 0;
	Station_Locator_Full[0] = 0;

	if (f_mount(&FS, SDPath, 1) != FR_OK)
		return;

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
			const ini_section_t *section = get_ini_section(&ini_data, INI_SECTION_STATION);
			if (section != NULL)
			{
				setup_station_call(get_ini_value_from_section(section, INI_KEY_CALL));
				setup_locator(get_ini_value_from_section(section, INI_KEY_LOCATOR));
			}

			section = get_ini_section(&ini_data, INI_SECTION_FREETEXT);
			if (section != NULL)
			{
				setup_free_text(get_ini_value_from_section(section, INI_KEY_FREETEXT1), FreeText1);
				setup_free_text(get_ini_value_from_section(section, INI_KEY_FREETEXT2), FreeText2);
			}

			section = get_ini_section(&ini_data, INI_SECTION_BANDDATA);
			if (section != NULL)
			{
				// see BandIndex
				for (int idx = _40M; idx <= _10M; ++idx)
				{
					const char *band_data = get_ini_value_from_section(section, INI_KEY_BANDS[idx]);
					if (band_data != NULL)
					{
						size_t band_data_size = strlen(band_data) + 1;
						if (band_data_size > 0 && band_data_size < BAND_DATA_SIZE)
						{
							sBand_Data[idx].Frequency = (uint16_t)(atof(band_data) * 1000);
							memcpy(sBand_Data[idx].display, band_data, band_data_size);
						}
					}
				}
			}

			// backwards compatibility
			const char *value = get_ini_value(&ini_data, "MISC", "COMMENT");
			set_comment(value);

			value = get_ini_value(&ini_data, INI_SECTION_MISC, INI_KEY_COMMENT);
			set_comment(value);

			value = get_ini_value(&ini_data, INI_SECTION_AUTOSEQ, INI_KEY_MAX_TX_RETRIES);
			if (value != NULL && strlen(value) > 0)
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

// write a key-value pair to the INI file
static int write_ini_key_value(const char *key, const char *value)
{
	char buffer[64];
	sprintf(buffer, "%s=%s\n", key, value);
	return f_puts(buffer, &fil2);
}

// write a key-value pair with a numeric value to the INI file
static int write_ini_key_value_numeric(const char *key, uint32_t value)
{
	char buffer[64];
	sprintf(buffer, "%s=%u\n", key, value);
	return f_puts(buffer, &fil2);
}

// write a key-value pair with a formatted numeric value to the INI file
static int write_ini_key_value_formatted_numeric(const char *key, uint32_t value)
{
	char buffer[64];
	sprintf(buffer, "%s=%u.%03u\n", key, value / 1000, value % 1000);
	return f_puts(buffer, &fil2);
}

void update_stationdata(void)
{
	FRESULT fres = f_mount(&FS, SDPath, 1);
	if (fres == FR_OK)
		fres = f_open(&fil2, ini_file_name, FA_CREATE_ALWAYS | FA_WRITE);
	if (fres == FR_OK)
	{
		fres = f_lseek(&fil2, 0);
		if (fres == FR_OK)
		{
			f_puts("[" INI_SECTION_STATION "]\n", &fil2);
			write_ini_key_value(INI_KEY_CALL, Station_Call);
			write_ini_key_value(INI_KEY_LOCATOR, Station_Locator_Full);

			bool freetext1_changed = strlen(Free_Text1) > 0 && strcmp(Free_Text1, INI_VALUE_FREETEXT1) != 0;
			bool freetext2_changed = strlen(Free_Text2) > 0 && strcmp(Free_Text2, INI_VALUE_FREETEXT2) != 0;
			if (freetext1_changed || freetext2_changed)
			{
				f_puts("[" INI_SECTION_FREETEXT "]\n", &fil2);
				if (freetext1_changed)
				{
					write_ini_key_value(INI_KEY_FREETEXT1, Free_Text1);
				}
				if (freetext2_changed)
				{
					write_ini_key_value(INI_KEY_FREETEXT2, Free_Text2);
				}
			}

			// detemine if band data has changed
			bool banddata_changed = false;
			for (int idx = _40M; idx <= _10M; ++idx)
			{
				if (INI_VALUE_BANDS[idx] != sBand_Data[idx].Frequency)
				{
					banddata_changed = true;
					break;
				}
			}

			// write band data if changed
			if (banddata_changed)
			{
				f_puts("[" INI_SECTION_BANDDATA "]\n", &fil2);
				for (int idx = _40M; idx <= _10M; ++idx)
				{
					if (INI_VALUE_BANDS[idx] == sBand_Data[idx].Frequency)
						continue; // skip unchanged bands

					write_ini_key_value_formatted_numeric(INI_KEY_BANDS[idx], sBand_Data[idx].Frequency);
				}
			}

			// have autosequence settings changed?
			if (INI_VALUE_MAX_TX_RETRIES != max_tx_retries)
			{
				f_puts("[" INI_SECTION_AUTOSEQ "]\n", &fil2);
				write_ini_key_value_numeric(INI_KEY_MAX_TX_RETRIES, max_tx_retries);
			}

			// has the comment changed?
			if (strcmp(Comment, INI_VALUE_COMMENT) != 0)
			{
				f_puts("[" INI_SECTION_MISC "]\n", &fil2);
				write_ini_key_value(INI_KEY_COMMENT, Comment);
			}
		}
	}
	f_close(&fil2);
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
		BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"Failed to initialise SDRAM", LEFT_MODE);
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
