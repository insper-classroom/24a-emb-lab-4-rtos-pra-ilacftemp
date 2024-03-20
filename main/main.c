/*
 * LED blink with FreeRTOS
 */

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include "gfx.h"
#include "ssd1306.h"
#include <stdio.h>

const int ECHO_PIN = 12;
const int TRIG_PIN = 13;

QueueHandle_t time_queue;
QueueHandle_t dist_queue;

SemaphoreHandle_t xSemaphore;

void echo_pin_callback(uint gpio, uint32_t events){
    static uint32_t time_start;
    static uint32_t time_end, time_diff;
    if(events == GPIO_IRQ_EDGE_RISE){
        time_start = time_us_32();
    } else if (events == GPIO_IRQ_EDGE_FALL){
        time_end = time_us_32();
        time_diff = time_end - time_start;
        xQueueSendFromISR(time_queue, &time_diff, NULL);
    }
}

void trigger_task(void *p){
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    while (true){
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        xSemaphoreGive(xSemaphore);
    }  
}

void echo_task(void *p){
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    uint32_t time_diff, dist;

    while(true){
        if(xQueueReceive(time_queue, &time_diff, portMAX_DELAY) == pdTRUE){
            dist = time_diff / 58;
            xQueueSend(dist_queue, &dist, portMAX_DELAY);
        }
    }
}

void oled_task(void *p){
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    uint32_t dist;
    char dist_str[20];
    char progress_str[128];
    int progress;

    while(true){
        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE){
            if(xQueueReceive(dist_queue, &dist, pdMS_TO_TICKS(1000)) == pdTRUE){
                printf("%u\n", dist);
                float scale = 1.5;
                progress = (int)(dist * scale);
                // progress = abs((dist/128)*100);
                if(progress > 128){
                    progress = 128;
                }
                sprintf(dist_str, "%u cm", dist);
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                gfx_draw_string(&disp, 0, 10, 1, dist_str);
                gfx_draw_line(&disp, 0, 20, progress, 20);

                gfx_show(&disp);
            } else {
                printf("else");
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 10, 1, "Falhou");
                gfx_show(&disp);
            }
        }
    }
}

int main() {
    stdio_init_all();

    time_queue = xQueueCreate(100, sizeof(uint32_t));
    dist_queue = xQueueCreate(100, sizeof(uint32_t));

    xSemaphore = xSemaphoreCreateBinary();

    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_pin_callback);

    xTaskCreate(trigger_task, "trigger", 1024, NULL, 1, NULL);
    xTaskCreate(echo_task, "echo", 1024, NULL, 1, NULL);
    xTaskCreate(oled_task, "oled", 1024, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true){}
}
