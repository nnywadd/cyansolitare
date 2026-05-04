#ifndef CYAN_SOLITAIRE_H
#define CYAN_SOLITAIRE_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdint.h>

/* --- Enumerations --- */

typedef enum {
    SUIT_HEARTS,
    SUIT_DIAMONDS,
    SUIT_CLUBS,
    SUIT_SPADES
} Suit;

typedef enum {
    RANK_ACE = 1,
    RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9, RANK_10,
    RANK_JACK, RANK_QUEEN, RANK_KING
} Rank;

/* --- Data Structures --- */

typedef struct Card {
    Rank rank;
    Suit suit;
    bool is_face_up;

    void *ui_context; // Points to the GtkWidget (Card UI)

    /* Animation and Layout State */
    double current_x;
    double current_y;
    double target_x;
    double target_y;
    bool is_animating;

    struct Card *prev;
    struct Card *next;
} Card;

typedef struct {
    Card *head;
    Card *tail;
    uint32_t count;
} CardList;

typedef struct {
    CardList deck;           
    CardList waste;          
    CardList tableau[7];     
    CardList foundation[4];  

    int32_t score;           
    uint32_t timer_sec;      
    bool is_active;          
    
    /* Gameplay Modes */
    int draw_mode;           // Configurable: 1 for 1-card draw, 3 for 3-card draw

    GtkWidget *window;
    GtkWidget *play_area;    // GtkFixed for manual positioning
    GtkWidget *score_label;
    GtkWidget *timer_label;
    
    guint timer_source_id;   
    guint anim_source_id;    // Tracks the 60fps animation tick
} GameState;

/* --- Core Logic Prototypes --- */

void init_game_state(GameState *state, int mode);
void init_deck(CardList *deck);
void shuffle_deck(CardList *deck);
void free_deck(CardList *deck);

void card_list_push_tail(CardList *list, Card *card);
Card* card_list_pop_tail(CardList *list);
void card_list_move_sublist(CardList *src, CardList *dest, Card *start_node);
void card_list_remove(CardList *list, Card *card); 

void deal_initial_tableau(GameState *state);
void draw_cards(GameState *state); 
void restart_game(GameState *state); // Added for the Game Management Menu

bool is_red(Suit suit);
bool can_place_on_tableau(Card *bottom_card, Card *new_card);
bool can_place_on_foundation(Card *top_foundation_card, Card *new_card);
void apply_scoring_event(GameState *state, int event_type);

CardList* find_list_for_card(GameState *state, Card *card);
void check_win_condition(GameState *state);

/* --- UI Helper Prototypes --- */
const char* get_suit_sym(Suit s);
const char* get_rank_str(Rank r);

#endif /* CYAN_SOLITAIRE_H */