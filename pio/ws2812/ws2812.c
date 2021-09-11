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
#include "ws2812.pio.h"

#define COLOR_BRG(R, G, B) (((G) << 16) | ((R) << 8) | (B))

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void pattern_snakes(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(0);
    }
}

void pattern_random(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand());
}

void pattern_sparkle(uint len, uint t) {
    if (t % 20)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand() % 32 ? 0 : COLOR_BRG(255,130,80));
}

void pattern_greys(uint len, uint t) {
    int max = 32; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(t * 0x10101);
        if (++t >= max) t = 0;
    }
}

typedef void (*pattern)(uint len, uint t);
const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
        //{pattern_snakes,  "Snakes!"},
        //{pattern_random,  "Random data"},
        {pattern_sparkle, "Sparkles"},
        //{pattern_greys,   "Greys"},
};

const int PIN_TX = 0;

const int p_len = 12; // first p has 12 leds
const int o_len = 7;  // o has 7 leds
const int p_2_len = 9;
const int o_off = p_len;
const int po_mix_idx = 9; // led 9 is part of o
const int num_leds = 32;
const int p_1_leds[] = {1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
const int o_leds[] =   {0,0,0,0,0,0,0,0,0,1,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
const int o_p_2_leds[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // only want that middle one
const int p_2_leds[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,1,1,1,0,1,1,1,1,0,0,0};
const int excl_leds[]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,1,1,1};

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
    }
}

// topology of led sign, defines which nodes are physically adjacent
#define NO_NEIGHBORS 255
// contains an index into adjacency_nodes per node (or 255 for no neighbors besides +/- 1)
uint8_t adjacency_refs[] = {
    NO_NEIGHBORS, NO_NEIGHBORS, NO_NEIGHBORS, 0, NO_NEIGHBORS, NO_NEIGHBORS, NO_NEIGHBORS, NO_NEIGHBORS, // - 7
    4, 8, 13, 17, 20, NO_NEIGHBORS, 24, 28, 32, NO_NEIGHBORS, 37, 41, NO_NEIGHBORS, 43, 47, 51, // - 23
    NO_NEIGHBORS, 55, 59, 63, 67, 71, NO_NEIGHBORS, NO_NEIGHBORS
};
// contains adjacent node entries
// i + 0 = number of entries
// i + 1..n = adjacent nodes
uint8_t adjacency_nodes[] =
    {
        3, 2, 11, 4,       // for 3
        3, 7, 9, 12,       // for 8
        4, 8, 10, 12, 18,  // for 9
        3, 9, 11, 18,      // for 10
        2, 3, 10,          // for 11
        3, 8, 9, 13,       // for 12
        3, 13, 15, 22,     // for 14
        3, 14, 16, 22,     // for 15
        4, 15, 17, 21, 28, // for 16
        3, 9, 10, 17,      // for 18
        1, 20,             // for 19
        3, 16, 20, 28,     // for 21
        3, 14, 15, 22,     // for 22
        3, 22, 24, 25,     // for 23
        3, 23, 24, 26,     // for 25
        3, 25, 27, 29,     // for 26
        3, 26, 28, 29,     // for 27
        3, 16, 21, 27,     // for 28
        3, 26, 27, 30,     // for 29
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
                return COLOR_BRG(interp(3500,4000, 200, 50, t), interp(3500, 4000, 130, 0, t), interp(3500, 4000, 80, 0, t));
            else
                return COLOR_BRG(interp(3500,4000, 40, 255, t),interp(3500,4000,200,0,t),interp(3500,4000,200,0,t));
        else
            if (idx)
                return COLOR_BRG(interp(4000,4500, 50, 5, t), 0,0);
            else
                return COLOR_BRG(interp(4000,4500, 255, 50, t),0,0);
}

uint8_t worm0 = 6;
uint8_t worm1 = 32;
uint8_t worm2 = 32;
uint8_t worm3 = 32;

uint32_t frame_buffer[32];

void paint_frame_buffer() {
    for (int i = 0; i < num_leds; ++i) put_pixel(frame_buffer[i]);
}
void decay_frame_buffer() {
    for (int i = 0; i < num_leds; ++i) {
        uint32_t c = frame_buffer[i];
        if (c != 0) {
            uint8_t c1 = (c >> 16) & 0xff;
            uint8_t c2 = (c >> 8) & 0xff;
            uint8_t c3 = c & 0xff;
            frame_buffer[i] = ((c1 >> 1) << 16) | ((c2 >> 1) << 8) | (c3 >> 1);
        }
    }
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

    paint_frame_buffer();
}

void story() {
    // worm finds the light
    // worm moves around randomly
    // worm gets to spin (faster and faster)
    // worm overheats
    // worm heartbeats
    // worm grows a sparkling longer tail
    // fade out
    // fireworks
    // p - o - p - !
    // pop! pulsating
    // pop! static
    // (smiley)
}

int main() {
    //set_sys_clock_48();
    stdio_init_all();
    puts("WS2812 Smoke Test");

    // todo get free sm
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, PIN_TX, 800000, false);

    //int t = 0;
    //while (1) {
        for (uint32_t i = 0; i < 5000; ++i) {
            worm_moves(i);
            sleep_ms(5);
        }
        
        for (int i = 0; i < 100; ++i) {
            show_letter(p_1_leds, p_1_color, i);
            sleep_ms(10);
        }
        for (int i = 0; i < 100; ++i) {
            show_letter(o_leds, o_color, i);
            sleep_ms(10);
        }
        for (int i = 0; i < 100; ++i) {
            show_letter(p_2_leds, p_2_color, i);
            sleep_ms(10);
        }
        for (int i = 0; i < 100; ++i) {
            show_letter(excl_leds, excl_color, i);
            sleep_ms(10);
        }
        for (int i = 0; i < 100; ++i) {
            show_all(i);
            sleep_ms(10);
        }
        // int pat = rand() % count_of(pattern_table);
        // int dir = (rand() >> 30) & 1 ? 1 : -1;
        // puts(pattern_table[pat].name);
        // puts(dir == 1 ? "(forward)" : "(backward)");
        // for (int i = 0; i < 1000; ++i) {
        //     pattern_table[pat].pat(num_leds, t);
        //     sleep_ms(10);
        //     t += dir;
        // }
    //}
}