#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_random.h"

#define OFF 0
#define V  1
#define R  2

#define B1 16
#define B2 14

#define N 6

static const int filas[N] = {13, 12, 2, 4, 15, 0};
static const int colr[N] = {25, 27, 33, 5, 19, 22};
static const int colv[N] = {26, 17, 32, 18, 21, 23};

uint8_t m[N][N];

int x = 2;
bool dead = false;
bool start = false;

int vel = 70;
int t = 0;

static volatile int rf = 0;

void IRAM_ATTR isr(void *arg) {
    int ant = (rf + N - 1) % N;
    gpio_set_level(filas[ant], 0);

    for (int c = 0; c < N; c++) {
        gpio_set_level(colr[c], 0);
        gpio_set_level(colv[c], 0);
    }

    int r = rf;

    for (int c = 0; c < N; c++) {
        if (m[r][c] == V) gpio_set_level(colv[c], 1);
        if (m[r][c] == R) gpio_set_level(colr[c], 1);
    }

    gpio_set_level(filas[r], 1);
    rf = (rf + 1) % N;
}

void init() {
    for (int i = 0; i < N; i++) {
        gpio_reset_pin(filas[i]);
        gpio_set_direction(filas[i], GPIO_MODE_OUTPUT);
        gpio_set_level(filas[i], 0);
    }

    for (int i = 0; i < N; i++) {
        gpio_reset_pin(colr[i]);
        gpio_set_direction(colr[i], GPIO_MODE_OUTPUT);
        gpio_set_level(colr[i], 0);

        gpio_reset_pin(colv[i]);
        gpio_set_direction(colv[i], GPIO_MODE_OUTPUT);
        gpio_set_level(colv[i], 0);
    }

    gpio_set_direction(B1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(B1, GPIO_PULLUP_ONLY);

    gpio_set_direction(B2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(B2, GPIO_PULLUP_ONLY);
}

void clr() {
    memset(m, 0, sizeof(m));
}

static bool b1a = false;
static bool b2a = false;

void draw() {
    m[5][x] = R;
}

void move(bool l, bool r) {
    if (l && !b1a && x > 0) x--;
    if (r && !b2a && x < 5) x++;

    b1a = l;
    b2a = r;
}

void spawn() {
    int c = esp_random() % 6;
    m[0][c] = V;

    if (esp_random() % 2 == 0) {
        int c2;
        do { c2 = esp_random() % 6; } while (c2 == c);
        m[0][c2] = V;
    }
}

void fall() {
    for (int r = 5; r > 0; r--) {
        for (int c = 0; c < 6; c++) {
            m[r][c] = m[r - 1][c];
        }
    }

    for (int c = 0; c < 6; c++)
        m[0][c] = OFF;
}

bool l() { return gpio_get_level(B1) == 0; }
bool r() { return gpio_get_level(B2) == 0; }

bool hit() {
    return m[5][x] == V;
}

void face1() {
    clr();
    m[1][1] = R;
    m[1][4] = R;
    m[3][1] = R;
    m[3][4] = R;
    m[4][2] = R;
    m[4][3] = R;
}

void face2() {
    clr();
    m[0][0] = R; m[0][5] = R;
    m[1][1] = R; m[1][4] = R;
    m[2][2] = R; m[2][3] = R;
    m[3][2] = R; m[3][3] = R;
    m[4][1] = R; m[4][4] = R;
    m[5][0] = R; m[5][5] = R;
}

void reset() {
    x = 2;
    dead = false;
    start = false;
    vel = 45;
    t = 0;
    clr();
}

void loop(void *arg) {

    while (1) {

        if (!start) {
            face1();
            vTaskDelay(pdMS_TO_TICKS(2000));

            clr();
            draw();
            vTaskDelay(pdMS_TO_TICKS(1000));

            start = true;
        }

        else if (!dead) {

            bool iz = l();
            bool de = r();

            t++;

            if (t >= vel) {
                t = 0;

                fall();
                spawn();

                if (vel > 8) vel--;
            }

            move(iz, de);

            if (hit()) {
                dead = true;
            }

            for (int c = 0; c < 6; c++)
                if (m[5][c] == R)
                    m[5][c] = OFF;

            draw();
        }

        else {
            face2();
            vTaskDelay(pdMS_TO_TICKS(3000));
            reset();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main() {

    init();
    clr();

    esp_timer_handle_t tim;
    esp_timer_create_args_t a = {
        .callback = isr,
        .name = "x"
    };

    esp_timer_create(&a, &tim);
    esp_timer_start_periodic(tim, 2000);

    xTaskCreate(loop, "l", 4096, NULL, 5, NULL);
}