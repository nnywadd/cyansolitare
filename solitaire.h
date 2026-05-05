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

    void *ui_context; /* Points to the GtkWidget (Card UI) */

    /* Advanced Animation and Layout State */
    double current_x;
    double current_y;
    double start_x;
    double start_y;
    double target_x;
    double target_y;
    double anim_progress; /* 0.0 to 1.0 for interpolation/easing */
    bool is_animating;
    int z_index;          /* For proper depth sorting during animations */

    struct Card *prev;
    struct Card *next;
} Card;

typedef struct {
    Card *head;
    Card *tail;
    uint32_t count;
} CardList;

/* --- Undo/Redo Engine Structures --- */

typedef enum {
    MOVE_TYPE_STANDARD, /* Moving cards between piles */
    MOVE_TYPE_DRAW,     /* Drawing from deck to waste */
    MOVE_TYPE_RECYCLE   /* Moving waste back to deck */
} MoveType;

typedef struct MoveRecord {
    MoveType type;
    CardList *src;
    CardList *dest;
    Card *card_head;      /* The highest card moved in a stack */
    uint32_t card_count;  /* Number of cards moved */
    bool flipped_src_tail;/* True if this move caused the source's new tail to flip face-up */
    int score_delta;      /* Points gained/lost during this specific move */
    
    struct MoveRecord *next;
    struct MoveRecord *prev;
} MoveRecord;

typedef struct {
    MoveRecord *head;
    MoveRecord *tail;
    uint32_t count;
} UndoStack;

/* --- Global Game State --- */

typedef struct {
    CardList deck;           
    CardList waste;          
    CardList tableau[7];     
    CardList foundation[4];  

    UndoStack history;       /* Move history for Undo functionality */

    int32_t score;           
    uint32_t timer_sec;      
    bool is_active;          
    
    /* Gameplay Modes */
    int draw_mode;           /* 1 for 1-card draw, 3 for 3-card draw */

    /* UI Context */
    GtkWidget *window;
    GtkWidget *play_area;    /* GtkFixed for manual absolute positioning */
    GtkWidget *score_label;
    GtkWidget *timer_label;
    GtkWidget *undo_button;  /* Reference needed to update sensitivity */
    
    guint timer_source_id;   
    guint anim_source_id;    /* Tracks the 60fps animation tick */
} GameState;

/* --- Core Logic Prototypes --- */

void init_game_state(GameState *state, int mode);
void restart_game(GameState *state);
void cleanup_game_state(GameState *state); /* Strict memory management */

void init_deck(CardList *deck);
void shuffle_deck(CardList *deck);
void free_deck(CardList *deck);

void card_list_push_tail(CardList *list, Card *card);
Card* card_list_pop_tail(CardList *list);
void card_list_move_sublist(CardList *src, CardList *dest, Card *start_node);
void card_list_remove(CardList *list, Card *card); 

void deal_initial_tableau(GameState *state);
void draw_cards(GameState *state); 

bool is_red(Suit suit);
bool can_place_on_tableau(Card *bottom_card, Card *new_card);
bool can_place_on_foundation(Card *top_foundation_card, Card *new_card);
bool is_valid_sublist_move(Card *start_card); /* Validates strict Klondike multi-card logic */

void apply_scoring_event(GameState *state, int event_type, MoveRecord *record);
CardList* find_list_for_card(GameState *state, Card *card);
void check_win_condition(GameState *state);

/* --- Undo/Redo Logic Prototypes --- */
void record_move(GameState *state, MoveType type, CardList *src, CardList *dest, Card *head, uint32_t count, bool flipped, int score_delta);
void undo_last_move(GameState *state);
void clear_undo_history(UndoStack *stack);

/* --- UI Helper Prototypes --- */
const char* get_suit_sym(Suit s);
const char* get_rank_str(Rank r);
void trigger_layout_update(GameState *state); /* Abstracted UI sync hook */

#endif /* CYAN_SOLITAIRE_H */