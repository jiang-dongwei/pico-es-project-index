# BoardConfigs.cmake
# 
# This file defines hardware configuration, board type, and pin assignments for supported boards.
# It is included from the main CMakeLists.txt of the project.
#
# Usage:
#   - Set HW_CONFIG to select the hardware configuration.
#   - Set the PICO_SDK_PATH environment variable to the Pico SDK location.
#   - Set the PICO_PIO_USB_PATH environment variable to the Pico-PIO-USB library location if PIO USB is needed.
#
# Notes:
#   - The latest master branch of TinyUSB is required for PIO USB support and for Adafruit Metro RP2350.
#   - The latest branch of https://github.com/sekigon-gonnoc/Pico-PIO-USB is required for PIO USB support.
#   - PIO USB is only supported on boards with sufficient memory (e.g., RP2350).
#
# check if $ENV{PICO_SDK_PATH} is set and points to a valid directory
if (NOT DEFINED ENV{PICO_SDK_PATH} OR NOT IS_DIRECTORY $ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH environment variable is not set or points to an invalid directory. Please set it to the path of the Pico SDK.")
endif()
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# set PICO_PIO_USB_PATH from environment variable
if(NOT DEFINED PICO_PIO_USB_PATH)
  if(DEFINED ENV{PICO_PIO_USB_PATH})
    set(PICO_PIO_USB_PATH $ENV{PICO_PIO_USB_PATH})
  endif()
endif()
# check if PICO_PIO_USB_PATH is set and points to a valid directory
if(DEFINED PICO_PIO_USB_PATH)
    if(NOT EXISTS ${PICO_PIO_USB_PATH}/src/pio_usb.c)
        message(FATAL_ERROR "Pico PIO usb repo not found in ${PICO_PIO_USB_PATH}. Please fetch the repo")
    endif()
else()
    # disable PIO USB support if PICO_PIO_USB_PATH is not set
    set(ENABLE_PIO_USB 0 CACHE BOOL "Enable PIO USB support")
endif()

# Default tinyusb board type
set(BOARD pico_sdk)
if (NOT DEFINED FORCE_DVI)
    set(FORCE_DVI 0 CACHE BOOL "Set to 1 to force DVI output instead of HSTX when available")
endif()
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/pico_shared/boards)
if ( HW_CONFIG EQUAL 1 )
	# This default Config is for Pimoroni Pico DV Demo Base, note uart is disabled because gpio 1 is used for NES controller
	set(DVICONFIG "dviConfig_PimoroniDemoDVSock" CACHE STRING
	  "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "0" CACHE STRING "Select the GPIO pin for LED")         # use 0 for onboard LED (PICO/PICO_W)
	set(SD_CS "22" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
	set(SD_SCK "5" CACHE STRING "Specify de Clock GPIO pin for the SD card")
	set(SD_MOSI "18" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
	set(SD_MISO "19" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card") 
	set(NES_CLK "14" CACHE STRING "Select the Clock GPIO pin for NES controller")
	set(NES_DATA "15" CACHE STRING "Select the Data GPIO pin for NES controller")
	set(NES_LAT "16" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "1" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "21" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "20" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
	set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
	set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
    set(UART_ENABLED 1 CACHE STRING "Enable UART output") # Disable UART output for Pimoroni Pico DV Demo Base
    set(USE_I2S_AUDIO "PICO_AUDIO_I2S_DRIVER_PCM5000A" CACHE STRING "Enable I2S audio output") # Enable I2S audio output for Pimoroni Pico DV Demo Base
    set(PICO_AUDIO_I2S_DATA_PIN 26 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE 27 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN -1 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 2 )
	# --------------------------------------------------------------------
	# Alternate config for use with different SDcard reader and HDMI board
    # Use also for Printed Circuit Board (PCB) version.
	# --------------------------------------------------------------------
	# Adafruit DVI Breakout For HDMI Source Devices https://www.adafruit.com/product/4984
	set(DVICONFIG "dviConfig_PicoDVISock" CACHE STRING
	  "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "0" CACHE STRING "Select the GPIO pin for LED")       # use 0 for onboard LED (PICO/PICO_W)
	# Adafruit Micro-SD breakout board+ https://www.adafruit.com/product/254 
	set(SD_CS "5" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
	set(SD_SCK "2" CACHE STRING "Specify de Clock GPIO pin for the SD card")
	set(SD_MOSI "3" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
	set(SD_MISO "4" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card") 
	set(NES_CLK "6" CACHE STRING "Select the Clock GPIO pin for NES controller")
	set(NES_DATA "7" CACHE STRING "Select the Data GPIO pin for NES controller")
	set(NES_LAT "8" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "9" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "10" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "11" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
	set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
	set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output") 
    # Disable I2S audio output for this board configuration for now.
    # Enable by uncommenting the line below and commenting the next line.
    #set(USE_I2S_AUDIO "PICO_AUDIO_I2S_DRIVER_PCM5000A" CACHE STRING "Enable I2S audio output")
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") # Enable I2S audio output for Pimoroni Pico DV Demo Base
    set(PICO_AUDIO_I2S_DATA_PIN 26 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE 27 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    # PIO USB settings
    # set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    # connect the DP+ pin to GPIO 20, DP- will be GPIO 21
    set(PIO_DP_PLUS_PIN 20 CACHE STRING "PIO USB DP pin.")
    # HSTX for this board configuration is possible. 
    if ( USE_HSTX EQUAL 1 AND NOT DEFINED GPIOHSTXD0 AND FORCE_DVI EQUAL 0)
      set(GPIOHSTXD0 12 CACHE STRING "HSTX D0+ pin")
      set(GPIOHSTXD1 18 CACHE STRING "HSTX D1+ pin")  
      set(GPIOHSTXD2 16 CACHE STRING "HSTX D2+ pin")
      set(GPIOHSTXCK 14 CACHE STRING "HSTX CK+ pin")
      set(GPIOHSTXINVERTED 0 CACHE STRING "Set to 1 if HSTX pins are inverted: D- = D+ - 1, not inverted (default): D- = D+ + 1")
    endif()
elseif ( HW_CONFIG EQUAL 3 )
	# --------------------------------------------------------------------
	# Alternate config for use with Adafruit Feather RP2040 DVI + SD Wing
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_AdafruitFeatherDVI" CACHE STRING
	  "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "13" CACHE STRING "Select the GPIO pin for LED")   # Adafruit Feather RP2040 onboard LED
	set(SD_CS "10" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
	set(SD_SCK "14" CACHE STRING "Specify de Clock GPIO pin for the SD card")
	set(SD_MOSI "15" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
	set(SD_MISO "8" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi1" CACHE STRING "Select the SPI bus for SD card")
	set(NES_CLK "5" CACHE STRING "Select the Clock GPIO pin for NES controller")
	set(NES_DATA "6" CACHE STRING "Select the Data GPIO pin for NES controller")
	set(NES_LAT "9" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "26" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "28" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "27" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
	set(WII_SDA "2" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
	set(WII_SCL "3" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output")
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN -1 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 4 )
    # --------------------------------------------------------------------
	# Alternate config for use with Waveshare RP2040-PiZero
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_WaveShareRp2040" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "-1" CACHE STRING "Select the GPIO pin for LED")   # No onboard LED for Waveshare RP2040-PiZero
    set(SD_CS "21" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "18" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "19" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "20" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "5" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "6" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "9" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "10" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "12" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "11" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "2" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "3" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output")
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    # NOTE! PIO USB does not work with Waveshare RP2040-PiZero when also using the hdmi. see https://www.waveshare.com/rp2040-pizero.htm
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN 6 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 5 )
    # --------------------------------------------------------------------
	# Adafruit Metro RP2350
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_AdafruitMetroRP2350" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "0" CACHE STRING "Select the GPIO pin for LED")   # Adafruit fruitjam onboard LED
    set(SD_CS "39" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "34" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "35" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "36" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "2" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "3" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "4" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "5" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "6" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "7" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "20" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "21" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c0" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output")
    #set(USE_I2S_AUDIO "PICO_AUDIO_I2S_DRIVER_TLV320" CACHE STRING "Enable external audio output") 
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable external audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN 11 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE 9 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN 7 CACHE STRING "Select the GPIO pin for I2S reset")
    set(BOARD "adafruit_metro_rp2350")         # found in $PICO__SDK_PATH/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/board.h
    set(PICO_BOARD "adafruit_metro_rp2350")    # found in $PICO__SDK_PATH/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/adafruit_metro_rp2350.h
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN -1 CACHE STRING "PIO USB DP pin.")
    set(NEEDS_LATEST_TINYUSB_SDK 1 CACHE BOOL "Set to 1 if the latest TinyUSB SDK is needed for this board")
    # HSTX pins for Adafruit Metro RP2350 
    if ( USE_HSTX EQUAL 1 AND NOT DEFINED GPIOHSTXD0 AND FORCE_DVI EQUAL 0)
        set(GPIOHSTXD0 18 CACHE STRING "HSTX D0+ pin")
        set(GPIOHSTXD1 16 CACHE STRING "HSTX D1+ pin")  
        set(GPIOHSTXD2 12 CACHE STRING "HSTX D2+ pin")
        set(GPIOHSTXCK 14 CACHE STRING "HSTX CK+ pin")
        set(GPIOHSTXINVERTED 0 CACHE STRING "Set to 1 if HSTX pins are inverted: D- = D+ - 1, not inverted (default): D- = D+ + 1")
   endif()
    
   
elseif ( HW_CONFIG EQUAL 6 )
    # --------------------------------------------------------------------
	# RP2040/RP2350 Zero With PCB
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_RP2XX0_TinyPCB" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "-1" CACHE STRING "Select the GPIO pin for LED")  
    set(SD_CS "5" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "2" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "3" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "4" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "14" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "26" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "15" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "27" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "29" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "28" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c0" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output") 
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN -1 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 7 )
    # --------------------------------------------------------------------
	# WaveShare RP2350-PiZero
	# --------------------------------------------------------------------
    set(PICO_BOARD "waveshare_rp2350_pizero" )
	set(DVICONFIG "dviConfig_WaveShareRp2350" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "-1" CACHE STRING "Select the GPIO pin for LED")  
    set(SD_CS "43" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "30" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "31" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "40" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi1" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "5" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "6" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "9" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "10" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "12" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "11" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "2" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "3" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output") 
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN 28 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 8 )
    # --------------------------------------------------------------------
	# Adafruit FruitJam
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_AdafruitFruitJam" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "0" CACHE STRING "Select the GPIO pin for LED")   # Adafruit fruitjam onboard LED
    set(SD_CS "39" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "34" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "35" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "36" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "-1" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "3" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "4" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "-1" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "6" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "7" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "20" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "21" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c0" CACHE STRING "Select the I2C bus for Wii Classic controller")
    set(WIIPAD_DELAYED_START 1 CACHE BOOL "Set to 1 to enable delayed start for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output")
    set(USE_I2S_AUDIO "PICO_AUDIO_I2S_DRIVER_TLV320" CACHE STRING "Enable external audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN 24 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE 26 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN 22 CACHE STRING "Select the GPIO pin for I2S reset")
    set(PICO_AUDIO_I2S_INTERRUPT_PIN 23 CACHE STRING "Select the GPIO pin for I2S interrupt")
    set(PICO_AUDIO_I2S_INTERRUPT_IS_BUTTON 0 CACHE STRING "Set to 1 if the I2S interrupt pin is a button")
    set(BOARD "adafruit_fruit_jam")         # found in $PICO__SDK_PATH/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/board.h
    set(PICO_BOARD "adafruit_fruit_jam")    # found in $PICO__SDK_PATH/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/adafruit_metro_rp2350.h
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN -1 CACHE STRING "PIO USB DP pin.")
    set(NEEDS_LATEST_TINYUSB_SDK 1 CACHE BOOL "Set to 1 if the latest TinyUSB SDK is needed for this board")
    # HSTX pins for Adafruit Fruit Jam
    if ( USE_HSTX EQUAL 1 AND NOT DEFINED GPIOHSTXD0 AND FORCE_DVI EQUAL 0)
        set(GPIOHSTXD0 15 CACHE STRING "HSTX D0+ pin")
        set(GPIOHSTXD1 17 CACHE STRING "HSTX D1+ pin")  
        set(GPIOHSTXD2 19 CACHE STRING "HSTX D2+ pin")
        set(GPIOHSTXCK 13 CACHE STRING "HSTX CK+ pin")
        set(GPIOHSTXINVERTED 1 CACHE STRING "Set to 1 if HSTX pins are inverted: D- = D+ - 1, not inverted (default): D- = D+ + 1")
   endif()
   set(ENABLE_VU_METER 1 CACHE BOOL "Enable VU meter (fruitjam NeoPixel leds)")
   # Override default UART settings used for printf debugging on the Adafruit Fruit Jam board.
   # By default, the following UART configuration is set in:
   #   $PICO_SDK_PATH/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_fruit_jam/adafruit_fruit_jam.h
   #     PICO_DEFAULT_UART        1
   #     PICO_DEFAULT_UART_TX_PIN 8
   #     PICO_DEFAULT_UART_RX_PIN 9
   # However, the ESP32 WiFi module on the board uses these pins for its own output,
   # which prevents printf messages from appearing. To fix this, we need to remap the UART
   # to different, unused GPIOs.
   set (PICO_DEFAULT_UART 0 CACHE STRING "Select the default UART for this board") # Adafruit Fruit Jam uses UART0
   set (PICO_DEFAULT_UART_TX_PIN 44 CACHE STRING "Select the GPIO pin for UART TX") # Adafruit Fruit Jam uses GPIO 44 for UART TX
   set (PICO_DEFAULT_UART_RX_PIN 45 CACHE STRING "Select the GPIO pin for UART RX") # Adafruit Fruit Jam uses GPIO 45 for UART RX
elseif ( HW_CONFIG EQUAL 9 )
    # --------------------------------------------------------------------
	# WaveShare RP2350-USB-A
	# --------------------------------------------------------------------
		set(DVICONFIG "dviConfig_WaveShare2350USBA" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "-1" CACHE STRING "Select the GPIO pin for LED")  
    set(SD_CS "5" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "6" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "3" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "4" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "-1" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "-1" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "-1" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "-1" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "-1" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "-1" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output") 
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN 12 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 10 )
	# --------------------------------------------------------------------
	# Spotpear HDMI
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_Spotpear" CACHE STRING
	  "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "0" CACHE STRING "Select the GPIO pin for LED")       # use 0 for onboard LED (PICO/PICO_W)
	# Adafruit Micro-SD breakout board+ https://www.adafruit.com/product/254 
	set(SD_CS "5" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
	set(SD_SCK "2" CACHE STRING "Specify de Clock GPIO pin for the SD card")
	set(SD_MOSI "3" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
	set(SD_MISO "4" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card") 
	set(NES_CLK "-1" CACHE STRING "Select the Clock GPIO pin for NES controller")
	set(NES_DATA "7" CACHE STRING "Select the Data GPIO pin for NES controller")
	set(NES_LAT "8" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "-1" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "10" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "11" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
	set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
	set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output") 
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") # Enable I2S audio output for Pimoroni Pico DV Demo Base
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    # connect the DP+ pin to GPIO 20, DP- will be GPIO 21
    set(PIO_DP_PLUS_PIN 20 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 11 )
   # --------------------------------------------------------------------
	# WaveShare RP2350-USB-A - OLD config with different SD pins, now deprecated.
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_WaveShare2350USBA_OLDConfig" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "-1" CACHE STRING "Select the GPIO pin for LED")  
    set(SD_CS "26" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "2" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "0" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "1" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "-1" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "-1" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "-1" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "-1" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "-1" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "-1" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
	set(UART_ENABLED 0 CACHE STRING "Enable UART output") 
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN 12 CACHE STRING "PIO USB DP pin.")
elseif ( HW_CONFIG EQUAL 12 )
    # --------------------------------------------------------------------    
    # Murmulator M1
    # --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_PimoroniDemoDVSock" CACHE STRING "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "0" CACHE STRING "Select the GPIO pin for LED")  # use 0 for onboard LED (Pico/Pico2)
	set(SD_CS "5" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
	set(SD_SCK "2" CACHE STRING "Specify de Clock GPIO pin for the SD card")
	set(SD_MOSI "3" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
	set(SD_MISO "4" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card") 
	set(NES_CLK "14" CACHE STRING "Select the Clock GPIO pin for NES controller")
	set(NES_DATA "16" CACHE STRING "Select the Data GPIO pin for NES controller")
	set(NES_LAT "15" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "14" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "17" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "15" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
	set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
	set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
    set(UART_ENABLED 0 CACHE STRING "Enable UART output") # Disable UART
    set(USE_I2S_AUDIO "PICO_AUDIO_I2S_DRIVER_PCM5000A" CACHE STRING "Enable I2S audio output")
    set(PICO_AUDIO_I2S_DATA_PIN 26 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE 27 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN -1 CACHE STRING "PIO USB DP pin.")
    set(PSRAM_CS_PIN 19 CACHE STRING "Select the GPIO pin for PSRAM chip select")
elseif ( HW_CONFIG EQUAL 13 )
    # --------------------------------------------------------------------
    # Murmulator M2
    # --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_Murmulator_M2" CACHE STRING "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "0" CACHE STRING "Select the GPIO pin for LED") # use 0 for onboard LED (Pico/Pico2)
    set(SD_CS     "5" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK    "6" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI   "7" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO   "4" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK  "20" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "26" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT  "21" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO  "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1  "20" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "27" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1  "21" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "0" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c0" CACHE STRING "Select the I2C bus for Wii Classic controller")
    set(WIIPAD_DELAYED_START 1 CACHE BOOL "Set to 1 to enable delayed start for Wii Classic controller")
    set(UART_ENABLED 0 CACHE STRING "Enable UART output")
    set(USE_I2S_AUDIO "PICO_AUDIO_I2S_DRIVER_PCM5000A" CACHE STRING "Enable I2S audio output")
    set(PICO_AUDIO_I2S_DATA_PIN 9 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE 10 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN -1 CACHE STRING "PIO USB DP pin.")
    set(NEEDS_LATEST_TINYUSB_SDK 1 CACHE BOOL "Set to 1 if the latest TinyUSB SDK is needed for this board")
    if ( USE_HSTX EQUAL 1 AND NOT DEFINED GPIOHSTXD0 AND FORCE_DVI EQUAL 0)  # HSTX pins for Murmulator M2
        set(GPIOHSTXCK 13 CACHE STRING "HSTX CK+ pin")
        set(GPIOHSTXD0 15 CACHE STRING "HSTX D0+ pin")
        set(GPIOHSTXD1 17 CACHE STRING "HSTX D1+ pin")  
        set(GPIOHSTXD2 19 CACHE STRING "HSTX D2+ pin")
        set(GPIOHSTXINVERTED 1 CACHE STRING "Set to 1 if HSTX pins are inverted: D- = D+ - 1, not inverted (default): D- = D+ + 1")
   endif()
   set(PSRAM_CS_PIN 8 CACHE STRING "Select the GPIO pin for PSRAM chip select")
elseif ( HW_CONFIG EQUAL 14 )
   # --------------------------------------------------------------------
	# Adafruit Feather RP2350
	# --------------------------------------------------------------------
	set(DVICONFIG "dviConfig_AdafruitMetroRP2350" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "7" CACHE STRING "Select the GPIO pin for LED")   # Adafruit fruitjam onboard LED
    set(SD_CS "5" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "6" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "23" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "4" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi0" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "-1" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "-1" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "-1" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "-1" CACHE STRING "Select the Clock GPIO pin for second NES controller")
	set(NES_DATA_1 "-1" CACHE STRING "Select the Data GPIO pin for second NES controller")
	set(NES_LAT_1 "-1" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "2" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "3" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
    set(WIIPAD_DELAYED_START 1 CACHE BOOL "Set to 1 to enable delayed start for Wii Classic controller")
	set(UART_ENABLED 1 CACHE STRING "Enable UART output")
    set(USE_I2S_AUDIO "PICO_AUDIO_I2S_DRIVER_TLV320" CACHE STRING "Enable external audio output") 
    #set(USE_I2S_AUDIO 0 CACHE STRING "Enable external audio output") 
    set(PICO_AUDIO_I2S_DATA_PIN 11 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE 9 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN 26 CACHE STRING "Select the GPIO pin for I2S reset")
    #set(BOARD "adafruit_metro_rp2350")         # found in $PICO__SDK_PATH/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/board.h
    #set(PICO_BOARD "adafruit_metro_rp2350")    # found in $PICO__SDK_PATH/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/adafruit_metro_rp2350.h
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN 24 CACHE STRING "PIO USB DP pin.")
    set(NEEDS_LATEST_TINYUSB_SDK 1 CACHE BOOL "Set to 1 if the latest TinyUSB SDK is needed for this board")
    # HSTX pins for Adafruit Metro RP2350 
    if ( USE_HSTX EQUAL 1 AND NOT DEFINED GPIOHSTXD0 AND FORCE_DVI EQUAL 0)
        set(GPIOHSTXD0 18 CACHE STRING "HSTX D0+ pin")
        set(GPIOHSTXD1 16 CACHE STRING "HSTX D1+ pin")  
        set(GPIOHSTXD2 12 CACHE STRING "HSTX D2+ pin")
        set(GPIOHSTXCK 14 CACHE STRING "HSTX CK+ pin")
        set(GPIOHSTXINVERTED 0 CACHE STRING "Set to 1 if HSTX pins are inverted: D- = D+ - 1, not inverted (default): D- = D+ + 1")
   endif()
   set(PSRAM_CS_PIN 8 CACHE STRING "Select the GPIO pin for PSRAM chip select")
elseif ( HW_CONFIG EQUAL 15 )
    # --------------------------------------------------------------------
    # Retro_PicoES V2.0 (RP2350B) - custom board with on-board HDMI, TF card,
    # dual NES game ports, dual USB-A (PIO USB) and optional PSRAM on GPIO47.
    #
    # The board exposes two PIO USB ports (both wired into TinyUSB):
    #   Port 1: DP=GPIO28, DM=GPIO29  (default root port via tuh_configure)
    #   Port 2: DP=GPIO26, DM=GPIO27  (added via pio_usb_host_add_port)
    #
    # HSTX is not available on the HDMI pins (GPIO32-39) of this board, so
    # PicoDVI (PIO) is used.
    # --------------------------------------------------------------------
    set(DVICONFIG "dviConfig_RetroPicoES" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "-1" CACHE STRING "Select the GPIO pin for LED")
    set(SD_CS "43" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "30" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "31" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "40" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi1" CACHE STRING "Select the SPI bus for SD card")
    set(NES_CLK "6" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "7" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "8" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "9" CACHE STRING "Select the Clock GPIO pin for second NES controller")
    set(NES_DATA_1 "10" CACHE STRING "Select the Data GPIO pin for second NES controller")
    set(NES_LAT_1 "11" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
    set(UART_ENABLED 1 CACHE STRING "Enable UART output")
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output")
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN 28 CACHE STRING "PIO USB DP pin.")
    set(PIO_DP_PLUS_PIN_2 26 CACHE STRING "Second PIO USB DP pin (optional second root port).")
    set(PSRAM_CS_PIN 47 CACHE STRING "Select the GPIO pin for PSRAM chip select")
elseif ( HW_CONFIG EQUAL 16 )
    # --------------------------------------------------------------------
    # Retro_PicoES V2.1 (RP2350B) - same custom board as HW_CONFIG 15 but
    # with the HDMI lines moved from GPIO32-39 to GPIO12-19. GPIO12-19 are
    # the RP2350's HSTX-capable pins, so this revision uses the hardware
    # HSTX peripheral instead of PIO-driven PicoDVI. PIO0 is now free for
    # other uses.
    #
    # HDMI pin map (per V2.1 schematic):
    #   D0+ = GPIO16, D0- = GPIO17
    #   D1+ = GPIO14, D1- = GPIO15
    #   D2+ = GPIO12, D2- = GPIO13
    #   CK+ = GPIO18, CK- = GPIO19
    # All pairs are non-inverted (D- = D+ + 1).
    #
    # The R28/R30 pull-up footprints are now correctly marked NC on the
    # V2.1 schematic; default population is no external pull-up on D+.
    #
    # PIO USB, SD card, controllers and PSRAM pinning are unchanged from
    # V2.0 (see HW_CONFIG 15 above for the rationale).
    # --------------------------------------------------------------------
    set(DVICONFIG "dviConfig_RetroPicoESv21" CACHE STRING
    "Select a default pin configuration from common_dvi_pin_configs.h")
    set(LED_GPIO_PIN "-1" CACHE STRING "Select the GPIO pin for LED")
    set(SD_CS "43" CACHE STRING "Specify the Chip Select GPIO pin for the SD card")
    set(SD_SCK "30" CACHE STRING "Specify de Clock GPIO pin for the SD card")
    set(SD_MOSI "31" CACHE STRING "Select the Master Out Slave In GPIO pin for the SD card")
    set(SD_MISO "40" CACHE STRING "Select the Master In Slave Out GPIO pin for the SD card")
    set(SD_SPI "spi1" CACHE STRING "Select the SPI bus for SD card")
    # NES controller pins per the V2.1 schematic GAME_CONN netlist:
    #   Port 1 (J1): Clock=GPIO6, Latch=GPIO7, Data=GPIO8
    #   Port 2 (J3): Clock=GPIO9, Latch=GPIO10, Data=GPIO11
    # NOTE: V2.0 had DATA/LATCH swapped relative to this; V2.1 follows its own
    # schematic, so DATA and LAT are the reverse of the V2.0 software mapping.
    set(NES_CLK "6" CACHE STRING "Select the Clock GPIO pin for NES controller")
    set(NES_DATA "8" CACHE STRING "Select the Data GPIO pin for NES controller")
    set(NES_LAT "7" CACHE STRING "Select the Latch GPIO pin for NES controller")
    set(NES_PIO "pio1" CACHE STRING "Select the PIO for NES controller")
    set(NES_CLK_1 "9" CACHE STRING "Select the Clock GPIO pin for second NES controller")
    set(NES_DATA_1 "11" CACHE STRING "Select the Data GPIO pin for second NES controller")
    set(NES_LAT_1 "10" CACHE STRING "Select the Latch GPIO pin for second NES controller")
    set(NES_PIO_1 "pio1" CACHE STRING "Select the PIO for second NES controller")
    set(WII_SDA "-1" CACHE STRING "Select the SDA GPIO pin for Wii Classic controller")
    set(WII_SCL "-1" CACHE STRING "Select the SCL GPIO pin for Wii Classic controller")
    set(WIIPAD_I2C "i2c1" CACHE STRING "Select the I2C bus for Wii Classic controller")
    set(UART_ENABLED 1 CACHE STRING "Enable UART output")
    # Route stdio UART0 to GPIO 2 (TX) / GPIO 3 (RX). On RP2350 these are
    # UART0 TX/RX via funcsel 11 (UART_AUX); pico-sdk's UART_FUNCSEL_NUM
    # picks the right funcsel automatically.
    set(PICO_DEFAULT_UART_TX_PIN 2 CACHE STRING "UART TX pin")
    set(PICO_DEFAULT_UART_RX_PIN 3 CACHE STRING "UART RX pin")
    set(USE_I2S_AUDIO 0 CACHE STRING "Enable I2S audio output")
    set(PICO_AUDIO_I2S_DATA_PIN -1 CACHE STRING "Select the GPIO pin for I2S data")
    set(PICO_AUDIO_I2S_CLOCK_PIN_BASE -1 CACHE STRING "Select the GPIO pin for I2S clock")
    set(PICO_AUDIO_I2S_PIO 1 CACHE STRING "Select the PIO for I2S audio output")
    set(PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0 CACHE STRING "Set to 1 if the I2S clock pins are swapped")
    set(PICO_AUDIO_I2S_RESET_PIN -1 CACHE STRING "Select the GPIO pin for I2S reset")
    set(ENABLE_PIO_USB 1 CACHE BOOL "Enable PIO USB support")
    set(PIO_USB_USE_PIO 2 CACHE BOOL "Select the PIO used for PIO-USB")
    set(PIO_DP_PLUS_PIN 28 CACHE STRING "PIO USB DP pin.")
    set(PIO_DP_PLUS_PIN_2 26 CACHE STRING "Second PIO USB DP pin (optional second root port).")
    set(PSRAM_CS_PIN 47 CACHE STRING "Select the GPIO pin for PSRAM chip select")
    # HSTX configuration for HDMI on GPIO12-19 (matches v2.1 schematic netlist:
    # GPIO16/17 = D0+/-, GPIO14/15 = D1+/-, GPIO12/13 = D2+/-, GPIO18/19 = CK+/-).
    if ( USE_HSTX EQUAL 1 AND NOT DEFINED GPIOHSTXD0 AND FORCE_DVI EQUAL 0)
        set(GPIOHSTXD0 16 CACHE STRING "HSTX D0+ pin")
        set(GPIOHSTXD1 14 CACHE STRING "HSTX D1+ pin")
        set(GPIOHSTXD2 12 CACHE STRING "HSTX D2+ pin")
        set(GPIOHSTXCK 18 CACHE STRING "HSTX CK+ pin")
        set(GPIOHSTXINVERTED 0 CACHE STRING "Set to 1 if HSTX pins are inverted: D- = D+ - 1, not inverted (default): D- = D+ + 1")
    endif()
endif ( )
# --------------------------------------------------------------------
if (NOT DEFINED ENABLE_VU_METER)
    set(ENABLE_VU_METER 0 CACHE BOOL "Enable VU meter (fruitjam NeoPixel leds)")
endif()
if ( NOT DEFINED PICO_AUDIO_I2S_INTERRUPT_PIN )
    set(PICO_AUDIO_I2S_INTERRUPT_PIN -1 CACHE STRING "Select the GPIO pin for I2S interrupt")
endif()
if ( NOT DEFINED PICO_AUDIO_I2S_INTERRUPT_IS_BUTTON )
    set(PICO_AUDIO_I2S_INTERRUPT_IS_BUTTON 0 CACHE STRING "Set to 1 if the I2S interrupt pin is a button")
endif()
if (NOT DEFINED GPIOHSTXD0)
    set(GPIOHSTXD0 0 CACHE STRING "HSTX D0 pin")
    set(GPIOHSTXD1 0 CACHE STRING "HSTX D1 pin")
    set(GPIOHSTXD2 0 CACHE STRING "HSTX D2 pin")
    set(GPIOHSTXCK 0 CACHE STRING "HSTX CK pin")
    set(GPIOHSTXINVERTED 0 CACHE STRING "Set to 1 if HSTX pins are inverted: D- = D+ - 1, not inverted (default): D- = D+ + 1")
endif()

if (NOT DEFINED ENABLE_PIO_USB )
    set(ENABLE_PIO_USB 0 CACHE BOOL "Enable PIO USB support")
endif()
# Optional second PIO USB root port (-1 disables)
if (NOT DEFINED PIO_DP_PLUS_PIN_2)
    set(PIO_DP_PLUS_PIN_2 -1 CACHE STRING "Second PIO USB DP pin (optional second root port).")
endif()
# Latest TinyUSB SDK is needed for Adafruit Metro RP2350 or when PIO USB is enabled
if (ENABLE_PIO_USB EQUAL 1)
    set(NEEDS_LATEST_TINYUSB_SDK 1 CACHE BOOL "Set to 1 if the latest TinyUSB SDK is needed for this board")
endif()
# If the board is not Adafruit Metro RP2350 and PIO USB is not enabled, set NEEDS_LATEST_TINYUSB_SDK to 0
if (NOT DEFINED NEEDS_LATEST_TINYUSB_SDK)
    set(NEEDS_LATEST_TINYUSB_SDK 0 CACHE BOOL "Set to 1 if the latest TinyUSB SDK is needed for this board")
endif()

if (NEEDS_LATEST_TINYUSB_SDK EQUAL 1)
    # ensure the file ${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/adafruit_metro_rp2350.h exists
    # This to check whether the latest master branch of TinyUsb is used.
    # This is needed for supporting the Adafruit Metro RP2350 or when PIO USB is enabled.
    if (NOT EXISTS ${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/rp2040/boards/adafruit_metro_rp2350/adafruit_metro_rp2350.h)
        # The metro board dir is only a heuristic for "recent TinyUSB". The SDK
        # 2.2.x-pinned TinyUSB (86ad6e5) lacks that board dir but DOES have
        # family_configure_target + PIO USB host (hcd_pio_usb.c), which is all we
        # need. Downgrade to a warning so the 2.2.x dev build proceeds.
        message(WARNING [[
TinyUSB metro board dir not found; proceeding anyway (PIO USB host is present).
If PIO USB fails to build, pull a newer TinyUSB master.
               ]]
               )
    endif()
endif()
# Default PSRAM chip select pin. (RP2350 only)
# The emulator will try to detect if PSRAM is present. If so it will use it for loading roms.
# otherwise it will use flash memory to load roms.
if ( NOT DEFINED PSRAM_CS_PIN )
    # Pimoroni Pico Plus 2
    # Adafruit Metro RP2350
    set(PSRAM_CS_PIN 47 CACHE STRING "Select the GPIO pin for PSRAM chip select")
endif()
if ( NOT DEFINED WIIPAD_DELAYED_START )
    set(WIIPAD_DELAYED_START 0 CACHE BOOL "Set to 1 to enable delayed start for Wii Classic controller")
endif()
# --------------------------------------------------------------------
message("Hardware configuration: ${HW_CONFIG}")
message("Pico SDK board type   : ${PICO_BOARD}")
message("Tinyusb board type    : ${BOARD}")
#message("Using board header dir: ${PICO_BOARD_HEADER_DIRS}")  

if ( ENABLE_PIO_USB EQUAL 1 )
    message("PIO USB enabled, using PIO USB driver.")
    if ( PIO_DP_PLUS_PIN EQUAL -1 )
        message("PIO USB DP+           : From board config -> ${BOARD}")
    else()
        message("PIO USB DP+           : ${PIO_DP_PLUS_PIN}")
    endif()
    message("PIO USB PIO           : pio${PIO_USB_USE_PIO}")
    message("PIO USB path          : ${PICO_PIO_USB_PATH}")
else()
    message("PIO USB is disabled, using default USB driver.")
endif()
message("HDMI board type       : ${DVICONFIG}")
message("SD card CS            : ${SD_CS}")
message("SD card SCK           : ${SD_SCK}")
message("SD card MOSI          : ${SD_MOSI}")
message("SD card MISO          : ${SD_MISO}")
message("SD card SPI           : ${SD_SPI}")
message("NES controller 0 CLK  : ${NES_CLK}")
message("NES controller 0 DATA : ${NES_DATA}")
message("NES controller 0 LAT  : ${NES_LAT}")
message("NES controller 0 PIO  : ${NES_PIO}")
message("NES controller 1 CLK  : ${NES_CLK_1}")
message("NES controller 1 DATA : ${NES_DATA_1}")
message("NES controller 1 LAT  : ${NES_LAT_1}")
message("NES controller 1 PIO  : ${NES_PIO_1}")
message("Wii controller SDA    : ${WII_SDA}")
message("Wii controller SCL    : ${WII_SCL}")
message("Wii controller I2C    : ${WIIPAD_I2C}")
message("LED pin               : ${LED_GPIO_PIN}")
message("UART output enabled   : ${UART_ENABLED}")
message("I2S Audio enabled     : ${USE_I2S_AUDIO}")
message("I2S audio data pin    : ${PICO_AUDIO_I2S_DATA_PIN}")
message("I2S audio clock pin   : ${PICO_AUDIO_I2S_CLOCK_PIN_BASE}")
message("I2S audio PIO         : ${PICO_AUDIO_I2S_PIO}")
message("I2S audio clock swap  : ${PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED}")
message("I2S audio reset pin   : ${PICO_AUDIO_I2S_RESET_PIN}")
message("PSRAM chip select pin : ${PSRAM_CS_PIN}")
if ( NOT GPIOHSTXD0 EQUAL 0 )
    message("Using HSTX for display output on RP2350, note RP2040 still uses PicoDVI")
    message("HSTX D0+ pin           : ${GPIOHSTXD0}")
    message("HSTX D1+ pin           : ${GPIOHSTXD1}")
    message("HSTX D2+ pin           : ${GPIOHSTXD2}")
    message("HSTX CK+ pin           : ${GPIOHSTXCK}")
    message("HSTX Inverted         : ${GPIOHSTXINVERTED}")
else()
    if ( FORCE_DVI EQUAL 0)
        message("HSTX is not enabled, using default display output.")
    else()
        message("DVI output is forced, HSTX ignored.")
    endif()
endif()
if ( DEFINED PICO_DEFAULT_UART_TX_PIN )
    message("Default UART TX pin   : ${PICO_DEFAULT_UART_TX_PIN}")
endif()
if ( DEFINED PICO_DEFAULT_UART_RX_PIN )
    message("Default UART RX pin   : ${PICO_DEFAULT_UART_RX_PIN}")
endif()


# ---------------------------------------------------------------------------
# RetroPicoES v2.0 / v2.1 (HW_CONFIG 15/16) shared build configuration
# ---------------------------------------------------------------------------
# Lives here rather than in a single project's CMakeLists so every consumer of
# pico_shared (pico-infonesPlus, retroJam, pico-snesPlus, pico-genesisPlus...)
# inherits it. Must be set before project()/pico_sdk_init(), which is where
# BoardConfigs.cmake is included from.
if (HW_CONFIG EQUAL 15 OR HW_CONFIG EQUAL 16)
    # These boards are RP2350B (48 GPIO). Building them as "pico2" (RP2350A,
    # 30 GPIO) makes tf_card.c compile the 30-pin SPI table, reject the SD pins
    # (MISO 40 / SCK 30 / MOSI 31), fall back to the PIO SPI driver and hang at
    # mount. pico2b.h sets PICO_RP2350A=0.
    if (NOT PICO_BOARD)
        set(PICO_BOARD pico2b CACHE STRING "Board type")
    endif()

    # Quad boot2: sets QE with the 1-byte 0x31 SR2 write (the 2-byte 0x01 does
    # not take on XTX XT25F128F-W) and reads 0xEB quad from the first
    # instruction, so QE=1 is consistent end to end. Verified on XTX and Puya.
    set(PICO_DEFAULT_BOOT_STAGE2_FILE
        "${CMAKE_CURRENT_LIST_DIR}/boot2_xtx_quad.S"
        CACHE STRING "Boot stage 2 source" FORCE)

    # Boot2 flash SCK divisor. 6 => 42 MHz at 252 MHz clk_sys, sized for the XTX
    # (~96 MHz Quad-I/O ceiling). Puya PY25Q129HA / Winbond W25Q128 are rated
    # 133 MHz and run 2 (126 MHz). Override with -DFLASH_CLKDIV=<n>.
    if (NOT DEFINED FLASH_CLKDIV)
        set(FLASH_CLKDIV 6)
    endif()
    add_definitions( -DPICO_FLASH_SPI_CLKDIV=${FLASH_CLKDIV} )

    # APS6404 8 MB PSRAM on QMI M1 (CS1 = GPIO47) sharing the QSPI bus with
    # flash, driven through the SDK 2.2.x hardware_psram API. The divisors below
    # express the intended frequency AT 252 MHz; FrensHelpers::initPsram()
    # rescales them to the live clk_sys so a runtime overclock cannot push the
    # parts past their 133 MHz ratings.
    if (NOT DEFINED XTX_PSRAM_DIVISOR)
        set(XTX_PSRAM_DIVISOR 4)
    endif()
    if (NOT DEFINED XTX_PSRAM_RXDELAY)
        set(XTX_PSRAM_RXDELAY 4)
    endif()
    if (NOT DEFINED XTX_PSRAM_MINDESEL)
        set(XTX_PSRAM_MINDESEL 3)
    endif()
    if (NOT DEFINED FLASH_6B_CLKDIV)
        set(FLASH_6B_CLKDIV 6)
    endif()
    add_definitions( -DXTX_PSRAM_DIVISOR=${XTX_PSRAM_DIVISOR} )
    add_definitions( -DXTX_PSRAM_RXDELAY=${XTX_PSRAM_RXDELAY} )
    add_definitions( -DXTX_PSRAM_MINDESEL=${XTX_PSRAM_MINDESEL} )
    add_definitions( -DXTX_FLASH_6B_CLKDIV=${FLASH_6B_CLKDIV} )
    add_definitions( -DXTX_FLASH_6B_MINDESEL=8 )
    add_definitions( -DPICO_PSRAM_CS_PIN=47 )
    add_definitions( -DPICO_PSRAM_SIZE_BYTES=0x800000 )
    add_definitions( -DHAVE_HARDWARE_PSRAM=1 )
endif()
