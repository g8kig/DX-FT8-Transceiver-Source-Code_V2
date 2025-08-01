# Makefile for the DX FT8 Multiband Tablet Transceiver project
# Requires GNU Make and the ARM GCC toolchain from the PlatformIO toolset to be installed
# Optionally also requires the ST-Link tools for flashing the STM32F7 microcontroller
# Installation of stlink tools: For Linux Debian family install using "sudo apt -y install stlink-tools"
# For other plaforms see https://github.com/stlink-org/stlink/releases (not tested) 
# Install platformio-cli and then run 'pio pkg install --tool toolchain-gccarmnoneeabi@@~1.100301.0' to install the toolchain
# To install GNU Make on Windows see https://stackoverflow.com/questions/32127524/how-to-install-and-use-make-in-windows

# STM32 toolchain path
ifeq ($(strip $(HOME)),)
TOOLCHAIN_PATH = $(USERPROFILE)/.platformio/packages/toolchain-gccarmnoneeabi/bin
else
TOOLCHAIN_PATH = $(HOME)/.platformio/packages/toolchain-gccarmnoneeabi/bin/
endif

TOOLCHAIN_PATH := $(subst \,/,$(TOOLCHAIN_PATH))

CC = $(TOOLCHAIN_PATH)/arm-none-eabi-gcc
C++ = $(TOOLCHAIN_PATH)/arm-none-eabi-g++
SIZE = $(TOOLCHAIN_PATH)/arm-none-eabi-size
OBJDUMP = $(TOOLCHAIN_PATH)/arm-none-eabi-objdump
OBJCOPY = $(TOOLCHAIN_PATH)/arm-none-eabi-objcopy

CFLAGS = -mcpu=cortex-m7 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_STM32746G_DISCO -DUSE_IOEXPANDER \
         -Os -ffunction-sections --specs=nano.specs \
         -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb \
         -Wall -Wextra

CPPFLAGS = -mcpu=cortex-m7 -std=gnu++11 -g3 -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_STM32746G_DISCO -DUSE_IOEXPANDER \
         -Os -ffunction-sections --specs=nano.specs \
         -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb \
         -Wall -Wextra

EXTRA_INCLUDES = \
    -IDrivers/BSP/Common \
    -IDrivers/BSP/STM32746G_DISCOVERY \
    -IDrivers/BSP/exc7200 \
    -IDrivers/BSP/ft5336 \
    -IDrivers/BSP/mfxstm32l152 \
    -IDrivers/BSP/ov9655 \
    -IDrivers/BSP/s5k5cag \
    -IDrivers/BSP/st7735 \
    -IDrivers/BSP/stmpe811 \
    -IDrivers/BSP/ts3510 \
    -IDrivers/BSP/wm8994 \
    -IDrivers/BSP/rk043fn48h \
	-IFT8_library \
	-IMiddlewares/src \
    -IInc \
    -IDrivers/CMSIS/Include \
    -IDrivers/CMSIS/Device/ST/STM32F7xx/Include \
    -IDrivers/STM32F7xx_HAL_Driver/Inc \
    -IUtilities/Fonts

ASMFLAGS = -mcpu=cortex-m7 -g3 -c -Wall -Wextra -x assembler-with-cpp --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb

LDFLAGS = -mcpu=cortex-m7 -TSTM32F746NGHx_FLASH.ld --specs=nosys.specs -Wl,-Map=Katy.map -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group

ASM_SRCS = Drivers/CMSIS/Device/ST/STM32F7xx/Source/Templates/gcc/startup_stm32f746xx.s \
		   DSP_CMSIS/arm_bitreversal2.s

C_SRCS = \
Drivers/BSP/exc7200/exc7200.c \
Drivers/BSP/ft5336/ft5336.c \
Drivers/BSP/mfxstm32l152/mfxstm32l152.c \
Drivers/BSP/ov9655/ov9655.c \
Drivers/BSP/s5k5cag/s5k5cag.c \
Drivers/BSP/st7735/st7735.c \
Drivers/BSP/stmpe811/stmpe811.c \
Drivers/BSP/ts3510/ts3510.c \
Drivers/BSP/wm8994/wm8994.c \
Drivers/BSP/STM32746G_DISCOVERY/stm32746g_discovery.c \
Drivers/BSP/STM32746G_DISCOVERY/stm32746g_discovery_audio.c \
Drivers/BSP/STM32746G_DISCOVERY/stm32746g_discovery_eeprom.c \
Drivers/BSP/STM32746G_DISCOVERY/stm32746g_discovery_lcd.c \
Drivers/BSP/STM32746G_DISCOVERY/stm32746g_discovery_sd.c \
Drivers/BSP/STM32746G_DISCOVERY/stm32746g_discovery_sdram.c \
Drivers/BSP/STM32746G_DISCOVERY/stm32746g_discovery_ts.c \
Drivers/CMSIS/Device/ST/STM32F7xx/Source/Templates/system_stm32f7xx.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_can.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cec.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cortex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_crc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_crc_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cryp.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cryp_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dac.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dac_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dcmi.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dcmi_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma2d.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_eth.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_gpio.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_hash.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_hash_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_hcd.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2s.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_irda.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_iwdg.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_lptim.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_msp_template.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_nand.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_nor.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pcd.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pcd_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_qspi.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rng.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rtc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rtc_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sai.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sai_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sd.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sdram.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_smartcard.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_smartcard_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_spdifrx.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_spi.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sram.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_usart.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_wwdg.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_fmc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_sdmmc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_usb.c \
DSP_CMSIS/arm_bitreversal.c \
DSP_CMSIS/arm_cfft_q15.c \
DSP_CMSIS/arm_cfft_radix2_init_q15.c \
DSP_CMSIS/arm_cfft_radix2_q15.c \
DSP_CMSIS/arm_cfft_radix4_init_q15.c \
DSP_CMSIS/arm_cfft_radix4_q15.c \
DSP_CMSIS/arm_cmplx_mag_squared_q15.c \
DSP_CMSIS/arm_common_tables.c \
DSP_CMSIS/arm_const_structs.c \
DSP_CMSIS/arm_fir_decimate_q15.c \
DSP_CMSIS/arm_fir_q15.c \
DSP_CMSIS/arm_rfft_init_q15.c \
DSP_CMSIS/arm_rfft_q15.c \
DSP_CMSIS/arm_scale_q15.c \
DSP_CMSIS/arm_shift_q15.c \
FT8_library/constants.c \
FT8_library/decode.c \
FT8_library/encode.c \
FT8_library/ldpc.c \
FT8_library/pack.c \
FT8_library/text.c \
FT8_library/unpack.c \
Middlewares/src/ccsbcs.c \
Middlewares/src/diskio.c \
Middlewares/src/ff.c \
Middlewares/src/ff_gen_drv.c \
Middlewares/src/sdram_diskio.c \
Middlewares/src/sd_diskio.c \
Middlewares/src/unicode.c \
Middlewares/src/option/syscall.c \
Src/stm32f7xx_it.c

CPP_SRCS = \
Src/ADIF.cpp \
Src/autoseq_engine.cpp \
Src/button.cpp \
Src/Codec_Gains.cpp \
Src/decode_ft8.cpp \
Src/Display.cpp \
Src/DS3231.cpp \
Src/FIR_Coefficients.cpp \
Src/gen_ft8.cpp \
Src/Ini.cpp \
Src/log_file.cpp \
Src/main.cpp \
Src/options.cpp \
Src/Process_DSP.cpp \
Src/qso_display.cpp \
Src/SDR_Audio.cpp \
Src/SiLabs.cpp \
Src/Sine_table.cpp \
Src/traffic_manager.cpp

# Object files
OBJDIR = build

ASM_OBJS = $(addprefix $(OBJDIR)/,$(ASM_SRCS:.s=.o))
C_OBJS   = $(addprefix $(OBJDIR)/,$(C_SRCS:.c=.o))
CPP_OBJS = $(addprefix $(OBJDIR)/,$(CPP_SRCS:.cpp=.o))
OBJS     = $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS)

TARGET = Katy.elf

.PHONY: all clean flash

all: $(OBJDIR) $(TARGET) Katy.hex Katy.list

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(EXTRA_INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(C++) $(CPPFLAGS) $(EXTRA_INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASMFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

Katy.hex: $(TARGET)
	$(OBJCOPY) -O ihex $(TARGET) $@

Katy.list: $(TARGET)
	$(OBJDUMP) -h -S $(TARGET) > $@
	$(SIZE) $(TARGET)

flash: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET) Katy.bin
	st-info --probe
	st-flash --reset write Katy.bin 0x08000000
	rm -f Katy.bin

clean:
	rm -f $(OBJS) $(TARGET) Katy.hex Katy.list
	rm -rf $(OBJDIR)

