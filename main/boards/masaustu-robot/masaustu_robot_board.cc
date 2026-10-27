// Masaüstü Robot — ESP32-S3 N16R8 + INMP441 + MAX98357A + SSD1306 128x64 + 2x SG90 (pan/tilt)
#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "head_controller.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "MasaustuRobot"

// OLED ekran + duyguya göre baş hareketi
class RobotOledDisplay : public OledDisplay {
public:
    using OledDisplay::OledDisplay;
    bool SetEmotion(const char* emotion) override {
        HeadController::GetInstance().OnEmotion(emotion);
        return OledDisplay::SetEmotion(emotion);
    }
};

// MAX98357A: ses çalınmıyorken SD pini LOW → amfi kapalı, servo/Wi-Fi paraziti hoparlöre gitmez
class RobotAudioCodec : public NoAudioCodecSimplex {
public:
    using NoAudioCodecSimplex::NoAudioCodecSimplex;
    void EnableOutput(bool enable) override {
        if (enable) {
            gpio_set_level(AMP_SD_GPIO, 1);
            NoAudioCodecSimplex::EnableOutput(true);
        } else {
            NoAudioCodecSimplex::EnableOutput(false);
            gpio_set_level(AMP_SD_GPIO, 0);
        }
    }
};

class MasaustuRobotBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    Button talk_button_;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;
        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "OLED başlatılamadı (SDA=41, SCL=42, adres 0x3C?)");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
        display_ = new RobotOledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        // BOOT: açılışta basılırsa Wi-Fi ayar modu, sonra sohbeti aç/kapa
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        // Kafadaki buton: basılı tut, konuş, bırak
        talk_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        talk_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }

public:
    MasaustuRobotBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        talk_button_(TALK_BUTTON_GPIO) {
        gpio_config_t amp_sd = {};
        amp_sd.pin_bit_mask = 1ULL << AMP_SD_GPIO;
        amp_sd.mode = GPIO_MODE_OUTPUT;
        ESP_ERROR_CHECK(gpio_config(&amp_sd));
        gpio_set_level(AMP_SD_GPIO, 0);           // açılışta amfi kapalı
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        HeadController::GetInstance();   // servoları ortala + MCP baş araçlarını kaydet
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static RobotAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(MasaustuRobotBoard);
