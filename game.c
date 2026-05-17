#include "game.h"
#include "lvgl.h"
#include "gd32h7xx.h" 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ff.h" 
#include "gd32h7xx_adc.h"

#define SCREEN_W 1024
#define SCREEN_H 600
#define MAX_BULLETS 20

// ????????
extern void delay_us(uint32_t us); 

// --- ??????? (????????) ---
#define JS_ADC_PERIPH          ADC2
#define JS_ADC_CLK             RCU_ADC2
#define JS_X_GPIO_PORT         GPIOC
#define JS_X_GPIO_CLK          RCU_GPIOC
#define JS_X_GPIO_PIN          GPIO_PIN_3
#define JS_X_ADC_CHANNEL       ADC_CHANNEL_1
#define JS_Y_GPIO_PORT         GPIOC
#define JS_Y_GPIO_CLK          RCU_GPIOC
#define JS_Y_GPIO_PIN          GPIO_PIN_2
#define JS_Y_ADC_CHANNEL       ADC_CHANNEL_0
#define JS_S_GPIO_PORT         GPIOB          // ??????
#define JS_S_GPIO_CLK          RCU_GPIOB
#define JS_S_GPIO_PIN          GPIO_PIN_2

// --- ?????? ---
#define JS_ADC_LOW_THRESHOLD   1100U
#define JS_ADC_HIGH_THRESHOLD  3000U

int player_hp = 5;
int player_x;
int player_y;

// --- ??UI?? ---
static lv_obj_t *s_root;
static lv_obj_t *s_bg_img;
static lv_obj_t *s_dark_mask;
static lv_obj_t *s_title;
static lv_obj_t *s_hint;
static lv_obj_t *s_start_btn;
static lv_obj_t *s_start_btn_lbl;
static lv_obj_t *s_status_lbl;

static lv_obj_t *g_game_screen;
static lv_obj_t *background_img;
static lv_obj_t *player_plane;
static lv_obj_t *s_hp_label;
static lv_obj_t *s_hp_bar;
static lv_obj_t *s_power_bar;
static lv_obj_t *s_power_label;
static lv_obj_t *s_power_value_label;

static lv_obj_t *s_game_over_label;
static lv_obj_t *s_try_again_btn;
static lv_obj_t *s_game_over_container;
static lv_obj_t *s_score_label;

// --- Boss ???UI??? ---
static lv_obj_t *s_boss_hp_bar;
static lv_obj_t *s_boss_hp_label;
static lv_obj_t *s_boss_warning_container;
static lv_obj_t *s_boss_warning_label;
static boss_t boss;
static bool boss_spawned = false;
static bool boss_incoming = false;

// --- ???? ---
static lv_obj_t *s_laser_beam;
static int laser_active_ticks = 0;  

// --- ???? ---
static int power = 0;
static int power_max = 30;
static int score = 0;
static int high_score = 0; 
static int difficulty_level = 0;
static int total_kills = 0;

static bool player_can_move = false; // ????????????
static bool s_s3_last_state = true;

// ??? (??????)
typedef struct {
    lv_obj_t *obj;
    bool active;
    int x; 
    int y; 
} bullet_t;

static bullet_t bullets[MAX_BULLETS];
static enemy_t enemies[MAX_ENEMIES];
static enemy_bullet_t enemy_bullets[MAX_ENEMY_BULLETS];
static lv_timer_t *game_timer;
static int spawn_counter = 0;
static bool game_over = false;

// ???? (????? touch_event_cb)
static void start_btn_event_cb(lv_event_t *e);
static void try_again_event_cb(lv_event_t *e);
static void update_player_position(void);
static void game_timer_cb(lv_timer_t * timer);
static void spawn_enemy(void);
static void update_enemies(void);
static void update_enemy_bullets(void);
static void check_collisions(void);
static void game_over_screen(void);
void game_shoot(void);
void enemy_shoot(enemy_t *enemy);
void boss_shoot(void);
void init_game_entities(void);

/* ==========================================
 * SD? ???????
 * ========================================== */
static void load_high_score(void) {
    FIL file;
    UINT br;
    char buf[16] = {0};
    
    if (f_open(&file, "0:/highscore.txt", FA_READ) == FR_OK) {
        f_read(&file, buf, sizeof(buf) - 1, &br);
        if (br > 0) {
            high_score = atoi(buf); 
        }
        f_close(&file);
    } else {
        high_score = 0; 
    }
}

static void save_high_score(void) {
    FIL file;
    UINT bw;
    char buf[16];
    
    snprintf(buf, sizeof(buf), "%d", high_score);
    if (f_open(&file, "0:/highscore.txt", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        f_write(&file, buf, strlen(buf), &bw);
        f_close(&file);
    }
}

/* ==========================================
 * ??:ADC ??????????
 * ========================================== */
static void joystick_adc_init(void) {
    // ?? GPIOC (??XY) ? GPIOB (????) ??
    rcu_periph_clock_enable(JS_X_GPIO_CLK);
    rcu_periph_clock_enable(JS_Y_GPIO_CLK);
    rcu_periph_clock_enable(JS_S_GPIO_CLK);
    rcu_periph_clock_enable(JS_ADC_CLK);
    
    // ?? X, Y ?????
    gpio_mode_set(JS_X_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, JS_X_GPIO_PIN);
    gpio_mode_set(JS_Y_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, JS_Y_GPIO_PIN);
    
    // ????????????? (?????:active-high when pressed)
    gpio_mode_set(JS_S_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, JS_S_GPIO_PIN);
    
    // ADC ??
    adc_deinit(JS_ADC_PERIPH);
    adc_clock_config(JS_ADC_PERIPH, ADC_CLK_SYNC_HCLK_DIV6);
    adc_special_function_config(JS_ADC_PERIPH, ADC_CONTINUOUS_MODE, DISABLE);
    adc_special_function_config(JS_ADC_PERIPH, ADC_SCAN_MODE, DISABLE);
    adc_resolution_config(JS_ADC_PERIPH, ADC_RESOLUTION_12B);
    adc_data_alignment_config(JS_ADC_PERIPH, ADC_DATAALIGN_RIGHT);
    adc_end_of_conversion_config(JS_ADC_PERIPH, ADC_EOC_SET_CONVERSION);
    adc_channel_length_config(JS_ADC_PERIPH, ADC_REGULAR_CHANNEL, 1U);
    adc_external_trigger_config(JS_ADC_PERIPH, ADC_REGULAR_CHANNEL, EXTERNAL_TRIGGER_DISABLE);
    adc_enable(JS_ADC_PERIPH);
    
    // ??ADC??
    delay_us(1000U);
    
    adc_calibration_mode_config(JS_ADC_PERIPH, ADC_CALIBRATION_OFFSET);
    adc_calibration_number(JS_ADC_PERIPH, ADC_CALIBRATION_NUM1);
    adc_calibration_enable(JS_ADC_PERIPH);
}

static uint16_t joystick_read_channel(uint8_t channel) {
    // ????????? GD32 ???? ADC_SAMPLETIME_480,?????? 480U
    adc_regular_channel_config(JS_ADC_PERIPH, 0U, channel, 638U);  // ??GD32H7xx?ADC2
    adc_flag_clear(JS_ADC_PERIPH, ADC_FLAG_EOC);
    adc_software_trigger_enable(JS_ADC_PERIPH, ADC_REGULAR_CHANNEL);
    
    // ??????,???????????
    uint32_t timeout = 800000U; 
    while((adc_flag_get(JS_ADC_PERIPH, ADC_FLAG_EOC) == RESET) && (timeout > 0U)) {
        timeout--;
    }
    
    if(timeout == 0U) return 2048U; // ???????,??????
    return (uint16_t)(adc_regular_data_read(JS_ADC_PERIPH) & 0x0FFFU);
}

/* ==========================================
 * ?? UI ??
 * ========================================== */
static void style_title(lv_obj_t *obj)
{
    lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(obj, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_letter_space(obj, 4, 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(obj, 8, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_70, 0);
}

static void style_start_btn(lv_obj_t *btn, lv_obj_t *lbl)
{
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF6A00), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 3, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(btn, 24, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0xFF6A00), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_70, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_letter_space(lbl, 2, 0);
}

// ????
static bool check_collision(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) 
{
    return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

/* ==========================================
 * ???????
 * ========================================== */
static void game_timer_cb(lv_timer_t * timer)
{
    (void)timer;
    if(game_over) return;

    // 1. ?????? (????)
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i].active) {
            bullets[i].y -= 15;
            if(bullets[i].y < -20) {
                bullets[i].active = false;
                lv_obj_add_flag(bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_set_pos(bullets[i].obj, bullets[i].x, bullets[i].y);
            }
        }
    }

    // 2. ????
    update_enemies();
    update_enemy_bullets();
    check_collisions();

    // 3. ???????????
    if (player_can_move && !boss_incoming) {
        uint16_t js_x = joystick_read_channel(JS_X_ADC_CHANNEL);
        uint16_t js_y = joystick_read_channel(JS_Y_ADC_CHANNEL);
        
        int dx = 0;
        int dy = 0;
        
        // ??????:?????????(24 ??/?)
        if(js_x < JS_ADC_LOW_THRESHOLD) dx = -24;
        else if(js_x > JS_ADC_HIGH_THRESHOLD) dx = 24;
        
        if(js_y < JS_ADC_LOW_THRESHOLD) dy = -24; // ??????,??? 24 ? -24 ??
        else if(js_y > JS_ADC_HIGH_THRESHOLD) dy = 24;
        
        if(dx != 0 || dy != 0) {
            player_x += dx;
            player_y += dy;

            // ?????
            if(player_x < 50) player_x = 50;
            if(player_x > SCREEN_W - 50) player_x = SCREEN_W - 50;
            if(player_y < 44) player_y = 44;
            if(player_y > SCREEN_H - 44) player_y = SCREEN_H - 44;

            update_player_position();
        }
    }

    // 4. ????????: ?? S2??(???) ?? ????(???)
    static uint8_t fire_cd = 0;
    bool s2_state = (gpio_input_bit_get(GPIOC, GPIO_PIN_13) == RESET);
    bool js_btn_state = (gpio_input_bit_get(JS_S_GPIO_PORT, JS_S_GPIO_PIN) == SET); 
    
    if(!boss_incoming) {
        if(s2_state || js_btn_state) {
            if(fire_cd == 0) {
                game_shoot();
                fire_cd = 6;
            } else {
                fire_cd--;
            }
        } else {
            fire_cd = 0;
        }
    }

    // 5. ???????? (S3 / PD3)
    bool button_s3_state = (gpio_input_bit_get(GPIOD, GPIO_PIN_3) == RESET);
    if(!boss_incoming) {
        if(button_s3_state && s_s3_last_state == true && power >= power_max) {
            power = 0; 
            if(s_power_bar != NULL) {
                lv_bar_set_value(s_power_bar, 0, LV_ANIM_OFF);
                lv_label_set_text_fmt(s_power_value_label, "%d / %d", power, power_max);
            }
            laser_active_ticks = 16; 
        }
    }
    s_s3_last_state = !button_s3_state;

    // 6. ???????????????
    if(laser_active_ticks > 0) {
        laser_active_ticks--;
        
        int laser_w = 80; 
        int laser_h = player_y - 44; 
        if(laser_h < 0) laser_h = 0;
        
        lv_obj_set_size(s_laser_beam, laser_w, laser_h);
        lv_obj_set_pos(s_laser_beam, player_x - laser_w / 2, 0);
        lv_obj_clear_flag(s_laser_beam, LV_OBJ_FLAG_HIDDEN);

        int laser_cx = player_x;
        int laser_cy = laser_h / 2;
        
        for(int i = 0; i < MAX_ENEMIES; i++) {
            if(enemies[i].active) {
                if(abs(enemies[i].x - laser_cx) < (100 + laser_w) / 2 && 
                   abs(enemies[i].y - laser_cy) < (72 + laser_h) / 2) {
                    enemies[i].active = false;
                    lv_obj_add_flag(enemies[i].obj, LV_OBJ_FLAG_HIDDEN);
                    score += 1;
                    total_kills++;
                    difficulty_level = score / 20;
                    if(s_score_label) lv_label_set_text_fmt(s_score_label, "SCORE: %d", score);
                }
            }
        }
        
        if(boss.active && !boss_incoming) {
            if(abs(boss.x - laser_cx) < (250 + laser_w) / 2 && 
               abs(boss.y - laser_cy) < (138 + laser_h) / 2) {
                if (laser_active_ticks % 3 == 0) { 
                    boss.hp -= 1;
                    lv_bar_set_value(s_boss_hp_bar, boss.hp, LV_ANIM_ON); 
                    if(boss.hp <= 0) { 
                        boss.active = false;
                        lv_obj_add_flag(boss.obj, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_add_flag(s_boss_hp_bar, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_add_flag(s_boss_hp_label, LV_OBJ_FLAG_HIDDEN);
                        score += 20; 
                        difficulty_level = score / 20;
                        if(s_score_label) lv_label_set_text_fmt(s_score_label, "SCORE: %d", score);
                        player_hp = 5;
                        game_update_health(player_hp);
                        power += power_max / 2;
                        if(power > power_max) power = power_max;
                        if(s_power_bar != NULL) {
                            lv_bar_set_value(s_power_bar, power, LV_ANIM_OFF);
                            lv_label_set_text_fmt(s_power_value_label, "%d / %d", power, power_max);
                        }
                    }
                }
            }
        }
    } else {
        if(s_laser_beam != NULL && !lv_obj_has_flag(s_laser_beam, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(s_laser_beam, LV_OBJ_FLAG_HIDDEN); 
        }
    }

    // 7. Boss ????
    if(total_kills >= 25 && !boss_spawned && !boss.active) {
        for(int i = 0; i < MAX_ENEMIES; i++) {
            enemies[i].active = false;
            if(enemies[i].obj != NULL) {
                lv_obj_add_flag(enemies[i].obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
        for(int i = 0; i < MAX_ENEMY_BULLETS; i++) {
            enemy_bullets[i].active = false;
            lv_obj_add_flag(enemy_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
        }
        for(int i = 0; i < MAX_BULLETS; i++) {
            bullets[i].active = false;
            lv_obj_add_flag(bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
        }

        boss_spawned = true;
        boss.active = true;
        boss.hp = 20; 
        boss.x = SCREEN_W / 2;
        boss.y = -120;
        boss.dir_x = 4;
        boss.shoot_counter = 0;
        lv_obj_clear_flag(boss.obj, LV_OBJ_FLAG_HIDDEN);
        
        lv_obj_clear_flag(s_boss_hp_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_boss_hp_label, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(s_boss_hp_bar, boss.hp, LV_ANIM_ON);

        boss_incoming = true;
        player_can_move = false; 
        if(s_boss_warning_container != NULL) {
            lv_obj_clear_flag(s_boss_warning_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_boss_warning_container);
        }
    }

    // 8. Boss ???????
    if(boss.active) {
        if(boss_incoming) {
            boss.y += 2; 
            if(boss.y >= 80) { 
                boss_incoming = false;
                player_can_move = true; 
                if(s_boss_warning_container != NULL) {
                    lv_obj_add_flag(s_boss_warning_container, LV_OBJ_FLAG_HIDDEN);
                }
            }
        } else {
            boss.x += boss.dir_x; 
            if(boss.x < 125 || boss.x > SCREEN_W - 125) {
                boss.dir_x = -boss.dir_x;
            }
            boss.shoot_counter++;
            if(boss.shoot_counter >= 15) { 
                boss_shoot();
                boss.shoot_counter = 0;
            }
        }
        lv_obj_set_pos(boss.obj, boss.x - 125, boss.y - 69); 
    }

    // 9. ????
    if (!boss.active && !boss_incoming) {
        spawn_counter++;
        int spawn_threshold = 60 - (difficulty_level * 3);
        if(spawn_threshold < 25) spawn_threshold = 25;
        if(spawn_counter >= spawn_threshold) {
            spawn_counter = 0;
            int enemies_to_spawn = 2 + (difficulty_level / 4);
            if(enemies_to_spawn > 4) enemies_to_spawn = 4;

            for(int j = 0; j < enemies_to_spawn; j++) {
                spawn_enemy();
            }
        }
    }
}

// ????
void enemy_shoot(enemy_t *enemy)
{
    if(enemy->y < -72 || enemy->y > SCREEN_H + 72) return;

    for(int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if(!enemy_bullets[i].active) {
            enemy_bullets[i].active = true;
            enemy_bullets[i].type = 0; 
            
            enemy_bullets[i].x = enemy->x - 3;
            enemy_bullets[i].y = enemy->y + 30;

            if(enemy_bullets[i].x < 0) enemy_bullets[i].x = 0;
            if(enemy_bullets[i].x > SCREEN_W - 6) enemy_bullets[i].x = SCREEN_W - 6;
            if(enemy_bullets[i].y < 0) enemy_bullets[i].y = 0;

            lv_obj_set_pos(enemy_bullets[i].obj, enemy_bullets[i].x, enemy_bullets[i].y);
            lv_obj_clear_flag(enemy_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            break;
        }
    }
}

// Boss ??
void boss_shoot(void)
{
    int spawned = 0;
    for(int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if(!enemy_bullets[i].active) {
            enemy_bullets[i].active = true;
            enemy_bullets[i].type = 1; 
            enemy_bullets[i].angle = 0;
            enemy_bullets[i].y = boss.y + 50; 
            
            if(spawned == 0) {
                enemy_bullets[i].base_x = boss.x - 40; 
            } else {
                enemy_bullets[i].base_x = boss.x + 40; 
            }
            
            enemy_bullets[i].x = enemy_bullets[i].base_x;
            lv_obj_set_pos(enemy_bullets[i].obj, enemy_bullets[i].x, enemy_bullets[i].y);
            lv_obj_clear_flag(enemy_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);

            spawned++;
            if (spawned >= 2) break;
        }
    }
}

void game_shoot(void)
{
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].x = player_x - 3;
            bullets[i].y = player_y - 44 - 20;

            lv_obj_set_pos(bullets[i].obj, bullets[i].x, bullets[i].y);
            lv_obj_clear_flag(bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            break;
        }
    }
}

void init_game_entities(void)
{
    if(game_timer != NULL) return;

    for(int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].obj = lv_obj_create(g_game_screen);
        lv_obj_set_size(bullets[i].obj, 6, 20);
        lv_obj_set_style_bg_color(bullets[i].obj, lv_color_hex(0xFFDD33), 0);
        lv_obj_set_style_radius(bullets[i].obj, 3, 0);
        lv_obj_set_style_border_width(bullets[i].obj, 0, 0);
        lv_obj_clear_flag(bullets[i].obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
        bullets[i].active = false;
    }

    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = false;
        enemies[i].obj = NULL;
    }

    for(int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        enemy_bullets[i].obj = lv_obj_create(g_game_screen);
        lv_obj_set_size(enemy_bullets[i].obj, 8, 20); 
        lv_obj_set_style_bg_color(enemy_bullets[i].obj, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_radius(enemy_bullets[i].obj, 3, 0);
        lv_obj_set_style_border_width(enemy_bullets[i].obj, 0, 0);
        lv_obj_clear_flag(enemy_bullets[i].obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(enemy_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
        enemy_bullets[i].active = false;
    }

    boss.obj = lv_img_create(g_game_screen);
    lv_img_set_src(boss.obj, "0:/boss.bin");
    lv_obj_set_size(boss.obj, 250, 138);
    lv_obj_clear_flag(boss.obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(boss.obj, LV_OBJ_FLAG_HIDDEN);
    boss.active = false;

    if(s_boss_hp_bar == NULL) {
        s_boss_hp_bar = lv_bar_create(g_game_screen);
        lv_obj_set_size(s_boss_hp_bar, 400, 20); 
        lv_obj_align(s_boss_hp_bar, LV_ALIGN_TOP_MID, 0, 40);
        lv_bar_set_range(s_boss_hp_bar, 0, 20);
        lv_bar_set_value(s_boss_hp_bar, 20, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_boss_hp_bar, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_boss_hp_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
        lv_obj_add_flag(s_boss_hp_bar, LV_OBJ_FLAG_HIDDEN);

        s_boss_hp_label = lv_label_create(g_game_screen);
        lv_label_set_text(s_boss_hp_label, "BOSS");
        lv_obj_set_style_text_color(s_boss_hp_label, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_text_font(s_boss_hp_label, LV_FONT_DEFAULT, 0);
        lv_obj_align_to(s_boss_hp_label, s_boss_hp_bar, LV_ALIGN_OUT_TOP_MID, 0, -5);
        lv_obj_add_flag(s_boss_hp_label, LV_OBJ_FLAG_HIDDEN);
    }

    if(s_boss_warning_container == NULL) {
        s_boss_warning_container = lv_obj_create(g_game_screen);
        lv_obj_set_size(s_boss_warning_container, SCREEN_W, 100);
        lv_obj_center(s_boss_warning_container);
        lv_obj_set_style_bg_color(s_boss_warning_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_boss_warning_container, LV_OPA_80, 0);
        lv_obj_set_style_border_width(s_boss_warning_container, 0, 0);
        lv_obj_clear_flag(s_boss_warning_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_boss_warning_container, LV_OBJ_FLAG_HIDDEN);

        s_boss_warning_label = lv_label_create(s_boss_warning_container);
        lv_label_set_text(s_boss_warning_label, "The boss is coming !");
        lv_obj_set_style_text_color(s_boss_warning_label, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_text_font(s_boss_warning_label, LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_letter_space(s_boss_warning_label, 4, 0);
        lv_obj_center(s_boss_warning_label);
    }

    if(s_laser_beam == NULL) {
        s_laser_beam = lv_obj_create(g_game_screen);
        lv_obj_set_style_bg_color(s_laser_beam, lv_color_hex(0x00FFFF), 0); 
        lv_obj_set_style_bg_opa(s_laser_beam, LV_OPA_80, 0); 
        lv_obj_set_style_border_width(s_laser_beam, 0, 0);
        lv_obj_set_style_shadow_color(s_laser_beam, lv_color_hex(0x00FFFF), 0);
        lv_obj_set_style_shadow_width(s_laser_beam, 30, 0); 
        lv_obj_clear_flag(s_laser_beam, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_laser_beam, LV_OBJ_FLAG_HIDDEN);
    }

    game_timer = lv_timer_create(game_timer_cb, 30, NULL);
}

static bool check_enemy_overlap(int x, int y)
{
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].active) {
            int dx = abs(x - enemies[i].x);
            int dy = abs(y - enemies[i].y);
            if(dx < 110 && dy < 82) return true;
        }
    }
    return false;
}

static void spawn_enemy(void)
{
    int active_count = 0;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].active) active_count++;
    }

    int max_enemies_now = MAX_ENEMIES;
    if(active_count >= max_enemies_now) return;

    int base_hp = 1 + (difficulty_level / 3);

    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(!enemies[i].active) {
            int new_x, new_y;
            int attempts = 0;
            
            do {
                new_x = (rand() % (SCREEN_W - 100)) + 50;
                new_y = -50 - (rand() % 100);
                attempts++;
            } while(attempts < 100 && check_enemy_overlap(new_x, new_y));

            enemies[i].active = true;
            enemies[i].x = new_x;
            enemies[i].y = new_y;
            enemies[i].hp = base_hp;

            enemies[i].move_mode = rand() % 10;
            if(enemies[i].move_mode < 3) {
                enemies[i].dir_x = 0;
                enemies[i].dir_y = (rand() % 2) + 1;
            } else if(enemies[i].move_mode < 8) {
                enemies[i].dir_x = (rand() % 3) - 1;
                enemies[i].dir_y = (rand() % 2) + 1;
            } else {
                enemies[i].dir_x = 0;
                enemies[i].dir_y = 0;
            }

            enemies[i].move_counter = 0;
            enemies[i].shoot_counter = 0;
            enemies[i].mode_timer = 0;

            if(enemies[i].obj == NULL) {
                enemies[i].obj = lv_img_create(g_game_screen);
                lv_img_set_src(enemies[i].obj, "0:/enemy.bin");
                lv_obj_set_size(enemies[i].obj, 100, 72);
                lv_obj_clear_flag(enemies[i].obj, LV_OBJ_FLAG_CLICKABLE);
            }

            lv_obj_set_pos(enemies[i].obj, enemies[i].x - 50, enemies[i].y - 36);
            lv_obj_clear_flag(enemies[i].obj, LV_OBJ_FLAG_HIDDEN);
            break;
        }
    }
}

static void update_enemies(void)
{
    int bottom_boundary = SCREEN_H * 3 / 4 - 36;

    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(!enemies[i].active) continue;

        enemies[i].move_counter++;
        enemies[i].shoot_counter++;
        enemies[i].mode_timer++;

        if(enemies[i].move_counter >= 3) {
            enemies[i].move_counter = 0;

            int speed_x = 8 + (difficulty_level * 2);
            int speed_y = 6 + (difficulty_level * 2);
            enemies[i].x += enemies[i].dir_x * speed_x;
            enemies[i].y += enemies[i].dir_y * speed_y;

            if(enemies[i].x < 50) {
                enemies[i].x = 50;
                enemies[i].dir_x = (rand() % 2) + 1;
            }
            if(enemies[i].x > SCREEN_W - 50) {
                enemies[i].x = SCREEN_W - 50;
                enemies[i].dir_x = (rand() % 2) - 2;
            }
            if(enemies[i].y > bottom_boundary) {
                enemies[i].y = bottom_boundary;
                enemies[i].dir_y = -(rand() % 2) - 1;
            }
            if(enemies[i].y < -36) {
                enemies[i].dir_y = (rand() % 2) + 1;
            }

            lv_obj_set_pos(enemies[i].obj, enemies[i].x - 50, enemies[i].y - 36);
        }

        if(enemies[i].mode_timer >= 150) {
            enemies[i].mode_timer = 0;
            enemies[i].move_mode = rand() % 10;
            if(enemies[i].move_mode < 3) {
                enemies[i].dir_x = 0;
                if(enemies[i].dir_y == 0) enemies[i].dir_y = (rand() % 2) + 1;
            } else if(enemies[i].move_mode < 8) {
                enemies[i].dir_x = (rand() % 3) - 1;
                enemies[i].dir_y = (rand() % 2) + 1;
            } else {
                enemies[i].dir_x = 0;
                enemies[i].dir_y = 0;
            }
        }

        int shoot_threshold = 50 - (difficulty_level * 5);
        if(shoot_threshold < 10) shoot_threshold = 10;
        if(enemies[i].shoot_counter >= shoot_threshold) {
            enemies[i].shoot_counter = 0;
            enemy_shoot(&enemies[i]);
        }
    }
}

static void update_enemy_bullets(void)
{
    for(int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if(enemy_bullets[i].active) {
            if (enemy_bullets[i].type == 0) {
                enemy_bullets[i].y += 8; 
            } else {
                enemy_bullets[i].angle = (enemy_bullets[i].angle + 15) % 360; 
                enemy_bullets[i].y += 7; 
                enemy_bullets[i].x = enemy_bullets[i].base_x + ((lv_trigo_sin(enemy_bullets[i].angle) * 80) >> 15);
            }

            if(enemy_bullets[i].y > SCREEN_H + 20) {
                enemy_bullets[i].active = false;
                lv_obj_add_flag(enemy_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_set_pos(enemy_bullets[i].obj, enemy_bullets[i].x, enemy_bullets[i].y);
            }
        }
    }
}

static void check_collisions(void)
{
    int px = player_x;
    int py = player_y;
    int player_w = 100;
    int player_h = 88;

    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(!enemies[i].active) continue;

        int ex = enemies[i].x;
        int ey = enemies[i].y;
        int ew = 100;
        int eh = 72;

        if(abs(px - ex) < (player_w + ew) / 2 && abs(py - ey) < (player_h + eh) / 2) {
            enemies[i].active = false;
            lv_obj_add_flag(enemies[i].obj, LV_OBJ_FLAG_HIDDEN);

            player_hp--;
            if(player_hp < 0) player_hp = 0;
            game_update_health(player_hp);

            if(player_hp <= 0) {
                game_over = true;
                game_over_screen();
                return;
            }
            continue;
        }

        for(int j = 0; j < MAX_BULLETS; j++) {
            if(!bullets[j].active) continue;

            int bx = bullets[j].x + 3;
            int by = bullets[j].y + 10;

            if(abs(bx - ex) < ew / 2 && abs(by - ey) < eh / 2) {
                bullets[j].active = false;
                lv_obj_add_flag(bullets[j].obj, LV_OBJ_FLAG_HIDDEN);

                enemies[i].hp--;
                if(enemies[i].hp <= 0) {
                    enemies[i].active = false;
                    lv_obj_add_flag(enemies[i].obj, LV_OBJ_FLAG_HIDDEN);

                    score += 1; 
                    total_kills++;
                    difficulty_level = score / 20;
                    if(s_score_label != NULL) {
                        lv_label_set_text_fmt(s_score_label, "SCORE: %d", score);
                    }

                    power += (rand() % 2) + 1;
                    if(power > power_max) power = power_max;
                    if(s_power_bar != NULL) {
                        lv_bar_set_value(s_power_bar, power, LV_ANIM_OFF);
                        lv_label_set_text_fmt(s_power_value_label, "%d / %d", power, power_max);
                    }
                }
                break;
            }
        }
    }

    if(boss.active && !boss_incoming) {
        int bw = 250;
        int bh = 138;
        
        if(abs(px - boss.x) < (player_w + bw) / 2 && abs(py - boss.y) < (player_h + bh) / 2) {
            player_hp--;
            if(player_hp < 0) player_hp = 0;
            game_update_health(player_hp);
            if(player_hp <= 0) {
                game_over = true;
                game_over_screen();
                return;
            }
        }
        
        for(int j = 0; j < MAX_BULLETS; j++) {
            if(!bullets[j].active) continue;
            int bx = bullets[j].x + 3;
            int by = bullets[j].y + 10;
            
            if(abs(bx - boss.x) < bw / 2 && abs(by - boss.y) < bh / 2) {
                bullets[j].active = false;
                lv_obj_add_flag(bullets[j].obj, LV_OBJ_FLAG_HIDDEN);
                
                boss.hp--;
                lv_bar_set_value(s_boss_hp_bar, boss.hp, LV_ANIM_ON); 
                
                if(boss.hp <= 0) {
                    boss.active = false;
                    lv_obj_add_flag(boss.obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(s_boss_hp_bar, LV_OBJ_FLAG_HIDDEN); 
                    lv_obj_add_flag(s_boss_hp_label, LV_OBJ_FLAG_HIDDEN);
                    
                    score += 20; 
                    difficulty_level = score / 20;
                    if(s_score_label != NULL) {
                        lv_label_set_text_fmt(s_score_label, "SCORE: %d", score);
                    }
                    player_hp = 5;
                    game_update_health(player_hp);
                    power += power_max / 2;
                    if(power > power_max) power = power_max;
                    if(s_power_bar != NULL) {
                        lv_bar_set_value(s_power_bar, power, LV_ANIM_OFF);
                        lv_label_set_text_fmt(s_power_value_label, "%d / %d", power, power_max);
                    }
                }
            }
        }
    }

    for(int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if(!enemy_bullets[i].active) continue;

        int bx = enemy_bullets[i].x + 3;
        int by = enemy_bullets[i].y + 10;

        if(abs(bx - px) < player_w / 2 && abs(by - py) < player_h / 2) {
            enemy_bullets[i].active = false;
            lv_obj_add_flag(enemy_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);

            player_hp--;
            if(player_hp < 0) player_hp = 0;
            game_update_health(player_hp);

            if(player_hp <= 0) {
                game_over = true;
                game_over_screen();
                return;
            }
        }
    }
}

static void game_over_screen(void)
{
    bool is_new_best = false;
    if(score > high_score) {
        high_score = score;
        save_high_score(); 
        is_new_best = true;
    }

    s_game_over_container = lv_obj_create(g_game_screen);
    lv_obj_set_size(s_game_over_container, 300, 240);
    lv_obj_align(s_game_over_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_game_over_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_game_over_container, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_game_over_container, 0, 0);
    lv_obj_center(s_game_over_container);

    s_game_over_label = lv_label_create(s_game_over_container);
    lv_label_set_text(s_game_over_label, "GAME OVER");
    lv_obj_set_style_text_color(s_game_over_label, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_text_font(s_game_over_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(s_game_over_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(s_game_over_label, 8, 0);
    lv_obj_set_style_text_opa(s_game_over_label, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(s_game_over_label, 10, 0);
    lv_obj_set_style_shadow_color(s_game_over_label, lv_color_hex(0x000000), 0);
    lv_obj_align(s_game_over_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *final_score_lbl = lv_label_create(s_game_over_container);
    if(is_new_best) {
        lv_label_set_text_fmt(final_score_lbl, "NEW RECORD!\nSCORE: %d\nBEST: %d", score, high_score);
    } else {
        lv_label_set_text_fmt(final_score_lbl, "SCORE: %d\nBEST: %d", score, high_score);
    }
    lv_obj_set_style_text_color(final_score_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(final_score_lbl, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(final_score_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(final_score_lbl, LV_ALIGN_CENTER, 0, -10);

    s_try_again_btn = lv_btn_create(s_game_over_container);
    lv_obj_set_size(s_try_again_btn, 200, 60);
    lv_obj_align(s_try_again_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_radius(s_try_again_btn, 14, 0);
    lv_obj_set_style_bg_color(s_try_again_btn, lv_color_hex(0xFF6A00), 0);
    lv_obj_set_style_bg_opa(s_try_again_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_try_again_btn, 3, 0);
    lv_obj_set_style_border_color(s_try_again_btn, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *try_again_lbl = lv_label_create(s_try_again_btn);
    lv_label_set_text(try_again_lbl, "TRY AGAIN");
    lv_obj_set_style_text_color(try_again_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(try_again_lbl, LV_FONT_DEFAULT, 0);
    lv_obj_center(try_again_lbl);

    lv_obj_add_event_cb(s_try_again_btn, try_again_event_cb, LV_EVENT_CLICKED, NULL);
}

static void try_again_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        player_can_move = false;
        game_over = false;

        if(game_timer != NULL) {
            lv_timer_del(game_timer);
            game_timer = NULL;
        }

        if(s_game_over_container != NULL) {
            lv_obj_del(s_game_over_container);
            s_game_over_container = NULL;
            s_game_over_label = NULL;
            s_try_again_btn = NULL;
        }

        if(g_game_screen != NULL) {
            lv_obj_del(g_game_screen);
            g_game_screen = NULL;
        }

        player_plane = NULL;
        s_hp_label = NULL;
        s_score_label = NULL;
        s_power_bar = NULL;
        s_power_label = NULL;
        s_power_value_label = NULL;
        
        s_laser_beam = NULL;
        s_boss_hp_bar = NULL;
        s_boss_hp_label = NULL;
        s_boss_warning_container = NULL;
        s_boss_warning_label = NULL;
        
        score = 0;
        power = 0;
        difficulty_level = 0;
        player_hp = 5;
        total_kills = 0;
        
        boss_spawned = false;
        boss_incoming = false;
        boss.active = false;

        for(int i = 0; i < MAX_ENEMIES; i++) {
            enemies[i].active = false;
            enemies[i].obj = NULL;
        }

        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_player_position(void)
{
    if(player_plane != NULL) {
        lv_obj_set_pos(player_plane, player_x - 50, player_y - 44);
    }
}

void game_update_health(int32_t health)
{
    if(health < 0) health = 0;
    if(health > 5) health = 5;
    if(s_hp_bar != NULL) {
        lv_bar_set_value(s_hp_bar, health, LV_ANIM_ON);
    }
    if(s_hp_label != NULL) {
        lv_label_set_text_fmt(s_hp_label, "%d / 5", (int)health);
    }
}

void show_game_screen(void)
{
    player_hp = 5;
    game_over = false;
    spawn_counter = 0;
    score = 0;
    power = 0;
    difficulty_level = 0;
    total_kills = 0;
    
    boss_spawned = false;
    boss_incoming = false;
    boss.active = false;
    laser_active_ticks = 0;

    if(g_game_screen != NULL) {
        lv_obj_clear_flag(g_game_screen, LV_OBJ_FLAG_HIDDEN);
        game_update_health(player_hp);
        return;
    }

    g_game_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_game_screen, SCREEN_W, SCREEN_H);
    lv_obj_align(g_game_screen, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // ????????????????
    lv_obj_clear_flag(g_game_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g_game_screen, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_bg_opa(g_game_screen, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_game_screen, 0, 0);

    background_img = lv_img_create(g_game_screen);
    lv_img_set_src(background_img, "0:/mainbk.bin");
    lv_obj_align(background_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(background_img, LV_OBJ_FLAG_CLICKABLE);

    s_hp_bar = lv_bar_create(g_game_screen);
    lv_obj_set_size(s_hp_bar, 150, 15);
    lv_obj_align(s_hp_bar, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_bar_set_range(s_hp_bar, 0, 5);
    lv_bar_set_value(s_hp_bar, 5, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_hp_bar, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hp_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);

    lv_obj_t *hp_text_label = lv_label_create(g_game_screen);
    lv_label_set_text(hp_text_label, "HP");
    lv_obj_set_style_text_color(hp_text_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(hp_text_label, s_hp_bar, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

    s_hp_label = lv_label_create(g_game_screen);
    lv_label_set_text(s_hp_label, "5 / 5");
    lv_obj_set_style_text_color(s_hp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(s_hp_label, s_hp_bar, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    s_power_bar = lv_bar_create(g_game_screen);
    lv_obj_set_size(s_power_bar, 150, 15);
    lv_obj_align_to(s_power_bar, s_hp_bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
    lv_bar_set_range(s_power_bar, 0, power_max);
    lv_bar_set_value(s_power_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_power_bar, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_power_bar, lv_color_hex(0x00FF00), LV_PART_INDICATOR);

    s_power_label = lv_label_create(g_game_screen);
    lv_label_set_text(s_power_label, "POWER");
    lv_obj_set_style_text_color(s_power_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_power_label, LV_FONT_DEFAULT, 0);
    lv_obj_align_to(s_power_label, s_power_bar, LV_ALIGN_OUT_TOP_LEFT, 0, -2);

    s_power_value_label = lv_label_create(g_game_screen);
    lv_label_set_text_fmt(s_power_value_label, "%d / %d", power, power_max);
    lv_obj_set_style_text_color(s_power_value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_power_value_label, LV_FONT_DEFAULT, 0);
    lv_obj_align_to(s_power_value_label, s_power_bar, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    s_score_label = lv_label_create(g_game_screen);
    lv_label_set_text_fmt(s_score_label, "SCORE: %d", score);
    lv_obj_set_style_text_color(s_score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_score_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(s_score_label, LV_ALIGN_TOP_RIGHT, -20, 20);

    player_plane = lv_img_create(g_game_screen);
    lv_img_set_src(player_plane, "0:/myself.bin");
    lv_obj_set_size(player_plane, 100, 88);

    player_x = SCREEN_W / 2;
    player_y = SCREEN_H - 60;
    lv_obj_set_pos(player_plane, player_x - 50, player_y - 44);
    lv_obj_set_style_opa(player_plane, LV_OPA_COVER, 0);
    lv_obj_clear_flag(player_plane, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(player_plane);

    init_game_entities();

    player_can_move = true; // ????????
}

void game_init(void)
{
    load_high_score(); // ??????SD???
    
    // ?????????
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_13);

    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_3);
    
    joystick_adc_init();
    
    lv_obj_t *scr = lv_scr_act();

    s_root = lv_obj_create(scr);
    lv_obj_set_size(s_root, SCREEN_W, SCREEN_H);
    lv_obj_align(s_root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x101828), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    s_bg_img = lv_img_create(s_root);
    lv_img_set_src(s_bg_img, "0:/back1.bin");
    lv_obj_align(s_bg_img, LV_ALIGN_CENTER, 0, 0);

    s_dark_mask = lv_obj_create(s_root);
    lv_obj_set_size(s_dark_mask, SCREEN_W, SCREEN_H);
    lv_obj_align(s_dark_mask, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_border_width(s_dark_mask, 0, 0);
    lv_obj_set_style_bg_color(s_dark_mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_dark_mask, LV_OPA_30, 0);
    lv_obj_clear_flag(s_dark_mask, LV_OBJ_FLAG_SCROLLABLE);

    s_title = lv_label_create(s_root);
    lv_label_set_text(s_title, "PLANE WAR");
    style_title(s_title);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 54);

    s_hint = lv_label_create(s_root);
    lv_label_set_text(s_hint, "Use Joystick to move\nPress S2 / JS_Btn to shoot\nPress S3 for Laser");
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_hint, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -130);

    s_start_btn = lv_btn_create(s_root);
    lv_obj_set_size(s_start_btn, 280, 90);
    lv_obj_align(s_start_btn, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_add_event_cb(s_start_btn, start_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_start_btn_lbl = lv_label_create(s_start_btn);
    lv_label_set_text(s_start_btn_lbl, "START");
    style_start_btn(s_start_btn, s_start_btn_lbl);
    lv_obj_center(s_start_btn_lbl);

    s_status_lbl = lv_label_create(s_root);
    lv_label_set_text_fmt(s_status_lbl, "BEST SCORE: %d", high_score);
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(s_status_lbl, LV_FONT_DEFAULT, 0);
    lv_obj_align(s_status_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void start_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        show_game_screen();
    }
}
