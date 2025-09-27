#include "app/setup.h"
#include <esp_log.h>
#include <SendTask.h>
#include <audio/LofiSynth.h>

struct LofiStreamContext {
    static const size_t BUFFER_SIZE = 3000;
    static const size_t QUEUE_LENGTH = 3;
    struct AudioBuffer {
        int16_t samples[BUFFER_SIZE];
        size_t sampleCount;
    };
    QueueHandle_t audioQueue;
    TaskHandle_t producerTask;
    TaskHandle_t consumerTask;
    volatile bool streaming;
    volatile bool stopRequested;
    
    LofiStreamContext() : audioQueue(nullptr), 
                         producerTask(nullptr), consumerTask(nullptr),
                         streaming(false), stopRequested(false) {
    }
    
    bool init() {
        audioQueue = xQueueCreate(QUEUE_LENGTH, sizeof(AudioBuffer));
        return (audioQueue != nullptr);
    }
    
    void cleanup() {
        if (audioQueue) { 
            vQueueDelete(audioQueue); 
            audioQueue = nullptr; 
        }
    }
};

static LofiStreamContext* streamContext = nullptr;
void stopLofiStreaming();
void lofiAudioProducerTask(void* parameters) {
    LofiStreamContext* ctx = (LofiStreamContext*)parameters;
    const char* taskName = "LofiProducer";
    
    ESP_LOGI(taskName, "Audio producer task started");
    LofiSynth::LofiEngine* lofiEngine = new LofiSynth::LofiEngine();
    if (!lofiEngine || !lofiEngine->init(16000)) {
        ESP_LOGE(taskName, "Failed to initialize LofiEngine");
        ctx->stopRequested = true;
        vTaskDelete(nullptr);
        return;
    }
    if (!lofiEngine->startMelody()) {
        ESP_LOGE(taskName, "Failed to start lofi melody");
        delete lofiEngine;
        ctx->stopRequested = true;
        vTaskDelete(nullptr);
        return;
    }
    
    ESP_LOGI(taskName, "LofiEngine initialized and melody started");
    
    while (!ctx->stopRequested) {
        long startTime = millis();
        static long lastSendLog = millis();
        long sendLog = 5000;
        LofiStreamContext::AudioBuffer audioBuffer;
        audioBuffer.sampleCount = ctx->BUFFER_SIZE;
        for (size_t i = 0; i < ctx->BUFFER_SIZE; i++) {
            float sample = lofiEngine->renderSample();
            sample *= 15000.0f;  // Fixed amplitude for clean audio
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32767.0f) sample = -32767.0f;
            audioBuffer.samples[i] = (int16_t)sample;
        }
        
        if (xQueueSend(ctx->audioQueue, &audioBuffer, pdMS_TO_TICKS(500)) != pdTRUE) {
            ESP_LOGW(taskName, "Queue full, dropping audio buffer");
        }
        if (millis() - lastSendLog > sendLog) {
            ESP_LOGI(taskName, "1 buffer need %dms time", millis() - startTime);
            lastSendLog = millis();
        }
        taskYIELD();
    }
    
    ESP_LOGI(taskName, "Producer task stopping");
    delete lofiEngine;
    vTaskDelete(nullptr);
}
void lofiAudioConsumerTask(void* parameters) {
    LofiStreamContext* ctx = (LofiStreamContext*)parameters;
    const char* taskName = "LofiConsumer";
    
    ESP_LOGI(taskName, "Audio consumer task started");
    
    while (!ctx->stopRequested) {
        long startTime = millis();
        static long lastSendLog = millis();
        long sendLog = 5000;
        LofiStreamContext::AudioBuffer audioBuffer;
        if (xQueueReceive(ctx->audioQueue, &audioBuffer, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (i2sSpeaker && i2sSpeaker->isActive()) {
                int samplesWritten = i2sSpeaker->writeSamples(audioBuffer.samples, audioBuffer.sampleCount, pdMS_TO_TICKS(50));
                if (samplesWritten <= 0) {
                    ESP_LOGW(taskName, "I2S write failed, continuing...");
                }
            }
        }
        if (millis() - lastSendLog > sendLog) {
            ESP_LOGI(taskName, "1 buffer need %dms time", millis() - startTime);
            lastSendLog = millis();
        }
        taskYIELD();
    }
    
    ESP_LOGI(taskName, "Consumer task stopping");
    vTaskDelete(nullptr);
}
bool startLofiStreaming() {
    if (streamContext && streamContext->streaming) {
        ESP_LOGW("LofiStream", "Already streaming, stopping first");
        stopLofiStreaming();
    }
    streamContext = new LofiStreamContext();
    if (!streamContext->init()) {
        ESP_LOGE("LofiStream", "Failed to initialize stream context");
        delete streamContext;
        streamContext = nullptr;
        return false;
    }
    
    streamContext->streaming = true;
    streamContext->stopRequested = false;
    vTaskDelay(50);
    BaseType_t result1 = xTaskCreatePinnedToCore(
        lofiAudioProducerTask,
        "LofiProducer",
        9*1024,
        streamContext,
        5,
        &streamContext->producerTask,
        0
    );
    
    vTaskDelay(pdMS_TO_TICKS(30));
    BaseType_t result2 = xTaskCreatePinnedToCore(
        lofiAudioConsumerTask,
        "LofiConsumer", 
        9 * 1024,
        streamContext,
        9,
        &streamContext->consumerTask,
        1
    );
    
    if (result1 != pdPASS || result2 != pdPASS) {
        ESP_LOGE("LofiStream", "Failed to create streaming tasks");
        stopLofiStreaming();
        return false;
    }
    
    return true;
}

void stopLofiStreaming() {
    if (!streamContext) return;
    
    ESP_LOGI("LofiStream", "Stopping dual-task streaming");
    streamContext->stopRequested = true;
    streamContext->streaming = false;
    if (streamContext->producerTask) {
        ESP_LOGI("LofiStream", "Waiting for producer task to finish");
        vTaskDelay(pdMS_TO_TICKS(200));  // Give time to finish
    }
    
    if (streamContext->consumerTask) {
        ESP_LOGI("LofiStream", "Waiting for consumer task to finish");
        vTaskDelay(pdMS_TO_TICKS(200));  // Give time to finish
    }
    streamContext->cleanup();
    delete streamContext;
    streamContext = nullptr;
    
    ESP_LOGI("LofiStream", "Dual-task streaming stopped");
}

bool isLofiStreaming() {
    return (streamContext && streamContext->streaming && !streamContext->stopRequested);
}