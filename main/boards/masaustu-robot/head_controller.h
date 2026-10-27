#ifndef __HEAD_CONTROLLER_H__
#define __HEAD_CONTROLLER_H__

// Pan/tilt kafa kontrolü: iki SG90 servo, yumuşak hareket, duygu hareketleri ve MCP araçları.
// Hareketler ayrı bir görevde (task) oynar; ses ve ekranı bekletmez.

#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdlib>
#include <string>

#include "config.h"
#include "mcp_server.h"

class HeadController {
public:
    enum Gesture { kNone, kHappy, kSad, kSurprised, kThinking, kAngry, kLove, kNod, kShake, kLookAround, kCenter, kLook };

    static HeadController& GetInstance() {
        static HeadController instance;
        return instance;
    }

    // Duygu adından baş hareketi (xiaozhi duyguları: happy, sad, angry, surprised, thinking, loving ...)
    void OnEmotion(const char* emotion) {
        if (emotion == nullptr) return;
        std::string e(emotion);
        int64_t now = esp_timer_get_time() / 1000;
        if (e == last_emotion_ && now - last_emotion_ms_ < 4000) return;   // aynı duygu art arda gelirse tekrarlama
        last_emotion_ = e;
        last_emotion_ms_ = now;
        Gesture g = kNone;
        if (e == "happy" || e == "laughing" || e == "funny" || e == "winking" || e == "cool" || e == "confident") g = kHappy;
        else if (e == "sad" || e == "crying") g = kSad;
        else if (e == "surprised" || e == "shocked") g = kSurprised;
        else if (e == "thinking" || e == "confused") g = kThinking;
        else if (e == "angry") g = kAngry;
        else if (e == "loving" || e == "kissy" || e == "embarrassed" || e == "relaxed") g = kLove;
        if (g != kNone) Enqueue(g);
    }

    void Enqueue(Gesture g, int pan = 0, int tilt = 0) {
        Cmd c{g, pan, tilt};
        xQueueOverwrite(queue_, &c);   // en yeni komut geçerli
    }

    int pan() const { return pan_ - PAN_CENTER_DEG; }
    int tilt() const { return tilt_ - TILT_CENTER_DEG; }

private:
    struct Cmd { Gesture g; int pan; int tilt; };
    QueueHandle_t queue_;
    int pan_ = PAN_CENTER_DEG, tilt_ = TILT_CENTER_DEG;
    bool attached_ = false;
    int64_t last_move_ms_ = 0;
    std::string last_emotion_;
    int64_t last_emotion_ms_ = 0;

    static constexpr ledc_channel_t kPanCh = LEDC_CHANNEL_6;
    static constexpr ledc_channel_t kTiltCh = LEDC_CHANNEL_7;
    static constexpr ledc_timer_t kTimer = LEDC_TIMER_3;

    HeadController() {
        ledc_timer_config_t t = {};
        t.speed_mode = LEDC_LOW_SPEED_MODE;
        t.duty_resolution = LEDC_TIMER_14_BIT;
        t.timer_num = kTimer;
        t.freq_hz = 50;
        t.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&t));
        ConfigChannel(PAN_SERVO_GPIO, kPanCh);
        ConfigChannel(TILT_SERVO_GPIO, kTiltCh);

        queue_ = xQueueCreate(1, sizeof(Cmd));
        xTaskCreate(TaskEntry, "head", 3072, this, 3, nullptr);
        Enqueue(kCenter);
        RegisterTools();
    }

    static void ConfigChannel(gpio_num_t pin, ledc_channel_t ch) {
        ledc_channel_config_t c = {};
        c.gpio_num = pin;
        c.speed_mode = LEDC_LOW_SPEED_MODE;
        c.channel = ch;
        c.intr_type = LEDC_INTR_DISABLE;
        c.timer_sel = kTimer;
        c.duty = 0;           // başta sinyal yok
        c.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&c));
    }

    static uint32_t DutyFor(int deg) {
        deg = std::clamp(deg, 0, 180);
        uint32_t us = SERVO_MIN_PULSE_US + (uint32_t)(SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * deg / 180;
        return us * 16384 / 20000;   // 14 bit, 20 ms periyot
    }

    void Write(ledc_channel_t ch, int deg) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, DutyFor(deg));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
    }

    // Hareket bitince sinyali kes: servo titremez, akım çekmez (ses parazitini de azaltır)
    void Relax() {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, kPanCh, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, kPanCh);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, kTiltCh, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, kTiltCh);
        attached_ = false;
    }

    // Göreli açıyla (merkeze göre) yumuşak git. step_ms: her 1° için bekleme
    void Look(int pan_rel, int tilt_rel, int step_ms) {
        int pt = PAN_CENTER_DEG + PAN_DIR * std::clamp(pan_rel, -PAN_RANGE_DEG, PAN_RANGE_DEG);
        int tt = TILT_CENTER_DEG + TILT_DIR * std::clamp(tilt_rel, -TILT_RANGE_DEG, TILT_RANGE_DEG);
        if (!attached_) { Write(kPanCh, pan_); Write(kTiltCh, tilt_); attached_ = true; }
        while (pan_ != pt || tilt_ != tt) {
            if (pan_ != pt) { pan_ += (pt > pan_) ? 1 : -1; Write(kPanCh, pan_); }
            if (tilt_ != tt) { tilt_ += (tt > tilt_) ? 1 : -1; Write(kTiltCh, tilt_); }
            vTaskDelay(pdMS_TO_TICKS(step_ms));
        }
        last_move_ms_ = esp_timer_get_time() / 1000;
    }
    int CurPan() const { return PAN_DIR * (pan_ - PAN_CENTER_DEG); }
    int CurTilt() const { return TILT_DIR * (tilt_ - TILT_CENTER_DEG); }

    void Play(const Cmd& c) {
        switch (c.g) {
            case kHappy:     for (int k = 0; k < 2; k++) { Look(-12, 10, 4); Look(12, -4, 4); } Look(0, 0, 5); break;
            case kSad:       Look(-8, -16, 18); vTaskDelay(pdMS_TO_TICKS(900)); Look(0, 0, 18); break;
            case kSurprised: Look(-20, 15, 3); vTaskDelay(pdMS_TO_TICKS(500)); Look(0, 0, 7); break;
            case kThinking:  Look(30, 12, 12); vTaskDelay(pdMS_TO_TICKS(1000)); Look(0, 0, 12); break;
            case kAngry:     Look(0, -8, 5); for (int k = 0; k < 3; k++) { Look(-12, -8, 3); Look(12, -8, 3); } Look(0, 0, 4); break;
            case kLove:      for (int k = 0; k < 2; k++) { Look(-10, 8, 10); Look(10, 8, 10); } Look(0, 0, 10); break;
            case kNod:       for (int k = 0; k < 2; k++) { Look(CurPan(), -12, 4); Look(CurPan(), 8, 4); } Look(CurPan(), 0, 5); break;
            case kShake:     for (int k = 0; k < 3; k++) { Look(-18, CurTilt(), 3); Look(18, CurTilt(), 3); } Look(0, CurTilt(), 4); break;
            case kLookAround:Look(-40, 6, 10); vTaskDelay(pdMS_TO_TICKS(500)); Look(40, 6, 10); vTaskDelay(pdMS_TO_TICKS(500)); Look(0, 0, 10); break;
            case kCenter:    Look(0, 0, 8); break;
            case kLook:      Look(c.pan, c.tilt, 8); break;
            default: break;
        }
    }

    static void TaskEntry(void* arg) {
        auto* self = static_cast<HeadController*>(arg);
        Cmd c;
        while (true) {
            if (xQueueReceive(self->queue_, &c, pdMS_TO_TICKS(200)) == pdTRUE) {
                self->Play(c);
            } else if (self->attached_ && esp_timer_get_time() / 1000 - self->last_move_ms_ > 1000) {
                self->Relax();
            }
        }
    }

    void RegisterTools() {
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.head.look",
            "Robotun başını çevirir. pan: sağa(+)/sola(-) derece (-60..60), tilt: yukarı(+)/aşağı(-) derece (-18..18). "
            "Kullanıcı 'bana bak', 'sağa bak', 'yukarı bak' dediğinde kullan.",
            PropertyList({Property("pan", kPropertyTypeInteger, 0, -PAN_RANGE_DEG, PAN_RANGE_DEG),
                          Property("tilt", kPropertyTypeInteger, 0, -TILT_RANGE_DEG, TILT_RANGE_DEG)}),
            [this](const PropertyList& p) -> ReturnValue {
                Enqueue(kLook, p["pan"].value<int>(), p["tilt"].value<int>());
                return true;
            });
        mcp.AddTool("self.head.nod", "Başını aşağı yukarı sallar (evet, onaylama).", PropertyList(),
            [this](const PropertyList&) -> ReturnValue { Enqueue(kNod); return true; });
        mcp.AddTool("self.head.shake", "Başını iki yana sallar (hayır, reddetme).", PropertyList(),
            [this](const PropertyList&) -> ReturnValue { Enqueue(kShake); return true; });
        mcp.AddTool("self.head.look_around", "Etrafına bakınır (sola, sağa, sonra öne).", PropertyList(),
            [this](const PropertyList&) -> ReturnValue { Enqueue(kLookAround); return true; });
        mcp.AddTool("self.head.center", "Başını ortaya, tam öne döndürür.", PropertyList(),
            [this](const PropertyList&) -> ReturnValue { Enqueue(kCenter); return true; });
        mcp.AddTool("self.head.get_position", "Başın şu anki açısını verir (pan, tilt).", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return "{\"pan\": " + std::to_string(CurPan()) + ", \"tilt\": " + std::to_string(CurTilt()) + "}";
            });
    }
};

#endif // __HEAD_CONTROLLER_H__
