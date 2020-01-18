#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <sprite.h>
#include <stdbool.h>
#include <cpu_speed.h>
#include <avr/io.h>
#include <macros.h>
#include <graphics.h>
#include <lcd.h>
#include "lcd_model.h"
#include <assert.h>
#include <cab202_adc.h>
#include <avr/pgmspace.h>
#include <ram_utils.h>
#include <usb_serial.h>

#define DELAYMS (20)
#define STATUSHEIGHT (5)
#define PLATWIDTH (10)
#define COLWIDTH (PLATWIDTH + 2)
#define ROWHEIGHT (PLATHEIGHT + HEROHEIGHT + 1)
#define MAXPLATS (21)
#define PLATHEIGHT (2)
#define OCCUPIEDPERCENT (100)
#define FORBPERCENT (25)
#define TREASUREWIDTH (6) 
#define TREASUREHEIGHT (3)
#define HEROWIDTH (6)
#define HEROHEIGHT (5)
#define ZOMBIEWIDTH (4)
#define ZOMBIEHEIGHT (3)
#define MAXTRIALS (1000)
#define MAXCOLUMNS (9)
#define MAXROWS (4)
#define FREQ     (8000000.0)
#define PRESCALE (1024.0)
#define MAXFOOD (5)
#define FOODWIDTH (2)
#define FOODHEIGHT (2)
#define MAXZOMBIES (1)
#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)
#define BRIGHT_MAX (1023)

Sprite hero;
Sprite safe_plat;
Sprite foodbox[MAXFOOD];

const uint8_t hero_bm[] PROGMEM = {
        0b00110000,
        0b00110000,
        0b11111100,
        0b00110000,        
        0b01001000,
};

uint8_t food_bm[] PROGMEM = {
        0b11000000,
        0b11000000
};

Sprite treasure;

uint8_t treasure_bm[] PROGMEM = {
        0b11111100,
        0b10000100,
        0b11111100,
};

Sprite platforms[MAXPLATS];

uint8_t safe_b_bm[] PROGMEM = {
        0b11111111, 0b11000000,
        0b11111111, 0b11000000,
};

uint8_t forb_b_bm[] PROGMEM = {
        0b10101010, 0b10000000,
        0b11111111, 0b11000000,
};

Sprite zombies[MAXZOMBIES];

uint8_t zombie_bm[] PROGMEM = {
        0b01100000,
        0b11110000,        
        0b01100000,
};

uint8_t wipescreen[8] = {
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
};

uint8_t direct_draw[8];
uint8_t x, y;

bool is_safe[MAXPLATS];
bool game_start = false;
bool game_over = false;
bool treasure_paused = false;
bool game_pause = false;
bool jumping = false;
bool allzombiesdead = false;
bool complete = false;
bool complete1 = false;
int finish_ctr = 0;
int finish_ctr1 = 0;
int jump_ctr = 0;
int num_plats;
int score = 0;
int lives = 10;
int time;
int key;
double timer0;
int seconds;
int minutes;
int moving_right_ctr = 0;
int moving_left_ctr = 0;
int food_ctr = 0;
int food_display = 5;
int dead_zombies = 0;
int timeshot;
int joyup_ctr = 0;
int joydown_ctr = 0;
int joyleft_ctr = 0;
int joyright_ctr = 0;
int joycentre_ctr = 0;
int leftb_ctr = 0;
int rightb_ctr = 0;
int time1;
double platdx = 0.2;
char buffer[100];
volatile int game_time_counter = 0;
volatile int zombie_time_counter = 0;

void DEBUG() {
	draw_string(LCD_X / 2 - 12.5, LCD_Y / 2 - 7, "DEBUG", FG_COLOUR);
}

void new_lcd_init(uint8_t contrast) {
    SET_OUTPUT(DDRD, SCEPIN);
    SET_OUTPUT(DDRB, RSTPIN);
    SET_OUTPUT(DDRB, DCPIN);
    SET_OUTPUT(DDRB, DINPIN);
    SET_OUTPUT(DDRF, SCKPIN);

    CLEAR_BIT(PORTB, RSTPIN); 
    SET_BIT(PORTD, SCEPIN);   
    SET_BIT(PORTB, RSTPIN);   

    LCD_CMD(lcd_set_function, lcd_instr_extended);
    LCD_CMD(lcd_set_contrast, contrast);
    LCD_CMD(lcd_set_temp_coeff, 0);
    LCD_CMD(lcd_set_bias, 3);

    LCD_CMD(lcd_set_function, lcd_instr_basic);
    LCD_CMD(lcd_set_display_mode, lcd_display_normal);
    LCD_CMD(lcd_set_x_addr, 0);
    LCD_CMD(lcd_set_y_addr, 0);
}

void setup_direct_lcd_draw(void) {
    for (int i = 0; i < 8; i++) 
    {
        for (int j = 0; j < 8; j++) 
        {
            uint8_t bit_val = BIT_VALUE(wipescreen[j], (7 - i));
            WRITE_BIT(direct_draw[i], j, bit_val);
        }
    }
}

void direct_lcd_draw(void) {
    x = 0;
    y = 0;
    lcd_position(x,y);

    while(!complete)
    {
        for (int i = 0; i < 8; i++) {
            LCD_DATA(direct_draw[i]);
        }
        
        x += 1;
        finish_ctr++;

        if(finish_ctr == 70)
        {
            complete = true;
            finish_ctr = 0;
        }

        _delay_ms(30);
    }
}

void erase_direct_lcd_draw(void) {
    x = 0;
    y = 0;
    lcd_position(x, y);

    while(!complete1)
    {
        for (int i = 0; i < 8; i++) {
            LCD_DATA(0);
        }
        x += 1;
        finish_ctr1++;

        if(finish_ctr1 == 70)
        {
            complete1 = true;
        }

        _delay_ms(30);
    }
}

int get_col(Sprite s)
{
    return (int)round(s.x) / COLWIDTH;
}

int get_row(Sprite s)
{
    return ((int)round(s.y) + PLATHEIGHT - STATUSHEIGHT) / ROWHEIGHT - 1;
}

void setup_safe_platform()
{
    sprite_init(&platforms[0], 0, 5, PLATWIDTH, PLATHEIGHT, load_rom_bitmap(safe_b_bm, 4));
    is_safe[0] = true;
}

void setup_hero()
{   
    sprite_init(&hero, platforms[0].x, platforms[0].y - HEROHEIGHT, HEROWIDTH, HEROHEIGHT, load_rom_bitmap(hero_bm, 5));
    hero.dx = 0.5;
    hero.dy = 1;
}

void setup_treasure()
{
    sprite_init(&treasure, 0, LCD_Y - TREASUREHEIGHT, TREASUREWIDTH, TREASUREHEIGHT, load_rom_bitmap(treasure_bm, 3));
    treasure.dx = 0.5;
    treasure.dy = 0;
}

void setup_foodbox()
{
    for(int i = 0; i < MAXFOOD; i++)
    {
        sprite_init(&foodbox[i], -10, -10, FOODWIDTH, FOODHEIGHT, load_rom_bitmap(food_bm, 2));
        foodbox[i].is_visible = false;
        foodbox[i].dx = 0;
        foodbox[i].dy = 0;
    }
}

void setup_zombies()
{
    for(int i = 0; i < MAXZOMBIES; i++)
    {
        sprite_init(&zombies[i], PLATWIDTH * 2 + (i * 10), -ZOMBIEHEIGHT - 1, ZOMBIEWIDTH, ZOMBIEHEIGHT, load_rom_bitmap(zombie_bm, 3));
        zombies[i].dx = 0.3;
        zombies[i].dy = 1;
    }
}

void draw_int(uint8_t x, uint8_t y, int value, colour_t colour) {
    snprintf(buffer, sizeof(buffer), "%d", value);
    draw_string(x, y, buffer, colour);
}

void start_screen(void)
{     
    draw_string(15, 12, "Jacob Kraut", FG_COLOUR);
    draw_string(19, 20, "n10282653", FG_COLOUR);
    draw_string(0, 30, "Press SW2 to Cont", FG_COLOUR);
    show_screen();
}

void draw_double(uint8_t x, uint8_t y, double value, colour_t colour) {
    snprintf(buffer, sizeof(buffer), "%f", value);
    draw_string(x, y, buffer, colour);
}

void stats_screen()
{
    clear_screen();
    draw_string(0,  0, "Lives:", FG_COLOUR);
    draw_int   (45, 0, lives, FG_COLOUR);

    draw_string(0,  10, "Score:", FG_COLOUR);
    draw_int   (45, 10, score, FG_COLOUR);
    
    draw_string(0,  20, "Time:", FG_COLOUR);
    draw_int   (35, 20, minutes, FG_COLOUR);

    draw_string(45, 20, ":", FG_COLOUR);
    draw_int   (50, 20, seconds, FG_COLOUR);

    draw_string(0,  30, "Zombies:", FG_COLOUR);
    draw_int   (45, 30, MAXZOMBIES - dead_zombies, FG_COLOUR);

    draw_string(0,  40, "Food:", FG_COLOUR);
    draw_int   (45, 40, food_display, FG_COLOUR);
    show_screen();
}

bool sprites_collide(Sprite s1, Sprite s2)
{
    int top1 = round(s1.y);
    int bottom1 = round(top1 + s1.height - 1);
    int left1 = round(s1.x);
    int right1 = round(left1 + s1.width - 1);

    int top2 = round(s2.y);
    int bottom2 = round(top2 + s2.height - 1);
    int left2 = round(s2.x);
    int right2 = round(left2 + s2.width - 1);

    return !( (top1 > bottom2) || 
              (top2 > bottom1) || 
              (right1 < left2) || 
              (right2 < left1));
}

void setup_platforms(void)
{
    int w = LCD_X;
    // int h = LCD_Y;

    int cols = w / COLWIDTH;
    // int rows = (h - 2) / ROWHEIGHT;
    int rows = 3;

    int wanted = rows * cols * OCCUPIEDPERCENT / 100;
    int out_of = rows * cols;

    num_plats = 0;

    // int safe_per_col[MAXPLATS] = {0};

    for(int row = 0; row < rows; row++)
    {
        for(int col = 0; col < cols; col++)
        {
            if(num_plats > MAXPLATS)
            {
                break;
            }

            double p = (double)rand() / RAND_MAX;

            if(p <= (double)wanted / out_of)
            {
                wanted--;
                sprite_id platform = &platforms[num_plats];
                int x = col * (w - COLWIDTH) / (cols - 1) + rand() % (COLWIDTH - PLATWIDTH);
                int y = 5 + HEROHEIGHT + (row + 1) * ROWHEIGHT - PLATHEIGHT - 5;

                sprite_init(platform, x, y, PLATWIDTH, PLATHEIGHT, load_rom_bitmap(safe_b_bm, 4));

                is_safe[num_plats] = true;
                // safe_per_col[col]++;
                num_plats++;            
            }

            out_of--;
        }
    }

    int num_forbidden = num_plats * FORBPERCENT / 100;

    if(num_forbidden < 2)
    {
        num_forbidden = 2;
    }

    for(int i = 0; i < num_forbidden; i++)
    {
        for(int trial = 0; trial < MAXTRIALS; trial++)
        {
            int platform_index = 1 + rand() % (num_plats- 1);
            // Sprite platform = platforms[platform_index];
            // int col = get_col(platform);

            // if(safe_per_col[col] > 1)
            // {
                is_safe[platform_index] = false;
                // safe_per_col[col]--;
                platforms[platform_index].bitmap = load_rom_bitmap(forb_b_bm, 4);
                break;
            // }
        }
    }
}


void draw()
{
    clear_screen();
    // draw_int(LCD_X/2, 0, joycentctr, FG_COLOUR);
    // draw_int(LCD_X/2 + 5, 0, flash_ctr, FG_COLOUR);
    sprite_draw(&treasure);
    sprite_draw(&hero);
    sprite_draw(&safe_plat);

    for(int k = 0; k < MAXPLATS; k++)
    {
        sprite_draw(&platforms[k]);
    }

    for(int i = 0; i < MAXFOOD; i++)
    {
        sprite_draw(&foodbox[i]);
    }

    for(int j = 0; j < MAXZOMBIES; j++)
    {
        sprite_draw(&zombies[j]);
    }

    show_screen();
}

int get_current_platform(Sprite s)
{
    int sl = (int)round(s.x);
    int sr = sl + s.width - 1;
    int sy = (int)round(s.y);

    for(int plat = 0; plat < num_plats; plat++)
    {
        Sprite p = platforms[plat];
        int pl = (int)round(p.x);
        int pr = pl + p.width - 1;
        int py = (int)round(p.y);

        if(sr >= pl && sl <= pr && sy == py - s.height)
        {
            return plat;
        }
    } 

    return -1;
}

void setup_teensy()
{
    set_clock_speed(CPU_8MHz);
    new_lcd_init(65);
    usb_init();
    adc_init();
    CLEAR_BIT(DDRF, 6);
    CLEAR_BIT(DDRF, 5);
    CLEAR_BIT(DDRD, 0);
    CLEAR_BIT(DDRD, 1);
    CLEAR_BIT(DDRB, 1);
    CLEAR_BIT(DDRB, 0);
    CLEAR_BIT(DDRB, 7);    
    SET_BIT(DDRB, 2);
	SET_BIT(DDRB, 3);
    TCCR0A = 0;
	TCCR0B = 5; 
	TIMSK0 = 1; 
    TCCR1A = 0;
	TCCR1B = 1;
	TIMSK1 = 1;
    time = 0;
    game_time_counter = 0;
    TC4H = OVERFLOW_TOP >> 8;
	OCR4C = OVERFLOW_TOP & 0xff;
	TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7);
	TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
	TCCR4D = 0;
    sei();
}

void setup()
{
    srand(TCNT1);
    lives = 1;
    score = 0;
    time = 0;
    game_time_counter = 0;
    setup_direct_lcd_draw();
    setup_treasure();
    setup_platforms();
    setup_safe_platform();
    setup_hero();
    setup_zombies();
    setup_foodbox();
}

volatile uint8_t joyup_count = 0;
volatile uint8_t joyup_closed = 0;

volatile uint8_t joydown_count = 0;
volatile uint8_t joydown_closed = 0;

volatile uint8_t joyleft_count = 0;
volatile uint8_t joyleft_closed = 0;

volatile uint8_t joyright_count = 0;
volatile uint8_t joyright_closed = 0;

volatile uint8_t joycentre_count = 0;
volatile uint8_t joycentre_closed = 0;

volatile uint8_t leftb_count = 0;
volatile uint8_t leftb_closed = 0;

volatile uint8_t rightb_count = 0;
volatile uint8_t rightb_closed = 0;

ISR(TIMER0_OVF_vect) {
	game_time_counter ++;
}

ISR(TIMER1_OVF_vect) {
	uint8_t mask = 0b00001111;

	joyup_count = ((joyup_count << 1) & mask) | BIT_VALUE(PIND, 1);
    joydown_count = ((joydown_count << 1) & mask) | BIT_VALUE(PINB, 7);
	joyleft_count = ((joyleft_count << 1) & mask) | BIT_VALUE(PINB, 1);
    joyright_count = ((joyright_count << 1) & mask) | BIT_VALUE(PIND, 0);
	joycentre_count = ((joycentre_count << 1) & mask) | BIT_VALUE(PINB, 0);
    leftb_count = ((leftb_count << 1) & mask) | BIT_VALUE(PINF, 6);
	rightb_count = ((rightb_count << 1) & mask) | BIT_VALUE(PINF, 5);

	if (joyup_count == 0)
	{
		joyup_closed = 0;
	}

	else if (joyup_count == mask)
	{
		joyup_closed = 1;
	}

	if (joydown_count == 0)
	{
		joydown_closed = 0;
	}
	else if (joydown_count == mask)
	{
		joydown_closed = 1;
	}

    if (joyleft_count == 0)
	{
		joyleft_closed = 0;
	}
	else if (joyleft_count == mask)
	{
		joyleft_closed = 1;
	}

    if (joyright_count == 0)
	{
		joyright_closed = 0;
	}
	else if (joyright_count == mask)
	{
		joyright_closed = 1;
	}

    if (joycentre_count == 0)
	{
		joycentre_closed = 0;
	}
	else if (joycentre_count == mask)
	{
		joycentre_closed = 1;
	}

    if (leftb_count == 0)
	{
		leftb_closed = 0;
	}
	else if (leftb_count == mask)
	{
		leftb_closed = 1;
	}

    if (rightb_count == 0)
	{
		rightb_closed = 0;
	}
	else if (rightb_count == mask)
	{
		rightb_closed = 1;
	}
}

bool joyup_pressed()
{
    static uint8_t joyup_prevState = 0;

    if(joyup_closed != joyup_prevState) 
    {
        joyup_prevState = joyup_closed;
        joyup_ctr++;

        if(joyup_ctr % 2 == 0 && joyup_ctr != 0)
        {
            return true;
        }
    }

    return false;
}

bool joydown_pressed()
{
    static uint8_t joydown_prevState = 0;

    if(joydown_closed != joydown_prevState) 
    {
        joydown_prevState = joydown_closed;
        joydown_ctr++;

        if(joydown_ctr % 2 == 0 && joydown_ctr != 0)
        {
            return true;
        }
    }

    return false;
}

bool joyleft_pressed()
{
    static uint8_t joyleft_prevState = 0;

    if(joyleft_closed != joyleft_prevState) 
    {
        joyleft_prevState = joyleft_closed;
        joyleft_ctr++;

        if(joyleft_ctr % 2 == 0 && joyleft_ctr != 0)
        {
            return true;
        }
    }

    return false;
}

bool joyright_pressed()
{
    static uint8_t joyright_prevState = 0;
    
    if(joyright_closed != joyright_prevState) 
    {
        joyright_prevState = joyright_closed;
        joyright_ctr++;

        if(joyright_ctr % 2 == 0 && joyright_ctr != 0)
        {
            return true;
        }
    }

    return false;
}

bool joycentre_pressed()
{
    static uint8_t joycentre_prevState = 0;

    if(joycentre_closed != joycentre_prevState) 
    {
        joycentre_prevState = joycentre_closed;
        joycentre_ctr++;

        if(joycentre_ctr % 2 == 0 && joycentre_ctr != 0)
        {
            return true;
        }
    }

    return false;
}

bool leftb_pressed()
{
    static uint8_t leftb_prevState = 0;

    if(leftb_closed != leftb_prevState) 
    {
        leftb_prevState = leftb_closed;
        leftb_ctr++;

        if(leftb_ctr % 2 == 0 && leftb_ctr != 0)
        {
            return true;
        }
    }

    return false;
}

bool rightb_pressed()
{
    static uint8_t rightb_prevState = 0;

    if(rightb_closed != rightb_prevState) 
    {
        rightb_prevState = rightb_closed;
        rightb_ctr++;

        if(rightb_ctr % 2 == 0 && rightb_ctr != 0)
        {
            return true;
        }
    }

    return false;
}

void set_duty_cycle(int duty_cycle) {
	TC4H = duty_cycle >> 8;
	OCR4A = duty_cycle & 0xff;
}

bool game_over_screen()
{
    clear_screen();
    draw_string(0, 0, "GAME OVER!", FG_COLOUR);
    draw_string(0, 10, "Score:", FG_COLOUR);
    draw_int(40, 10, score, FG_COLOUR);
    draw_string(0, 20, "Play Time", FG_COLOUR);
    draw_int(50, 20, time / 60, FG_COLOUR);
    draw_string(60, 20, ":", FG_COLOUR);
    draw_int(65, 20, time % 60, FG_COLOUR);
    draw_string(0, 30, "SW3 - Play Again", FG_COLOUR);
    draw_string(0, 40, "SW2 - Finish", FG_COLOUR);
    show_screen();

    while(!rightb_pressed() || !leftb_pressed() || key != 'r' || key != 'q')
    {
        if(usb_configured())
        {
            if(usb_serial_available())
            {
                key = usb_serial_getchar();
            }
        }

        if(rightb_pressed() || key == 'r')
        {
            return true;
            key = -1;
        }

        else if(leftb_pressed() || key == 'q')
        {
            return false;
            key = -1;
        }
    }

    return 0;

}

void finish_message()
{
    clear_screen();
    draw_string(19, 20, "n10282653", FG_COLOUR);
    show_screen();
}

bool all_zombies_landed()
{
    for(int i = 0; i < MAXZOMBIES; i++)
    {
        if(zombies[i].x < 0 || zombies[i].x > LCD_X)
        {
            return true;
        }

        if(get_current_platform(zombies[i]) == -1 && zombies[i].y < LCD_Y)
        {
            return false;
        }
    }

    return true;
}

void process()
{
    int heroplat = get_current_platform(hero);

    time = ( game_time_counter * 256.0 + TCNT0 ) * PRESCALE  / FREQ;
    timer0 = ( game_time_counter * 256.0 + TCNT0 ) * PRESCALE  / FREQ;

    seconds = time % 60;
    minutes = time / 60;

    if(!all_zombies_landed())
    {
        time1 = timer0 * 8;

        if(time1 % 2 == 0)
        {
            SET_BIT(PORTB, 2);
            SET_BIT(PORTB, 3);
        }

        else
        {
            CLEAR_BIT(PORTB, 2);
            CLEAR_BIT(PORTB, 3);
        }
    }
    
    else if(all_zombies_landed())
    {
        CLEAR_BIT(PORTB, 2);
        CLEAR_BIT(PORTB, 3);
    }

    static int top_row = 0;
    static int middle_row = 1;
    static int bottom_row = 2;

    static bool falling = false;
    static bool direction;

    bool died = false; 
    
    if(usb_configured())
    {
        if (usb_serial_available()) 
        {
            key = usb_serial_getchar();
        }
    }

    if(heroplat >= 0)
    {
        if(is_safe[heroplat])
        {
            if(falling)
            {
                score++;
            }

            falling = false;
            hero.dx = 0;

            if((joyleft_pressed() || key == 'a') && heroplat != 0)
            {   
                moving_left_ctr += 1;
                moving_right_ctr -= 1;

                if(moving_left_ctr > 1)
                {
                    moving_left_ctr = 1;
                }
                else if(moving_left_ctr < 0)
                {
                    moving_left_ctr = 0;
                }
                
                direction = false;
            }
            if(joyright_pressed() || key == 'd')
            {
                moving_right_ctr += 1;
                moving_left_ctr -= 1;

                if(moving_right_ctr > 1)
                {
                    moving_right_ctr = 1;
                }
                else if(moving_right_ctr < 0)
                {
                    moving_right_ctr = 0;
                }
                direction = true;
            }

            if((joyup_pressed() || key == 'w') && heroplat != 0)
            {
                jumping = true;
            }    

            if(joydown_pressed() || key == 's')   
            {
                if(food_ctr < 5 && heroplat != 0)
                {
                    foodbox[food_ctr].is_visible = true;
                    foodbox[food_ctr].x = hero.x + 2;
                    foodbox[food_ctr].y = hero.y + HEROHEIGHT - FOODHEIGHT;
                    foodbox[food_ctr].dx = platforms[heroplat].dx;
                    food_ctr++;
                    food_display--;
                }
            }

            if(moving_right_ctr == 1)
            {
                hero.dx = 0.7;
                hero.x += hero.dx;
            }
    
            if(moving_left_ctr == 1)
            {
                hero.dx = -0.7;
                hero.x += hero.dx;
            }

            hero.x += platforms[heroplat].dx;

            if(heroplat != 0)
            {
                platforms[0].y -= 1000;
            }
        }
        else
        {
            died = true;
            moving_left_ctr = 0;
            moving_right_ctr = 0;
        }
    }
    else
    {
        if(!jumping)
        {
            falling = true;
            hero.dy = 1;
        }

        hero.y += hero.dy;
        hero.x += hero.dx;

        if(hero.dx > 0)
        {
            hero.dx -= 0.02;
        }

        if(jump_ctr == 5)
        {
            jumping = false;
            jump_ctr = 0;
            hero.dy = 1;
        }

        moving_left_ctr = 0;
        moving_right_ctr = 0;
    }

    if(jumping)
    {   
        hero.dy = -hero.dy;
        hero.dy += -1.5;
        jump_ctr++;
        hero.y += hero.dy;
        hero.x += hero.dx;
    }

    int hl = (int)round(hero.x);
    int hr = hl + hero.width - 1;
    int ht = (int)round(hero.y);
    int hb = ht + hero.height - 1;

    if(hl < 0 || hr >= LCD_X || hb >= LCD_Y)
    {
        died = true;
    }

    for(int i = 0; i < MAXPLATS; i++)
    {
        int pl = (int)round(platforms[i].x);
        int pr = pl + platforms[i].width - 1;
        int pt = (int)round(platforms[i].y);
        int pb = pt + platforms[i].height - 1;

        if(sprites_collide(hero, platforms[i]))
        {
            if(hr > pl)
            {
                hero.x -= hero.dx;
                hero.dx = 0;
                hero.dy = 1;
            }

            if(hl < pr)
            {
                hero.x += hero.dx;
                hero.dx = 0;
                hero.dy = 1;
            }

            if(ht < pb)
            {
                jumping = false;
                hero.y += hero.dy;
                hero.dy = 1;
            }
        }
    }

    if(rightb_pressed() || key == 't')
    {
        treasure_paused = !treasure_paused;
    }

    if(!treasure_paused)
    {
        treasure.x += treasure.dx;
        int tl = (int)round(treasure.x);
        int tr = tl + treasure.width - 1;

        if(tl < 0 || tr >= LCD_X)
        {
            treasure.x -= treasure.dx;
            treasure.dx = -treasure.dx;
        }
    }

    if(sprites_collide(treasure, hero))
    {
        falling = false;
        treasure.y = -1000;
        lives += 2;
        setup_safe_platform();
        setup_hero();
    }

    for(int i = 0; i < MAXFOOD; i++)
    {
        for(int k = 0; k < MAXZOMBIES; k++)
        {
            if(sprites_collide(zombies[k], foodbox[i]))
            {
                zombies[k].y = LCD_Y;
                foodbox[i].y = -100;
                score += 10;
                food_ctr--;
                food_display++;
                break;
            }
        }
    }

    // for(int i = 0; i < MAXZOMBIES; i++)
    // {
    //     if(sprites_collide(hero, zombies[i]))
    //     {
    //         died = true;
    //     }
    // }
    
    if(died)
    {
        falling = false;
        lives--;
        moving_left_ctr = 0;
        moving_right_ctr = 0;

        if(lives > 0)
        {
            for(int i = 0, k = 0; i < BRIGHT_MAX; i += 20, k += 1)
            {
                set_duty_cycle(0 + i);
                lcd_init(65 - k);
                _delay_ms(40);
            }

            for(int i = 0, k = 0; i < BRIGHT_MAX; i += 20, k += 1)
            {
                set_duty_cycle(0 - i);
                lcd_init(15 + k);
                _delay_ms(40);
            }
            
            setup_safe_platform();
            setup_hero();
        }
        else
        {   
            direct_lcd_draw();
            erase_direct_lcd_draw();
            complete = false;
            complete1 = false;
            finish_ctr = 0;
            finish_ctr1 = 0;

            if(game_over_screen())
            {
                setup();
            }
            else
            {
                game_over = true;
                game_start = false;
                finish_message();
                return;
            }
        }
    }

    for(int i = 0; i < MAXZOMBIES; i++)
    {   
        if(seconds >= 3)
        {
            zombies[i].y += zombies[i].dy;
        }

        int zombieplat = get_current_platform(zombies[i]);
        Sprite zombielandedplat = platforms[zombieplat];
        
        int dist1 = 0 - zombies[i].x + ZOMBIEWIDTH / 2;
        int dist2 = zombies[i].x + ZOMBIEWIDTH / 2 - LCD_X;

        if(zombieplat >= 1)
        {
            zombies[i].dy = 0;

            zombies[i].x += zombielandedplat.dx;
            zombies[i].x += zombies[i].dx;

            int zl = (int)round(zombies[i].x + 2);
            int zr = (int)round(zl - 1);
            int plat_tracker = 0;

            if(zl <= zombielandedplat.x || zr >= zombielandedplat.x + PLATWIDTH)
            {   
                for(int k = 0; k < MAXPLATS; k++)
                {
                    if(sprites_collide(zombies[i], platforms[k]))
                    {
                        plat_tracker++;
                    }
                }

                if(plat_tracker < 2)
                {
                    zombies[i].x -= zombies[i].dx;
                    zombies[i].dx = -zombies[i].dx;
                }
                else
                {
                    break;
                }
            }


        }
        
        else if(zombies[i].x + ZOMBIEWIDTH / 2 < 0)
        {
            zombies[i].dy = 0;
            zombies[i].x += LCD_X + dist1;
        }

        else if(zombies[i].x > LCD_X)
        {
            zombies[i].dy = 0;
            zombies[i].x -= LCD_X + dist2;
        }

        else if(zombies[i].x > 0 || zombies[i].x + ZOMBIEWIDTH < LCD_X)
        {
            zombies[i].dy = 1;
        }

        if(zombies[i].y == LCD_Y)
        {
            dead_zombies++;
            zombies[i].y += 20;
        }
    }

    if(dead_zombies == MAXZOMBIES)
    {
        allzombiesdead = true;
        time1 = timer0 * 8;
        if(time1 % 2 == 0)
        {
            SET_BIT(PORTB, 2);
            SET_BIT(PORTB, 3);
        }

        else
        {
            CLEAR_BIT(PORTB, 2);
            CLEAR_BIT(PORTB, 3);
        }
    }

    if(!allzombiesdead)
    {
        timeshot = time;
    }

    if(time % timeshot >= 3)
    {
        dead_zombies = 0;
        allzombiesdead = false;
        setup_zombies();
    }
    int right_adc = adc_read(1);

    if(right_adc >= 0 && right_adc <= 250)
    {
        platdx = 0;
    }

    else if(right_adc > 251 && right_adc <= 500)
    {
        platdx = 0.25;
    }

    else if(right_adc > 501 && right_adc <= 750)
    {
        platdx = 0.35;
    }

    else if(right_adc > 750 && right_adc < 1000)
    {
        platdx = 0.45;
    }

    else if(right_adc > 1001)
    {
        platdx = 0.65;
    }

    for(int i = 0; i < MAXPLATS; i++)
    {
        if((get_row(platforms[i]) == 1))
        {
            platforms[i].dx = platdx;
        }

        else if(get_row(platforms[i]) == 0 || get_row(platforms[i]) == 2)
        {
            platforms[i].dx = -platdx;
        }
    }

    for(int i = 0; i < MAXPLATS; i++)
    {
        platforms[i].x += platforms[i].dx;

        if(platforms[i].x > LCD_X && (get_row(platforms[i]) == middle_row))
        {
            platforms[i].x -= LCD_X + PLATWIDTH;
        }

        else if(platforms[i].x + PLATWIDTH < 0 && (get_row(platforms[i]) == top_row || get_row(platforms[i]) == bottom_row))
        {
            platforms[i].x += LCD_X + PLATWIDTH;
        }
    }

    for(int i = 0; i < MAXFOOD; i++)
    {
        foodbox[i].x += foodbox[i].dx;

        int foodplat = get_current_platform(foodbox[i]);
        Sprite foodplatform = platforms[foodplat];


        int fooddist1 = LCD_X - foodplatform.x;
        int fooddist2 = LCD_X - foodbox[i].x + FOODWIDTH/2;
        int fooddist3 = fooddist1 - fooddist2;

        if(foodbox[i].x - fooddist3 > LCD_X && foodplatform.dx > 0)
        {
            foodbox[i].x -= LCD_X + PLATWIDTH;
        }

        else if(foodbox[i].x + fooddist3 < 0 && foodplatform.dx < 0)
        {
            foodbox[i].x += LCD_X + PLATWIDTH;
        }
    }

    if(key == 'p')
    {
        game_pause = !game_pause;
    }
    key = -1;
    clear_screen();
    draw();
}

int main(void)
{
    setup_teensy();
    clear_screen();
    start_screen();
    show_screen();

    while(!game_over)
    {        
        if(usb_configured())
        {
            if(usb_serial_available())
            {
                key = usb_serial_getchar();
            }
        }

        if(leftb_pressed() || key == 's')
        {
            setup();
            game_start = true;
            time = 0;
            game_time_counter = 0;
        }

        while(game_start)
        {
            process();
            _delay_ms(DELAYMS);

            if(joycentre_pressed() || key == 'p')
            {
                game_pause = !game_pause;
            }

            while(game_pause)
            {
                stats_screen();
                if(usb_configured())
                {
                    if(usb_serial_available())
                    {
                        key = usb_serial_getchar();
                    }
                }

                if(joycentre_pressed() || key == 'p')
                {
                    game_pause = !game_pause;
                }

                key = -1;
            }
        }
    }
}