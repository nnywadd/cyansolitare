#include "solitaire.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

GameState game_state;

/* --- Helpers --- */
const char* get_suit_sym(Suit s) {
    switch(s) {
        case SUIT_HEARTS: return "♥";
        case SUIT_DIAMONDS: return "♦";
        case SUIT_CLUBS: return "♣";
        case SUIT_SPADES: return "♠";
        default: return "";
    }
}

const char* get_rank_str(Rank r) {
    switch(r) {
        case RANK_ACE: return "A";
        case RANK_2: return "2"; case RANK_3: return "3"; case RANK_4: return "4";
        case RANK_5: return "5"; case RANK_6: return "6"; case RANK_7: return "7";
        case RANK_8: return "8"; case RANK_9: return "9"; case RANK_10: return "10";
        case RANK_JACK: return "J"; case RANK_QUEEN: return "Q"; case RANK_KING: return "K";
        default: return "";
    }
}

/* --- High-Fidelity Cyberpunk Theme --- */
static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    
    gtk_css_provider_load_from_string(provider,
        "window { background-color: #030C14; }"
        "headerbar { background-color: #051421; border-bottom: 2px solid #005f73; }"
        "button { font-family: \"JetBrains Mono\", monospace; font-weight: bold; color: #15F4EE; background: #0A192F; border: 1px solid #15F4EE; border-radius: 8px; }"
        "button:hover { background: #15F4EE; color: #030C14; box-shadow: 0 0 10px #15F4EE; }"
        "button:disabled { color: #333; border-color: #333; background: #111; box-shadow: none; }"
        ".status-label { font-family: \"JetBrains Mono\", monospace; font-size: 20px; font-weight: 900; color: #15F4EE; text-shadow: 0 0 8px rgba(21,244,238,0.6); }"
        ".card-widget { "
        "  border: 4px solid #005f73; border-radius: 20px; "
        "  min-width: 360px; min-height: 500px; margin: 0px; "
        "  box-shadow: 0 10px 20px rgba(0,0,0,0.8); "
        "  background-color: #0A192F; "
        "}"
        ".card-back { "
        "  background: linear-gradient(135deg, #051421 25%, transparent 25%) -50px 0, "
        "              linear-gradient(225deg, #051421 25%, transparent 25%) -50px 0, "
        "              linear-gradient(315deg, #051421 25%, transparent 25%), "
        "              linear-gradient(45deg, #051421 25%, transparent 25%); "
        "  background-size: 100px 100px; background-color: #005f73; "
        "  border: 4px solid #1E2E3E; "
        "}"
        ".card-face-up { "
        "  border: 4px solid #15F4EE; background-color: #0A192F;"
        "}"
        ".card-corner { "
        "  font-family: \"JetBrains Mono\", monospace; "
        "  font-size: 85px; font-weight: bold; padding: 15px; line-height: 1.0; "
        "}"
        ".card-corner-bottom { transform: rotate(180deg); }"
        ".card-center-watermark { "
        "  font-family: \"JetBrains Mono\", monospace; "
        "  font-size: 450px; font-weight: 900; opacity: 0.10; margin: 0; padding: 0; "
        "}"
        ".card-center-rank { "
        "  font-family: \"JetBrains Mono\", monospace; "
        "  font-size: 320px; font-weight: bold; opacity: 1.0; "
        "}"
        ".neon-violet { "
        "  color: #BF00FF; "
        "  text-shadow: 0px 0px 15px rgba(191,0,255,0.9), 0px 0px 30px rgba(191,0,255,0.6); "
        "}"
        ".neon-cyan { "
        "  color: #15F4EE; "
        "  text-shadow: 0px 0px 15px rgba(21,244,238,0.9), 0px 0px 30px rgba(21,244,238,0.6); "
        "}"
        ".foundation-imprint { font-size: 250px; opacity: 0.15; }"
        ".card-placeholder { background-color: rgba(0, 95, 115, 0.1); border: 4px dashed #005f73; box-shadow: inset 0 0 20px rgba(0,0,0,0.5); }");

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

/* --- Layout & Animation Engine --- */

static double ease_out_cubic(double t) {
    t -= 1.0;
    return t * t * t + 1.0;
}

void trigger_layout_update(GameState *state) {
    int col_stride = 400, card_overlap = 80, margin_x = 100, margin_y = 100;
    
    /* Pre-calculate target positions and z-index ordering */
    CardList* lists[] = { &state->deck, &state->waste, 
        &state->foundation[0], &state->foundation[1], &state->foundation[2], &state->foundation[3],
        &state->tableau[0], &state->tableau[1], &state->tableau[2], &state->tableau[3],
        &state->tableau[4], &state->tableau[5], &state->tableau[6] };
    
    for (int i=0; i<13; i++) {
        Card *curr = lists[i]->head;
        int waste_idx = 0;
        int y_offset = 0;
        
        while(curr) {
            /* Initialize animation state if target changed */
            double old_target_x = curr->target_x;
            double old_target_y = curr->target_y;

            if (lists[i] == &state->deck) {
                curr->target_x = margin_x; curr->target_y = margin_y;
            } else if (lists[i] == &state->waste) {
                int x_offset = 0;
                int waste_count = state->waste.count;
                if (state->draw_mode == 3) {
                    if (waste_idx >= waste_count - 3 && waste_count >= 3) {
                         x_offset = (waste_idx - (waste_count - 3)) * 60; 
                    } else if (waste_count < 3) {
                         x_offset = waste_idx * 60;
                    }
                }
                curr->target_x = margin_x + col_stride + x_offset; 
                curr->target_y = margin_y;
                waste_idx++;
            } else if (lists[i] >= &state->foundation[0] && lists[i] <= &state->foundation[3]) {
                int f_idx = lists[i] - &state->foundation[0];
                curr->target_x = margin_x + (3 + f_idx) * col_stride; 
                curr->target_y = margin_y;
            } else {
                int t_idx = lists[i] - &state->tableau[0];
                curr->target_x = margin_x + t_idx * col_stride; 
                curr->target_y = margin_y + 550 + y_offset;
                y_offset += (curr->is_face_up) ? card_overlap : (card_overlap / 2);
            }

            /* Trigger animation recalculation if destination moved */
            if (fabs(old_target_x - curr->target_x) > 0.1 || fabs(old_target_y - curr->target_y) > 0.1) {
                curr->start_x = curr->current_x;
                curr->start_y = curr->current_y;
                curr->anim_progress = 0.0;
                /* Bring animating cards to the front */
                gtk_widget_insert_before(GTK_WIDGET(curr->ui_context), state->play_area, NULL);
            } else if (curr->anim_progress >= 1.0) {
                /* Preserve correct Z-order for static stacks */
                gtk_widget_insert_before(GTK_WIDGET(curr->ui_context), state->play_area, NULL);
            }

            curr = curr->next;
        }
    }
}

static gboolean on_anim_tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data) {
    (void)widget; (void)frame_clock;
    GameState *state = (GameState*)user_data;
    
    CardList* lists[] = { &state->deck, &state->waste, 
        &state->foundation[0], &state->foundation[1], &state->foundation[2], &state->foundation[3],
        &state->tableau[0], &state->tableau[1], &state->tableau[2], &state->tableau[3],
        &state->tableau[4], &state->tableau[5], &state->tableau[6] };
    
    for (int i=0; i<13; i++) {
        Card *curr = lists[i]->head;
        while (curr) {
            if (curr->ui_context && curr->anim_progress < 1.0) {
                curr->anim_progress += 0.08; /* ~12 frames at 60fps */
                if (curr->anim_progress > 1.0) curr->anim_progress = 1.0;
                
                double t = ease_out_cubic(curr->anim_progress);
                curr->current_x = curr->start_x + (curr->target_x - curr->start_x) * t;
                curr->current_y = curr->start_y + (curr->target_y - curr->start_y) * t;
                
                gtk_fixed_move(GTK_FIXED(state->play_area), GTK_WIDGET(curr->ui_context), curr->current_x, curr->current_y);
            }
            curr = curr->next;
        }
    }
    return G_SOURCE_CONTINUE;
}

/* --- Core UI Visuals --- */

static GtkWidget* create_empty_slot(const char *text, bool is_foundation, const char *color_class) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "card-widget");
    gtk_widget_add_css_class(box, "card-placeholder");

    if (text) {
        GtkWidget *label = gtk_label_new(text);
        if (is_foundation) {
            gtk_widget_add_css_class(label, "foundation-imprint");
            if (color_class) gtk_widget_add_css_class(label, color_class);
        } else {
            gtk_widget_add_css_class(label, "card-center-watermark");
        }
        gtk_widget_set_vexpand(label, TRUE);
        gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(box), label);
    }
    return box;
}

static void refresh_card_visual(Card *card) {
    if (!card || !card->ui_context) return;
    GtkWidget *box = GTK_WIDGET(card->ui_context);

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(box)) != NULL) {
        gtk_box_remove(GTK_BOX(box), child);
    }

    gtk_widget_remove_css_class(box, "card-back");
    gtk_widget_remove_css_class(box, "card-face-up");

    if (!card->is_face_up) {
        gtk_widget_add_css_class(box, "card-back");
        return; 
    }

    gtk_widget_add_css_class(box, "card-face-up");
    
    const char *color_class = is_red(card->suit) ? "neon-violet" : "neon-cyan";
    const char *suit_sym = get_suit_sym(card->suit);
    const char *rank_str = get_rank_str(card->rank);

    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *tl_label = gtk_label_new(g_strdup_printf("%s\n%s", rank_str, suit_sym));
    gtk_widget_add_css_class(tl_label, "card-corner"); gtk_widget_add_css_class(tl_label, color_class);
    gtk_widget_set_halign(tl_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(top_row), tl_label);

    GtkWidget *tr_label = gtk_label_new(g_strdup_printf("%s\n%s", rank_str, suit_sym));
    gtk_widget_add_css_class(tr_label, "card-corner"); gtk_widget_add_css_class(tr_label, color_class);
    gtk_widget_set_halign(tr_label, GTK_ALIGN_END); gtk_widget_set_hexpand(tr_label, TRUE); 
    gtk_box_append(GTK_BOX(top_row), tr_label);
    gtk_box_append(GTK_BOX(box), top_row);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_widget_set_hexpand(overlay, TRUE); gtk_widget_set_vexpand(overlay, TRUE);

    GtkWidget *watermark = gtk_label_new(suit_sym);
    gtk_widget_add_css_class(watermark, "card-center-watermark"); gtk_widget_add_css_class(watermark, color_class);
    gtk_widget_set_halign(watermark, GTK_ALIGN_CENTER); gtk_widget_set_valign(watermark, GTK_ALIGN_CENTER);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), watermark);

    GtkWidget *center_rank = gtk_label_new(rank_str);
    gtk_widget_add_css_class(center_rank, "card-center-rank"); gtk_widget_add_css_class(center_rank, color_class);
    gtk_widget_set_halign(center_rank, GTK_ALIGN_CENTER); gtk_widget_set_valign(center_rank, GTK_ALIGN_CENTER);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), center_rank);
    gtk_box_append(GTK_BOX(box), overlay);

    GtkWidget *bot_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *bl_label = gtk_label_new(g_strdup_printf("%s\n%s", rank_str, suit_sym));
    gtk_widget_add_css_class(bl_label, "card-corner"); gtk_widget_add_css_class(bl_label, "card-corner-bottom");
    gtk_widget_add_css_class(bl_label, color_class); gtk_widget_set_halign(bl_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(bot_row), bl_label);

    GtkWidget *br_label = gtk_label_new(g_strdup_printf("%s\n%s", rank_str, suit_sym));
    gtk_widget_add_css_class(br_label, "card-corner"); gtk_widget_add_css_class(br_label, "card-corner-bottom");
    gtk_widget_add_css_class(br_label, color_class); gtk_widget_set_halign(br_label, GTK_ALIGN_END); 
    gtk_widget_set_hexpand(br_label, TRUE); gtk_box_append(GTK_BOX(bot_row), br_label);
    gtk_box_append(GTK_BOX(box), bot_row);
}

void force_full_ui_refresh(GameState *state) {
    CardList* lists[] = { &state->deck, &state->waste, 
        &state->foundation[0], &state->foundation[1], &state->foundation[2], &state->foundation[3],
        &state->tableau[0], &state->tableau[1], &state->tableau[2], &state->tableau[3],
        &state->tableau[4], &state->tableau[5], &state->tableau[6] };
    
    for (int i=0; i<13; i++) {
        Card *curr = lists[i]->head;
        while(curr) {
            refresh_card_visual(curr);
            curr = curr->next;
        }
    }
    trigger_layout_update(state);
}

/* --- Interactions & Handlers --- */

static GdkContentProvider* on_drag_prepare(GtkDragSource *source, double x, double y, gpointer user_data) {
    Card *card = (Card*)user_data;
    if (!card->is_face_up) return NULL; 

    CardList *src_list = find_list_for_card(&game_state, card);
    if (src_list == &game_state.waste && card != game_state.waste.tail) return NULL;
    if (src_list >= &game_state.tableau[0] && src_list <= &game_state.tableau[6]) {
        if (!is_valid_sublist_move(card)) return NULL;
    }

    /* Build composite drag icon for multi-card stacks */
    GtkWidget *ghost_fixed = gtk_fixed_new();
    gtk_widget_set_size_request(ghost_fixed, 360, 500 + (card->next ? 400 : 0));
    
    int y_offset = 0;
    Card *curr = card;
    while (curr) {
        GtkWidget *snapshot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(snapshot, "card-widget");
        gtk_widget_add_css_class(snapshot, "card-face-up");
        /* Simplified visuals for the drag icon for performance */
        GtkWidget *lbl = gtk_label_new(g_strdup_printf("%s %s", get_rank_str(curr->rank), get_suit_sym(curr->suit)));
        gtk_widget_add_css_class(lbl, "card-corner");
        gtk_widget_add_css_class(lbl, is_red(curr->suit) ? "neon-violet" : "neon-cyan");
        gtk_box_append(GTK_BOX(snapshot), lbl);
        
        gtk_fixed_put(GTK_FIXED(ghost_fixed), snapshot, 0, y_offset);
        y_offset += 80;
        curr = curr->next;
    }

    GdkPaintable *paintable = gtk_widget_paintable_new(ghost_fixed);
    gtk_drag_source_set_icon(source, paintable, (int)x, (int)y);
    g_object_unref(paintable);

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, card);
    return gdk_content_provider_new_for_value(&val);
}

static gboolean handle_drop_logic(Card *dragged_card, CardList *dest_list) {
    if (!dragged_card || !dest_list) return FALSE;
    CardList *src_list = find_list_for_card(&game_state, dragged_card);
    if (!src_list || src_list == dest_list) return FALSE;

    bool valid = false;
    int event_type = 0; /* 0: Tab->Tab, 1: Tab/Waste->Found, 2: Found->Tab */
    
    if (dest_list >= &game_state.foundation[0] && dest_list <= &game_state.foundation[3]) {
        valid = can_place_on_foundation(dest_list->tail, dragged_card);
        event_type = 1;
    } else if (dest_list >= &game_state.tableau[0] && dest_list <= &game_state.tableau[6]) {
        valid = can_place_on_tableau(dest_list->tail, dragged_card);
        if (src_list >= &game_state.foundation[0] && src_list <= &game_state.foundation[3]) {
            event_type = 2;
        }
    }

    if (valid) {
        uint32_t count = 0;
        Card *curr = dragged_card;
        while(curr) { count++; curr = curr->next; }

        card_list_move_sublist(src_list, dest_list, dragged_card);
        
        bool flipped = false;
        if (src_list->tail && !src_list->tail->is_face_up) {
            src_list->tail->is_face_up = true;
            flipped = true;
        }

        /* Record move for Undo stack */
        record_move(&game_state, MOVE_TYPE_STANDARD, src_list, dest_list, dragged_card, count, flipped, 0);
        
        apply_scoring_event(&game_state, event_type, game_state.history.tail);
        if (flipped) apply_scoring_event(&game_state, 4, game_state.history.tail);

        force_full_ui_refresh(&game_state);
        check_win_condition(&game_state);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_list_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer user_data) {
    (void)target; (void)x; (void)y;
    return handle_drop_logic((Card*)g_value_get_pointer(value), (CardList*)user_data);
}

static gboolean on_card_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer user_data) {
    (void)target; (void)x; (void)y;
    Card *target_card = (Card*)user_data;
    CardList *dest_list = find_list_for_card(&game_state, target_card);
    return handle_drop_logic((Card*)g_value_get_pointer(value), dest_list);
}

static void on_stock_empty_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    (void)gesture; (void)n_press; (void)x; (void)y;
    GameState *state = (GameState*)user_data;
    if (state->deck.count == 0 && state->waste.count > 0) {
        draw_cards(state);
        force_full_ui_refresh(state);
    }
}

static void on_card_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    (void)gesture; (void)n_press; (void)x; (void)y;
    Card *card = (Card*)user_data;
    CardList *list = find_list_for_card(&game_state, card);
    
    if (list == &game_state.deck && card == list->tail) {
        draw_cards(&game_state);
        force_full_ui_refresh(&game_state);
    }
}

static void on_menu_restart(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    restart_game(&game_state);
    force_full_ui_refresh(&game_state);
}

static void on_menu_toggle_draw(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    game_state.draw_mode = (game_state.draw_mode == 3) ? 1 : 3;
    gtk_button_set_label(btn, game_state.draw_mode == 3 ? "Mode: Draw 3" : "Mode: Draw 1");
    restart_game(&game_state);
    force_full_ui_refresh(&game_state);
}

static void on_menu_undo(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    undo_last_move(&game_state);
    force_full_ui_refresh(&game_state);
}

static void on_menu_quit(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    cleanup_game_state(&game_state);
    gtk_window_destroy(GTK_WINDOW(game_state.window));
}

static gboolean on_timer_tick(gpointer user_data) {
    GameState *state = (GameState *)user_data;
    if (!state->is_active) return G_SOURCE_CONTINUE;

    state->timer_sec++;
    char buf[32];
    snprintf(buf, sizeof(buf), "TIME: %02d:%02d", state->timer_sec / 60, state->timer_sec % 60);
    gtk_label_set_text(GTK_LABEL(state->timer_label), buf);

    return G_SOURCE_CONTINUE;
}

/* --- Initialization --- */

static void setup_empty_slot_drop(GtkWidget *widget, CardList *list) {
    GtkDropTarget *target = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_MOVE);
    g_signal_connect(target, "drop", G_CALLBACK(on_list_drop), list);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(target));
}

static void setup_card_controllers(Card *card) {
    GtkWidget *widget = GTK_WIDGET(card->ui_context);
    
    GtkDragSource *source = gtk_drag_source_new();
    gtk_drag_source_set_actions(source, GDK_ACTION_MOVE);
    g_signal_connect(source, "prepare", G_CALLBACK(on_drag_prepare), card);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(source));

    GtkDropTarget *target = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_MOVE);
    g_signal_connect(target, "drop", G_CALLBACK(on_card_drop), card);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(target));

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(on_card_clicked), card);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));
}

void init_ui(GameState *state) {
    int margin_x = 100, margin_y = 100, col_stride = 400; 
    
    GtkWidget *stock_empty = create_empty_slot("⟳", false, NULL);
    gtk_fixed_put(GTK_FIXED(state->play_area), stock_empty, margin_x, margin_y);
    GtkGesture *stock_click = gtk_gesture_click_new();
    g_signal_connect(stock_click, "pressed", G_CALLBACK(on_stock_empty_clicked), state);
    gtk_widget_add_controller(stock_empty, GTK_EVENT_CONTROLLER(stock_click));

    GtkWidget *waste_empty = create_empty_slot(NULL, false, NULL);
    gtk_fixed_put(GTK_FIXED(state->play_area), waste_empty, margin_x + col_stride, margin_y);

    const char* f_suits[] = {"♥", "♦", "♣", "♠"};
    const char* f_colors[] = {"neon-violet", "neon-violet", "neon-cyan", "neon-cyan"};
    for (int i=0; i<4; i++) {
        GtkWidget *f_empty = create_empty_slot(f_suits[i], true, f_colors[i]);
        gtk_fixed_put(GTK_FIXED(state->play_area), f_empty, margin_x + (3+i)*col_stride, margin_y);
        setup_empty_slot_drop(f_empty, &state->foundation[i]);
    }

    for (int i=0; i<7; i++) {
        GtkWidget *t_empty = create_empty_slot("K", false, NULL);
        gtk_fixed_put(GTK_FIXED(state->play_area), t_empty, margin_x + i*col_stride, margin_y + 550);
        setup_empty_slot_drop(t_empty, &state->tableau[i]);
    }

    Card *curr = state->deck.head;
    while(curr) {
        curr->ui_context = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(GTK_WIDGET(curr->ui_context), "card-widget");
        
        curr->current_x = margin_x; curr->current_y = margin_y;
        curr->start_x = margin_x; curr->start_y = margin_y;
        curr->target_x = margin_x; curr->target_y = margin_y;
        curr->anim_progress = 1.0;
        
        gtk_fixed_put(GTK_FIXED(state->play_area), GTK_WIDGET(curr->ui_context), margin_x, margin_y);
        setup_card_controllers(curr);
        
        curr = curr->next;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    gtk_init();
    load_css();

    game_state.window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(game_state.window), "Cyan Solitaire - Cyberpunk Edition");
    gtk_window_maximize(GTK_WINDOW(game_state.window));

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(game_state.window), main_box);

    /* --- Header & Menu --- */
    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header_bar), TRUE);

    GtkWidget *menu_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_btn), "open-menu-symbolic");
    
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 15); gtk_widget_set_margin_end(vbox, 15);
    gtk_widget_set_margin_top(vbox, 15); gtk_widget_set_margin_bottom(vbox, 15);

    GtkWidget *btn_restart = gtk_button_new_with_label("Restart Game");
    g_signal_connect(btn_restart, "clicked", G_CALLBACK(on_menu_restart), NULL);
    gtk_box_append(GTK_BOX(vbox), btn_restart);

    GtkWidget *btn_mode = gtk_button_new_with_label("Mode: Draw 3");
    g_signal_connect(btn_mode, "clicked", G_CALLBACK(on_menu_toggle_draw), NULL);
    gtk_box_append(GTK_BOX(vbox), btn_mode);

    game_state.undo_button = gtk_button_new_with_label("Undo Last Move");
    gtk_widget_set_sensitive(game_state.undo_button, FALSE);
    g_signal_connect(game_state.undo_button, "clicked", G_CALLBACK(on_menu_undo), NULL);
    gtk_box_append(GTK_BOX(vbox), game_state.undo_button);

    GtkWidget *btn_quit = gtk_button_new_with_label("Quit");
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(on_menu_quit), NULL);
    gtk_box_append(GTK_BOX(vbox), btn_quit);

    gtk_popover_set_child(GTK_POPOVER(popover), vbox);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_btn), popover);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), menu_btn);

    game_state.score_label = gtk_label_new("SCORE: 0");
    gtk_widget_add_css_class(game_state.score_label, "status-label");
    gtk_widget_set_margin_start(game_state.score_label, 30); gtk_widget_set_margin_end(game_state.score_label, 15);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), game_state.score_label);

    game_state.timer_label = gtk_label_new("TIME: 00:00");
    gtk_widget_add_css_class(game_state.timer_label, "status-label");
    gtk_widget_set_margin_end(game_state.timer_label, 30); gtk_widget_set_margin_start(game_state.timer_label, 15);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), game_state.timer_label);

    gtk_box_append(GTK_BOX(main_box), header_bar);

    /* --- Play Area --- */
    game_state.play_area = gtk_fixed_new();
    gtk_widget_set_hexpand(game_state.play_area, TRUE);
    gtk_widget_set_vexpand(game_state.play_area, TRUE);
    gtk_box_append(GTK_BOX(main_box), game_state.play_area);

    init_game_state(&game_state, 3);
    init_ui(&game_state);
    deal_initial_tableau(&game_state);
    force_full_ui_refresh(&game_state);

    game_state.timer_source_id = g_timeout_add(1000, on_timer_tick, &game_state);
    gtk_widget_add_tick_callback(game_state.play_area, on_anim_tick, &game_state, NULL);

    gtk_window_present(GTK_WINDOW(game_state.window));

    while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0) {
        g_main_context_iteration(NULL, TRUE);
    }

    return 0;
}