#include "game.h"
#include "lvgl.h"

#define SCREEN_W 1024
#define SCREEN_H 600

// Static object pointers for start screen
static lv_obj_t *s_root;
static lv_obj_t *s_bg_img;
static lv_obj_t *s_dark_mask;
static lv_obj_t *s_title;
static lv_obj_t *s_hint;
static lv_obj_t *s_start_btn;
static lv_obj_t *s_start_btn_lbl;
static lv_obj_t *s_status_lbl;

// Static object pointers for game screen
static lv_obj_t *g_game_screen;
static lv_obj_t *background_img;
static lv_obj_t *player_plane;

// UI components for game screen
static lv_obj_t *s_health_bar;
static lv_obj_t *s_ammo_bar;
static lv_obj_t *s_ammo_label;

// Player aircraft position (center point)
static int player_x = SCREEN_W / 2;
static int player_y = SCREEN_H - 60;
static lv_point_t touch_down_point;
static bool touch_enabled = false;

// Forward declarations for event callback functions
static void back_btn_event_handler(lv_event_t *e);
static void start_btn_event_cb(lv_event_t *e);
static void game_touch_event_cb(lv_event_t *e);
static void update_player_position(void);

/* Style functions */
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

/* Update player aircraft display position */
static void update_player_position(void)
{
    if(player_plane != NULL) {
        // Set aircraft top-left corner (aircraft is 100x88)
        lv_obj_set_pos(player_plane, player_x - 50, player_y - 44);
    }
}

/* Game touch event handler for aircraft movement */
static void game_touch_event_cb(lv_event_t *e)
{
    if(!touch_enabled) return;
    
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t point;
    
    switch(code) {
        case LV_EVENT_PRESSED:
            lv_indev_get_point(lv_indev_get_act(), &touch_down_point);
            break;
            
        case LV_EVENT_PRESSING:
            lv_indev_get_point(lv_indev_get_act(), &point);
            
            int dx = point.x - touch_down_point.x;
            int dy = point.y - touch_down_point.y;
            
            player_x += dx;
            player_y += dy;
            
            // Boundary constraints (aircraft size 100x88)
            if(player_x < 50) player_x = 50;
            if(player_x > SCREEN_W - 50) player_x = SCREEN_W - 50;
            if(player_y < 44) player_y = 44;
            if(player_y > SCREEN_H - 44) player_y = SCREEN_H - 44;
            
            update_player_position();
            touch_down_point = point;
            break;
            
        case LV_EVENT_RELEASED:
            break;
    }
}

/* Public function: Update health bar (0-100) */
void game_update_health(int32_t health)
{
    if(s_health_bar != NULL) {
        if(health < 0) health = 0;
        if(health > 100) health = 100;
        lv_bar_set_value(s_health_bar, health, LV_ANIM_ON);
    }
}

/* Public function: Update ammo bar and label (0-30) */
void game_update_ammo(int32_t ammo)
{
    if(s_ammo_bar != NULL && s_ammo_label != NULL) {
        if(ammo < 0) ammo = 0;
        if(ammo > 30) ammo = 30;
        lv_bar_set_value(s_ammo_bar, ammo, LV_ANIM_ON);
        lv_label_set_text_fmt(s_ammo_label, "%d / 30", ammo);
    }
}

/* Show the main game screen */
void show_game_screen(void)
{
    // If already created, just show and return
    if(g_game_screen != NULL) {
        lv_obj_clear_flag(g_game_screen, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Create new game screen
    g_game_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_game_screen, SCREEN_W, SCREEN_H);
    lv_obj_align(g_game_screen, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(g_game_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Make game screen background transparent
    lv_obj_set_style_bg_opa(g_game_screen, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_game_screen, 0, 0);
    
    // Enable touch events on game screen
    lv_obj_add_flag(g_game_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_game_screen, game_touch_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(g_game_screen, game_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_game_screen, game_touch_event_cb, LV_EVENT_RELEASED, NULL);

    // Background image
    background_img = lv_img_create(g_game_screen);
    lv_img_set_src(background_img, "0:/mainbk.bin");
    lv_obj_align(background_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(background_img, LV_OBJ_FLAG_CLICKABLE);

    // ==========================================
    // Health Bar UI (Top Left)
    // ==========================================
    s_health_bar = lv_bar_create(g_game_screen);
    lv_obj_set_size(s_health_bar, 200, 20);
    lv_obj_align(s_health_bar, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_bar_set_range(s_health_bar, 0, 100);
    lv_bar_set_value(s_health_bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_health_bar, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_health_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);

    // Add health label
    lv_obj_t *health_label = lv_label_create(g_game_screen);
    lv_label_set_text(health_label, "HEALTH");
    lv_obj_set_style_text_color(health_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(health_label, LV_FONT_DEFAULT, 0);
    lv_obj_align_to(health_label, s_health_bar, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

    // ==========================================
    // Ammo Bar UI (Below Health Bar)
    // ==========================================
    s_ammo_bar = lv_bar_create(g_game_screen);
    lv_obj_set_size(s_ammo_bar, 150, 15);
    lv_obj_align_to(s_ammo_bar, s_health_bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
    lv_bar_set_range(s_ammo_bar, 0, 30);
    lv_bar_set_value(s_ammo_bar, 30, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ammo_bar, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ammo_bar, lv_color_hex(0xFFD700), LV_PART_INDICATOR);

    // Ammo count label
    s_ammo_label = lv_label_create(g_game_screen);
    lv_label_set_text(s_ammo_label, "30 / 30");
    lv_obj_set_style_text_color(s_ammo_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(s_ammo_label, s_ammo_bar, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    // Add ammo text label
    lv_obj_t *ammo_text_label = lv_label_create(g_game_screen);
    lv_label_set_text(ammo_text_label, "AMMO");
    lv_obj_set_style_text_color(ammo_text_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(ammo_text_label, s_ammo_bar, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

    // ==========================================
    // Create Player Aircraft
    // ==========================================
    player_plane = lv_img_create(g_game_screen);
    lv_img_set_src(player_plane, "0:/myself.bin");
    lv_obj_set_size(player_plane, 100, 88);
    
    // Reset aircraft position
    player_x = SCREEN_W / 2;
    player_y = SCREEN_H - 60;
    lv_obj_set_pos(player_plane, player_x - 50, player_y - 44);
    lv_obj_set_style_opa(player_plane, LV_OPA_COVER, 0);
    lv_obj_clear_flag(player_plane, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(player_plane);

    // ==========================================
    // Back Button
    // ==========================================
    lv_obj_t *back_btn = lv_btn_create(g_game_screen);
    lv_obj_set_size(back_btn, 120, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *back_btn_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_btn_lbl, "BACK");
    lv_obj_set_style_text_color(back_btn_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(back_btn_lbl, LV_FONT_DEFAULT, 0);
    lv_obj_center(back_btn_lbl);

    lv_obj_add_event_cb(back_btn, back_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Enable touch control
    touch_enabled = true;
}

/* Back button event handler */
static void back_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        touch_enabled = false;
        lv_obj_add_flag(g_game_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Start button event callback */
static void start_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        show_game_screen();
    }
}

/* Initialize the start screen */
void game_init(void)
{
    lv_obj_t *scr = lv_scr_act();

    // Root layer
    s_root = lv_obj_create(scr);
    lv_obj_set_size(s_root, SCREEN_W, SCREEN_H);
    lv_obj_align(s_root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x101828), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    // Background image
    s_bg_img = lv_img_create(s_root);
    lv_img_set_src(s_bg_img, "0:/back1.bin");
    lv_obj_align(s_bg_img, LV_ALIGN_CENTER, 0, 0);

    // Black overlay
    s_dark_mask = lv_obj_create(s_root);
    lv_obj_set_size(s_dark_mask, SCREEN_W, SCREEN_H);
    lv_obj_align(s_dark_mask, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_border_width(s_dark_mask, 0, 0);
    lv_obj_set_style_bg_color(s_dark_mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_dark_mask, LV_OPA_30, 0);
    lv_obj_clear_flag(s_dark_mask, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    s_title = lv_label_create(s_root);
    lv_label_set_text(s_title, "PLANE WAR");
    style_title(s_title);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 54);

    // Hint
    s_hint = lv_label_create(s_root);
    lv_label_set_text(s_hint, "Touch START to begin");
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_hint, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -130);

    // Start button
    s_start_btn = lv_btn_create(s_root);
    lv_obj_set_size(s_start_btn, 280, 90);
    lv_obj_align(s_start_btn, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_add_event_cb(s_start_btn, start_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_start_btn, start_btn_event_cb, LV_EVENT_PRESSED, NULL);

    s_start_btn_lbl = lv_label_create(s_start_btn);
    lv_label_set_text(s_start_btn_lbl, "START");
    style_start_btn(s_start_btn, s_start_btn_lbl);
    lv_obj_center(s_start_btn_lbl);

    // Status label
    s_status_lbl = lv_label_create(s_root);
    lv_label_set_text(s_status_lbl, "Ready to play");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xD1D5DB), 0);
    lv_obj_set_style_text_font(s_status_lbl, LV_FONT_DEFAULT, 0);
    lv_obj_align(s_status_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
}