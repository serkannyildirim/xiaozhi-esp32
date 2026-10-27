#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// Masaüstü Robot (ESP32-S3 N16R8) — Serkan'ın pan/tilt kafalı robotu
// Pinler bread-compact-wifi ile aynı, servo ve tek konuş butonu eklendi.

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// INMP441 mikrofon
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
// MAX98357A amfi
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16
#define AMP_SD_GPIO             GPIO_NUM_21    // MAX98357A SD: sessizken amfi kapalı (servo cızırtısı duyulmaz)

// Butonlar
#define BOOT_BUTTON_GPIO        GPIO_NUM_0     // kart üzerindeki BOOT: Wi-Fi ayarı / sohbeti aç-kapa
#define TALK_BUTTON_GPIO        GPIO_NUM_47    // kafanın üstündeki konuş butonu (basılı tut-konuş)

// Kart üzerindeki RGB LED
#define BUILTIN_LED_GPIO        GPIO_NUM_48

// SSD1306 OLED 128x64 (I2C)
#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

// Servolar (SG90, 180°)
#define PAN_SERVO_GPIO          GPIO_NUM_17    // sağa-sola dönüş
#define TILT_SERVO_GPIO         GPIO_NUM_18    // yukarı-aşağı eğilme
#define SERVO_MIN_PULSE_US      500            // 0°
#define SERVO_MAX_PULSE_US      2500           // 180°
#define PAN_CENTER_DEG          90
#define TILT_CENTER_DEG         90
#define PAN_RANGE_DEG           60             // merkezden ±60° (kafa ±90'a kadar çarpmıyor, pay bırakıldı)
#define TILT_RANGE_DEG          18             // merkezden ±18° (kafa ±20°'de brakete yaklaşıyor)
#define PAN_DIR                 -1              // ters dönüyorsa -1 yap
#define TILT_DIR                -1              // yukarı yerine aşağı bakıyorsa -1 yap

#endif // _BOARD_CONFIG_H_
