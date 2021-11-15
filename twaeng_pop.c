/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "ws2812.pio.h"

#define COLOR_BRG(R, G, B) (((G) << 16) | ((R) << 8) | (B))
#define WHITE COLOR_BRG(255,130,80)

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

const int PIN_TX = 28;
#define num_leds 25

const int p_1_leds[] = {1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
const int o_leds[] =   {0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
const int o_p_2_leds[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // only want that middle one
const int p_2_leds[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
const int excl_leds[]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0};

const uint32_t p_1_color = COLOR_BRG(240, 5, 2);
const uint32_t p_1_o_color = COLOR_BRG(20, 0, 40);
const uint32_t o_color = COLOR_BRG(12, 35, 120);
const uint32_t o_p2_color = COLOR_BRG(28, 70, 6);
const uint32_t p_2_color = COLOR_BRG(200, 40, 0); ///* 23, 167, 254 */));// 20,135,206
const uint32_t p_2_excl_color = COLOR_BRG(150, 8, 1);
const uint32_t excl_color = COLOR_BRG(150, 12, 8);

void show_letter(const int *letter, uint32_t color, uint t) {
    for (int i = 0; i < num_leds; ++i) {
        if (letter[i])
            put_pixel(color);
        else
            put_pixel(0);
    }
}
void show_all(uint t) {
    for (int i = 0; i < num_leds; ++i) {
        if (p_1_leds[i] && o_leds[i])
            put_pixel(p_1_o_color);
        else if (p_1_leds[i])
            put_pixel(p_1_color);
        else if (o_p_2_leds[i])
            put_pixel(o_p2_color);
        else if (o_leds[i])
            put_pixel(o_color);
        else if (p_2_leds[i] && excl_leds[i])
            put_pixel(p_2_excl_color);
        else if (p_2_leds[i])
            put_pixel(p_2_color);
        else if (excl_leds[i])
            put_pixel(excl_color);
        else
            put_pixel(0);
    }
}

// topology of led sign, defines which nodes are physically adjacent
#define NO_NEIGHBORS 255
// contains an index into adjacency_nodes per node (or 255 for no neighbors besides +/- 1)
uint8_t adjacency_refs[] = {
    NO_NEIGHBORS, // 0
    0, // 1
    4, // 2
    NO_NEIGHBORS,            // 3
    8, // 4
    11, // 5
    15, // 6
    19, // 7
    23,            // 8
    27,            // 9
    31,           // 10
    34,           // 11
    38,           // 12
    42, // 13
    46,           // 14
    52,           // 15
    57,           // 16
    59, // 17
    63, // 18
    66,           // 19
    71,           // 20
    76, // 21
    79,           // 22
    84,           // 23
    NO_NEIGHBORS, // 24
};
// contains adjacent node entries
// i + 0 = number of entries
// i + 1..n = adjacent nodes
uint8_t adjacency_nodes[] =
    {
        3,0,2,5,  // 1 - 0
        3,1,3,5,  // 2 - 4
        2,3,7,    // 4 - 8
        3,1,2,6,  // 5 - 11
        3,5,7,9,  // 6 - 15
        3,4,6,8,  // 7 - 19
        3,7,9,10, // 8 - 23
        3,6,8,11, // 9 - 27
        2,8,12,   //10 - 31
        3,9,12,15,// 11 - 34
        3,10,11,13, // 12 - 38
        3,12,14,18, // 13 - 42
        5,11,12,13,15,17, // 14 - 46
        4,11,14,16,17, //15 - 52
        1,15, // 16 - 57
        3,14,15,20, // 17 - 59
        2,13,19, // 18 - 63
        4,18,20,21,22, // 19 - 66
        4,17,19,22,23, // 20 - 71
        2,19,22, // 21 - 76
        4,19,20,21,23, // 22 - 79
        3,20,22,24, // 23 - 84
    };

uint8_t random_next_pos(uint8_t at, uint8_t exclude, uint8_t exclude2) {
    uint8_t neighbors[4];
    int num = 0;
    int entry = adjacency_refs[at];
    if (entry == NO_NEIGHBORS) {
        if (at > 0) neighbors[num++] = at - 1;
        if (at < (num_leds - 1)) neighbors[num++] = at + 1;
    } else {
        int ns = adjacency_nodes[entry];
        for (int i = entry + 1; i < entry + ns + 1; i++) {
            uint8_t target = adjacency_nodes[i];
            //if (target != exclude && target != exclude2)
            neighbors[num++] = target;
        }
    }
    if (num == 1) return neighbors[0];

    for(int i = 0; i < 10; i++) {
        int idx = rand() % num;
        uint8_t cand = neighbors[idx];
        // check for exclusion criteria
        if (cand != exclude && cand != exclude2 && cand >= 0 && cand <= num_leds)
            return cand;
    }
    return neighbors[0]; // emergency exit
}

const int32_t worm0_color = COLOR_BRG(28, 70, 6);
const int32_t worm_tail_color = COLOR_BRG(100, 40, 0);

int speed = 60;

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

// macro doesn't work for some reason?
// #define INTERP(TFROM, TTO, YFROM, YTO, T)  ((YFROM) + ((YTO) - (YFROM)) * (min((T), (TTO)) - (TFROM)) / ((TTO) - (TFROM)))
int interp(int TFROM, int TTO, int YFROM, int YTO, int T) {
    return ((YFROM) + ((YTO) - (YFROM)) * (min((T), (TTO)) - (TFROM)) / ((TTO) - (TFROM)));
}

uint32_t worm_color_by_speed(int speed, uint32_t t, int idx) {
    if (speed > 15)
        if (idx) return worm_tail_color;
        else return worm0_color;
    else
        if (speed > 7)
            if (idx) return COLOR_BRG(150, 60, 10);
            else return COLOR_BRG(70, 140, 20);
        else if (t < 3500)
            if (idx) return COLOR_BRG(255, 130, 80);
            else return COLOR_BRG(40, 20, 200);
        else if (t < 4000)
            if (idx)
                return COLOR_BRG(interp(3500,4000, 40, 255, t), interp(3500,4000,20,0,t), interp(3500,4000,200,255,t));
            else
                return COLOR_BRG(interp(3500,4000, 255, 0, t), interp(3500, 4000, 130, 255, t), interp(3500, 4000, 80, 0, t));
        else
            if (idx)
                return COLOR_BRG(255, 0, 255);
            else
                return COLOR_BRG(0, 255, 0);
}

uint8_t worm0 = 6;
uint8_t worm1 = num_leds;
uint8_t worm2 = num_leds;
uint8_t worm3 = num_leds;

uint32_t frame_buffer[num_leds];

uint32_t background[num_leds];

const int default_pwm_led_fade = 80;
const int highlight_pwm_led_fade = 150;
int pwm_led_fade = default_pwm_led_fade;

void paint_letters_to_buffer(uint32_t *buffer) {
    for (int i = 0; i < num_leds; ++i) {
        if (p_1_leds[i] && o_leds[i])
            buffer[i] = (p_1_o_color);
        else if (p_1_leds[i])
            buffer[i] = (p_1_color);
        else if (o_p_2_leds[i])
            buffer[i] = (o_p2_color);
        else if (o_leds[i])
            buffer[i] = (o_color);
        else if (p_2_leds[i] && excl_leds[i])
            buffer[i] = (p_2_excl_color);
        else if (p_2_leds[i])
            buffer[i] = (p_2_color);
        else if (excl_leds[i])
            buffer[i] = (excl_color);
        else
            buffer[i] = (0);
    }
}
void init_background() {
    paint_letters_to_buffer(background);
    for (int i = 0; i < num_leds * 4; ++i) {
        // background is much lower intensity
        ((uint8_t*)background)[i] = max(1, (((uint16_t)(((uint8_t*)background)[i])))>>3);
    }
}
uint32_t paint_buffer[num_leds];
void paint_mixed() {
    for (int i = 0; i < num_leds * 4; ++i) {
        ((uint8_t*)paint_buffer)[i] =
            ((((uint16_t)(((uint8_t*)frame_buffer)[i]))) + (((uint16_t)(((uint8_t*)background)[i]))))>>1;
    }
    for (int i = 0; i < num_leds; ++i) put_pixel(paint_buffer[i]);
}

void paint_frame_buffer() {
    for (int i = 0; i < num_leds; ++i) put_pixel(frame_buffer[i]);
}
void decay_frame_buffer() {
    for (int i = 0; i < num_leds * 4; ++i) {
        ((uint8_t*)frame_buffer)[i] = (((uint16_t)(((uint8_t*)frame_buffer)[i])) * 14)>>4; // 14/16
    }
    pwm_led_fade = (pwm_led_fade * 14) >> 4;
}
void set_fbpixel(uint8_t pixel, uint32_t color) {
    if (pixel < num_leds) frame_buffer[pixel] = color;
}

void worm_moves(uint32_t t) {
    if (speed > 4 && t % 60 == 0) speed-=1;

    if (t % 6 == 0)
        decay_frame_buffer();

    if (t % speed == 0) {
        uint8_t newPos = random_next_pos(worm0, worm1, worm2);
        worm3 = worm2;
        worm2 = worm1;
        worm1 = worm0;
        worm0 = newPos;
    }

    set_fbpixel(worm1, worm_color_by_speed(speed, t, 1));
    set_fbpixel(worm2, worm_color_by_speed(speed, t, 2));
    set_fbpixel(worm3, worm_color_by_speed(speed, t, 3));

    set_fbpixel(worm0, worm_color_by_speed(speed, t, 0));

    paint_mixed();
}

void worm() {
    speed = 60;
    for (uint32_t i = 0; i < 4800; ++i) {
        worm_moves(i);
        sleep_ms(5);
    }
}

struct rocket_t {
    uint32_t offset_ms;
    uint8_t pos_idx;
    uint8_t state;
    uint32_t color;
    uint32_t letter_color;
    const int *letter_leds;
    uint8_t path_len;
    uint8_t path[6];
    bool fizzle;
    uint8_t fizzle_leds[4];
};

void rocket_step(struct rocket_t *r, int t) {
    if (t < r->offset_ms) return;
    if (r->pos_idx < r->path_len - 1) {
        r->pos_idx++;
        r->color = COLOR_BRG(25 << ((t - r->offset_ms) / 300), 5 << ((t - r->offset_ms) / 300), 0);
    } else if (r->fizzle) {
        r->state = 1;
        for (int i = 0; i < 2;) {
            int cand = rand() % num_leds;
            if (r->letter_leds[cand])
                r->fizzle_leds[i++] = cand;
        }
    } else if (r->state == 0) {
        r->state = 1;
    }
}
void rocket_paint(struct rocket_t *r, int t) {
    if (r->state == 0) 
        set_fbpixel(r->path[r->pos_idx], r->color);
    else if (r->fizzle) {
        pwm_led_fade = highlight_pwm_led_fade;
        for (int i = 0; i < 2; i++) {
            set_fbpixel(r->fizzle_leds[i], COLOR_BRG(rand()%255,rand()%130,rand()%80));//COLOR_BRG(255>>intensity,130>>intensity,80>>intensity));
        }
    } else if (r->state >= 1 && r->state < 200) {
        pwm_led_fade = highlight_pwm_led_fade;
        for (int i = 0; i < num_leds; i++)
            if (r->letter_leds[i]) set_fbpixel(i, r->letter_color);
        r->state += 1;
    }
}
struct rocket_t r1 = {
    .offset_ms = 0,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = p_1_color,
    .letter_leds = &p_1_leds,
    .path_len = 5,
    .path = {0,1,2,3,4},
    .fizzle = true
};
struct rocket_t r2 = {
    .offset_ms = 280,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = o_color,
    .letter_leds = &o_leds,
    .path_len = 5,
    .path = {16,15,14,13,18},
    .fizzle = true
};
struct rocket_t r3 = {
    .offset_ms = 560,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = p_2_color,
    .letter_leds = &p_2_leds,
    .path_len = 4,
    .path = {24,23,22,21},
    .fizzle = true
};
struct rocket_t r4 = {
    .offset_ms = 700,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = excl_color,
    .letter_leds = &excl_leds,
    .path_len = 4,
    .path = {24,23,22,21},
    .fizzle = true
};
struct rocket_t *rockets[] = {&r1,&r2,&r3,&r4};

struct rocket_t r5 = {
    .offset_ms = 0,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = WHITE,
    .letter_leds = &p_1_leds,
    .path_len = 5,
    .path = {0,1,2,3,4},
    .fizzle = true
};
struct rocket_t r6 = {
    .offset_ms = 0,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = WHITE,
    .letter_leds = &o_leds,
    .path_len = 5,
    .path = {16,15,14,13,18},
    .fizzle = true
};
struct rocket_t r7 = {
    .offset_ms = 210,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = WHITE,
    .letter_leds = &p_2_leds,
    .path_len = 4,
    .path = {24,23,22,21},
    .fizzle = true
};
struct rocket_t r8 = {
    .offset_ms = 210,
    .pos_idx = 0,
    .state = 0,
    .color = COLOR_BRG(25, 5 , 0),
    .letter_color = excl_color,
    .letter_leds = &excl_leds,
    .path_len = 4,
    .path = {24,23,22,21},
    .fizzle = true
};
struct rocket_t *white_rockets[] = {&r5,&r6,&r7,&r8};

void fireworks_run(bool fizzle, struct rocket_t **rockets) {
    // reset rockets
    for (int i = 0; i < 4; i++) {
        rockets[i]->state = 0;
        rockets[i]->pos_idx = 0;
        rockets[i]->color = COLOR_BRG(25, 5 , 0);
        rockets[i]->fizzle = fizzle;
    }

    for (int t = 1; t <= 2200; ++t) {
        if (t % 20 == 0) decay_frame_buffer();
        
        if (t % 140 == 0) {
            for (int i = 0; i < 4; i++)
                rocket_step(rockets[i], t);
        }
        for (int i = 0; i < 4; i++)
            rocket_paint(rockets[i], t);

        paint_frame_buffer();

        sleep_ms(1);
    }
}
void fireworks() {
    fireworks_run(false, rockets);
    fireworks_run(true, rockets);
    fireworks_run(false, rockets);
    fireworks_run(false, white_rockets);
    fireworks_run(false, rockets);
}

void put_faded_pixel(uint32_t color, int fade) {
    for (int i = 0; i < 4; ++i)
        ((uint8_t*)&color)[i] = (((uint16_t)(((uint8_t*)&color)[i])) * fade)>>6;
    put_pixel(color);
}

const int flicker = 12;

void fade_up_letters() {
    int flicker_eff = 0;
    for (int t = 0; t < 64; ++t) {
        flicker_eff = t >> 3;
        for (int i = 0; i < num_leds; ++i) {
            if (p_1_leds[i] && o_leds[i])
                put_faded_pixel(p_1_o_color, t - rand() % flicker_eff);
            else if (p_1_leds[i])
                put_faded_pixel(p_1_color, t - rand() % flicker_eff);
            else if (o_p_2_leds[i])
                put_faded_pixel(o_p2_color, t - rand() % flicker_eff);
            else if (o_leds[i])
                put_faded_pixel(o_color, t - rand() % flicker_eff);
            else if (p_2_leds[i] && excl_leds[i])
                put_faded_pixel(p_2_excl_color, t - rand() % flicker_eff);
            else if (p_2_leds[i])
                put_faded_pixel(p_2_color, t - rand() % flicker_eff);
            else if (excl_leds[i])
                put_faded_pixel(excl_color, t - rand() % flicker_eff);
            else put_pixel(0);
        }
        pwm_led_fade = (default_pwm_led_fade * t) >> 6;

        sleep_ms(30);
    }
}

void glowing_letters() {

    for (int t = 0; t < 200; ++t) {
        for (int i = 0; i < num_leds; ++i) {
            if (p_1_leds[i] && o_leds[i])
                put_faded_pixel(p_1_o_color, 64 - rand() % flicker);
            else if (p_1_leds[i])
                put_faded_pixel(p_1_color, 64 - rand() % flicker);
            else if (o_p_2_leds[i])
                put_faded_pixel(o_p2_color, 64 - rand() % flicker);
            else if (o_leds[i])
                put_faded_pixel(o_color, 64 - rand() % flicker);
            else if (p_2_leds[i] && excl_leds[i])
                put_faded_pixel(p_2_excl_color, 64 - rand() % flicker);
            else if (p_2_leds[i])
                put_faded_pixel(p_2_color, 64 - rand() % flicker);
            else if (excl_leds[i])
                put_faded_pixel(excl_color, 64 - rand() % flicker);
            else put_pixel(0);
        }

        sleep_ms(100);
    }
}

#define LED_PWM_PIN 22

void on_pwm_wrap() {
    // Clear the interrupt flag that brought us here
    pwm_clear_irq(pwm_gpio_to_slice_num(LED_PWM_PIN));

    // Square the fade value to make the LED's brightness appear more linear
    // Note this range matches with the wrap value
    pwm_set_gpio_level(LED_PWM_PIN, pwm_led_fade * pwm_led_fade);
}

void init_pwm() {
        // Tell the LED pin that the PWM is in charge of its value.
    gpio_set_function(LED_PWM_PIN, GPIO_FUNC_PWM);
    // Figure out which slice we just connected to the LED pin
    uint slice_num = pwm_gpio_to_slice_num(LED_PWM_PIN);

    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Get some sensible defaults for the slice configuration. By default, the
    // counter is allowed to wrap over its maximum range (0 to 2**16-1)
    pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 4.f);
    // Load the configuration into our PWM slice, and set it running.
    pwm_init(slice_num, &config, true);
}

int main() {
    //set_sys_clock_48();
    stdio_init_all();
    puts("Twaeng Pop Sign");

    init_pwm();

    // todo get free sm
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, PIN_TX, 800000, false);

    while(1) {
        fade_up_letters();
        glowing_letters(0);
        paint_letters_to_buffer(frame_buffer);
        fireworks();
        fade_up_letters();
        glowing_letters(0);

        paint_letters_to_buffer(frame_buffer); // for a smooth fading to background
        init_background();
        worm();
        for (int t = 0; t < 800; t++) {
            if (t % 20 == 0) decay_frame_buffer();
            paint_frame_buffer();
            sleep_ms(1);
        }
    }
}