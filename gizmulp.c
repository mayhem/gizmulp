#include <avr/io.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "gamma.h"

// Bit manipulation macros
#define sbi(a, b) ((a) |= 1 << (b))       //sets bit B in variable A
#define cbi(a, b) ((a) &= ~(1 << (b)))    //clears bit B in variable A
#define tbi(a, b) ((a) ^= 1 << (b))       //toggles bit B in variable A

#define HUE_MAX           252 
#define STEPS_PER_HEXTET   42

typedef struct
{
    uint8_t c[3];
} color_t;
uint8_t is_touched(void);

uint8_t seed_set = 0;

int32_t calibration;
color_t last_col;

volatile uint32_t g_time = 0;
uint16_t g_speed = 0;
uint8_t g_speed_index = 10;
int32_t SPEED_MIN = 64;
int32_t SPEED_MAX = 1;
int32_t SPEED_STEPS = 20;

int32_t SPEED_ADJUST_MIN = 20;
int32_t SPEED_ADJUST_MAX = 5;
int32_t SPEED_INCREMENT_BTH;
int32_t SPEED_ADJUST_INCREMENT_BTH;

ISR (TIMER0_OVF_vect)
{
    g_time++;
}

void led_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
#ifdef USE_GAMMA
    OCR1B = pgm_read_byte(&gamma_table[255 - blue]);
    OCR0B = pgm_read_byte(&gamma_table[255 - green]);
    OCR0A = pgm_read_byte(&gamma_table[255 - red]);
#else
    OCR1B = 255 - blue;
    OCR0B = 255 - green;
    OCR0A = 255 - red;
#endif
    last_col.c[0] = red;
    last_col.c[1] = green;
    last_col.c[2] = blue;
}

void led_color(color_t *col)
{
    led_rgb(col->c[0], col->c[1], col->c[2]);
}

void set_fade_color(color_t *from_col, color_t *to_col, int32_t steps, uint16_t index)
{
    color_t new_col;
    int32_t size;
    uint8_t  i;
  
    for(i = 0; i < 3; i++) 
    {
        size = ((int32_t)to_col->c[i] - (int32_t)from_col->c[i]) * 1000 / steps;
        new_col.c[i] = from_col->c[i] + (size * index / 1000);
    }
    led_color(&new_col);
}

void delay(uint16_t dly)
{
    for(; dly > 0; dly--)
        _delay_ms(1);
}

uint8_t fade(color_t *colors, uint8_t segments, uint16_t steps, uint8_t repeat, uint8_t hold)
{
    uint8_t i, n;
    uint16_t j, r;
    
    for(r = 0; repeat == 0 || r < repeat; r++)
    {
        i = (r % segments);
        n = (i + 1) % segments;
        for(j = 0; j < steps; j++)
        {
            if (is_touched())
                return 1;

            set_fade_color(&colors[i], &colors[n], steps, j);
            delay(g_speed);
        } 
        if (hold)
            delay((uint16_t)g_speed * (uint16_t)hold);
    }
    return 0;
}

void get_hue(uint32_t t, color_t *c) 
{
    uint8_t s, h;

    h = (uint8_t)(t & 0xFF);
    if (h >= HUE_MAX)
    {
        c->c[0] = HUE_MAX;
        c->c[1] = 0;
        c->c[2] = 0;

        return;
    }
    s = h % (252 / 6);
    switch(h / STEPS_PER_HEXTET) 
    {
        case 0:  // from 255, 0, 0 to 255, 255, 0
            c->c[0] = HUE_MAX;
            c->c[1] = s * 6;
            c->c[2] = 0;
            break;
        case 1: 
            c->c[0] = HUE_MAX - (s * 6);
            c->c[1] = HUE_MAX;
            c->c[2] = 0;
            break;
        case 2: 
            c->c[0] = 0;
            c->c[1] = HUE_MAX;
            c->c[2] = s * 6;
            break;
        case 3: 
            c->c[0] = 0;
            c->c[1] = HUE_MAX - (s * 6);
            c->c[2] = HUE_MAX;
            break;
        case 4: 
            c->c[0] = (s * 6);
            c->c[1] = 0;
            c->c[2] = HUE_MAX;
            break;
        case 5: 
            c->c[0] = HUE_MAX;
            c->c[1] = 0;
            c->c[2] = HUE_MAX - (s * 6);
            break;
    }
    c->c[0] += 3;
    c->c[1] += 3;
    c->c[2] += 3;
}

uint8_t rainbow(void)
{
    uint8_t i;
    color_t c;

    for(i = 0; !is_touched(); i++)
    {
        get_hue(i, &c);
        led_color(&c);
        delay(g_speed);

        if (i == HUE_MAX)
            i = 0;
    }
    return 1;
}

uint8_t random_fade(void)
{
    uint8_t t = 0;
    color_t c[2];

    for(;;)
    {
        get_hue(t, &c[0]);
        get_hue(t + 128, &c[1]);

        if (fade(c, 2, 255, 0, 25))
            return 1;

        t += 10;
    }
}

void setup(void)
{
    // PB4 = OC1B = blue
    // PB1 = OC0B = red
    // PB0 = 0C0A = green

    /* Set to Fast PWM */
    TCCR0A |= _BV(WGM01) | _BV(WGM00) | _BV(COM0A1) | _BV(COM0B1) | _BV(COM0A0) | _BV(COM0B0);
    GTCCR |= _BV(PWM1B) | _BV(COM1B1) | _BV(COM1B0);

    // Set the clock source
    TCCR0B |= _BV(CS00) | _BV(CS01);
    TCCR1 = 3<<COM1A0 | 7<<CS10;

    // Reset timers and comparators
    OCR0A = 0;
    OCR0B = 0;
    OCR1B = 0;
    TCNT0 = 0;
    TCNT1 = 0;

    TIMSK |= (1<<TOIE0);

    // Setup PWM pins as output
    DDRB = (1 << PB0) | (1 << PB1) | (1 << PB4); // | (1 << PB3);

    SPEED_INCREMENT_BTH = (SPEED_MIN - SPEED_MAX) * 1000 / SPEED_STEPS;
    SPEED_ADJUST_INCREMENT_BTH = (SPEED_ADJUST_MIN - SPEED_ADJUST_MAX) * 1000 / SPEED_STEPS;
    sei();
}

#define NUM_PATTERNS 5
color_t citrus[3] = { { 255, 128, 0 }, { 255, 255, 0 }, { 255, 00, 0 } };
color_t test[6] = 
{ 
    { 255,   0,   0 }, 
    { 255, 255,   0 }, 
    {   0, 255,   0 }, 
    {   0, 255, 255 }, 
    {   0,   0, 255 }, 
    { 255,   0, 255 } 
};
color_t test2[6] = 
{ 
    {   0,   0,   0 }, 
    { 255,   0,   0 }, 
    {   0,   0,   0 }, 
    {   0, 255,   0 }, 
    {   0,   0,   0 }, 
    {   0,   0, 255 }, 
};
color_t blue_throb[2] = { { 0, 0, 24}, { 0, 0, 40} };
color_t red_throb[2] = { { 24, 0, 0}, { 40, 0, 0} };
color_t candy_ho[4] = { { 255, 0, 255 }, { 75, 0, 138}, { 0, 0, 255}, { 75, 0, 138 } };
color_t xmas[3] = { { 255, 0, 0}, { 0, 64, 0}, {255, 255, 255} };

uint16_t get_speed(void)
{
    return SPEED_MAX + (SPEED_INCREMENT_BTH * g_speed_index / 1000);
}

void speed_setting(void)
{
    uint8_t i, state = 0;
    uint32_t t = 0;

    for(i = 0; i < 4; i++)
    {
        delay(250);
        if (!is_touched())
            return;
    }
    for(i = 0; is_touched(); i++)
    {
        if (t == 0 || g_time >= t)
        {
            if (state)
                led_rgb(0, 0, 0);
            else
                led_rgb(0, 0, 255);
            state = !state;
            t = g_time + SPEED_ADJUST_MAX + (SPEED_ADJUST_INCREMENT_BTH * g_speed_index / 1000);
        }
        while(g_time < t)
            ;
        if (i % 10 == 0)
        {
            g_speed_index--;
            if (g_speed_index == 0)
                g_speed_index = SPEED_STEPS;
        }
    }
    g_speed = get_speed();
}

void touch_feedback(void)
{
    color_t colors[2];
    uint8_t saved;

    led_rgb(255, 255, 255);
    delay(250);

    if (is_touched())
        speed_setting();

    colors[0].c[0] = 255;
    colors[0].c[1] = 255;
    colors[0].c[2] = 255;
    colors[1].c[0] = 0;
    colors[1].c[1] = 0;
    colors[1].c[2] = 0;

    // hacky. :(
    saved = g_speed;
    g_speed = 1;
    fade(colors, 2, 64, 1, 0);
    g_speed = saved;
}

uint8_t charge_time(uint8_t pin)
{
    uint8_t mask = (1 << pin);
    uint8_t i;

    DDRB &= ~mask; 
    PORTB |= mask; 

    for (i = 0; i < 16; i++) 
    {
        if (PINB & mask) 
            break;
    }

    PORTB &= ~mask; 
    DDRB |= mask; 

    return i;
}

uint8_t is_touched(void)
{
    int32_t n = charge_time(PB2);

    if (!seed_set && n > calibration)
    {
        seed_set = 1;
        srand(TCNT1);
    }

    return n > calibration;
}    

int main(int argc, char *argv[])
{
    uint8_t i, index = 0, touched = 0;

    setup();

    g_speed = get_speed();
    _delay_ms(100);
    for (i = 0; i < 8; i++) 
    {
        calibration += charge_time(PB2);
        _delay_ms(20);
    }
    srand(TCNT1 * calibration);
    calibration = (calibration + 4) / 6;

    for(;;)
    {
        switch(index)
        {
            case 0:
                touched = fade(candy_ho, 4, 128, 0, 50);
                break;
            case 1:
                touched = fade(citrus, 3, 128, 0, 50);
                break;
            case 2:
                touched = random_fade();
                break;
            case 3:
                touched = rainbow();
                break;
            case 4:
                touched = fade(xmas, 3, 128, 0, 50);
                break;
        }
        if (touched)
            touch_feedback();

        index = (index+1) % NUM_PATTERNS;
    }

    return 0;
}
