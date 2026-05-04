#include "solitaire.h"
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* --- List Operations --- */

void card_list_push_tail(CardList *list, Card *card) {
    if (!card || !list) return;
    card->next = NULL;
    card->prev = list->tail;
    if (list->tail) {
        list->tail->next = card;
    } else {
        list->head = card;
    }
    list->tail = card;
    list->count++;
}

Card* card_list_pop_tail(CardList *list) {
    if (!list || !list->tail) return NULL;
    Card *card = list->tail;
    list->tail = card->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }
    card->prev = NULL;
    card->next = NULL;
    list->count--;
    return card;
}

void card_list_remove(CardList *list, Card *card) {
    if (!list || !card) return;
    
    if (card->prev) {
        card->prev->next = card->next;
    } else {
        list->head = card->next;
    }
    
    if (card->next) {
        card->next->prev = card->prev;
    } else {
        list->tail = card->prev;
    }
    
    card->prev = NULL;
    card->next = NULL;
    list->count--;
}

void card_list_move_sublist(CardList *src, CardList *dest, Card *start_node) {
    if (!src || !dest || !start_node) return;

    uint32_t move_count = 0;
    Card *curr = start_node;
    while (curr) {
        move_count++;
        curr = curr->next;
    }

    /* Detach from source list */
    if (start_node->prev) {
        start_node->prev->next = NULL;
        src->tail = start_node->prev;
    } else {
        src->head = NULL;
        src->tail = NULL;
    }
    src->count -= move_count;

    /* Attach to destination list */
    start_node->prev = dest->tail;
    if (dest->tail) {
        dest->tail->next = start_node;
    } else {
        dest->head = start_node;
    }

    /* Find new tail for destination */
    curr = start_node;
    while (curr->next) {
        curr = curr->next;
    }
    dest->tail = curr;
    dest->count += move_count;
}

/* --- Deck Management --- */

void init_game_state(GameState *state, int mode) {
    if (!state) return;
    
    free_deck(&state->deck);
    free_deck(&state->waste);
    for(int i=0; i<7; i++) free_deck(&state->tableau[i]);
    for(int i=0; i<4; i++) free_deck(&state->foundation[i]);

    memset(&state->deck, 0, sizeof(CardList));
    memset(&state->waste, 0, sizeof(CardList));
    memset(state->tableau, 0, sizeof(state->tableau));
    memset(state->foundation, 0, sizeof(state->foundation));
    
    state->score = 0;
    state->timer_sec = 0;
    state->is_active = true;
    state->draw_mode = (mode == 3) ? 3 : 1; /* Enforce valid modes */
    
    init_deck(&state->deck);
    shuffle_deck(&state->deck);
}

void restart_game(GameState *state) {
    if (!state) return;
    /* Preserve the current draw mode when restarting */
    int current_mode = state->draw_mode;
    init_game_state(state, current_mode);
    deal_initial_tableau(state);
}

void init_deck(CardList *deck) {
    for (int s = SUIT_HEARTS; s <= SUIT_SPADES; s++) {
        for (int r = RANK_ACE; r <= RANK_KING; r++) {
            Card *c = g_malloc0(sizeof(Card));
            c->suit = (Suit)s;
            c->rank = (Rank)r;
            c->is_face_up = false;
            c->ui_context = NULL;
            card_list_push_tail(deck, c);
        }
    }
}

void shuffle_deck(CardList *deck) {
    if (!deck || deck->count == 0) return;
    srand((unsigned int)time(NULL));

    uint32_t n = deck->count;
    Card **arr = g_malloc(sizeof(Card*) * n);
    
    Card *curr = deck->head;
    for (uint32_t i = 0; i < n; i++) {
        arr[i] = curr;
        curr = curr->next;
    }

    for (uint32_t i = n - 1; i > 0; i--) {
        uint32_t j = rand() % (i + 1);
        Card *tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }

    deck->head = arr[0];
    deck->tail = arr[n-1];
    for (uint32_t i = 0; i < n; i++) {
        arr[i]->prev = (i == 0) ? NULL : arr[i-1];
        arr[i]->next = (i == n-1) ? NULL : arr[i+1];
    }
    g_free(arr);
}

void free_deck(CardList *list) {
    Card *curr = list->head;
    while (curr) {
        Card *next = curr->next;
        g_free(curr);
        curr = next;
    }
    list->head = list->tail = NULL;
    list->count = 0;
}

/* --- Gameplay Logic --- */

void deal_initial_tableau(GameState *state) {
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j <= i; j++) {
            Card *c = card_list_pop_tail(&state->deck);
            if (j == i) c->is_face_up = true;
            card_list_push_tail(&state->tableau[i], c);
        }
    }
}

void draw_cards(GameState *state) {
    if (state->deck.count == 0) {
        /* Recycle waste to deck, flipping them face down */
        while (state->waste.count > 0) {
            Card *c = card_list_pop_tail(&state->waste);
            c->is_face_up = false;
            card_list_push_tail(&state->deck, c);
        }
    } else {
        /* Draw cards based on current mode (1 or 3) */
        uint32_t to_draw = (state->deck.count < (uint32_t)state->draw_mode) 
                           ? state->deck.count 
                           : (uint32_t)state->draw_mode;

        for (uint32_t i = 0; i < to_draw; i++) {
            Card *c = card_list_pop_tail(&state->deck);
            c->is_face_up = true;
            card_list_push_tail(&state->waste, c);
        }
    }
}

bool is_red(Suit suit) {
    return (suit == SUIT_HEARTS || suit == SUIT_DIAMONDS);
}

bool can_place_on_tableau(Card *bottom_card, Card *new_card) {
    /* If tableau column is empty, only Kings can be placed */
    if (!bottom_card) return (new_card->rank == RANK_KING);
    
    if (!bottom_card->is_face_up) return false;
    
    /* Alternating colors and descending rank */
    bool colors_diff = is_red(bottom_card->suit) != is_red(new_card->suit);
    bool rank_desc = (bottom_card->rank == new_card->rank + 1);
    
    return (colors_diff && rank_desc);
}

bool can_place_on_foundation(Card *top_foundation_card, Card *new_card) {
    /* If foundation is empty, only Aces can be placed */
    if (!top_foundation_card) return (new_card->rank == RANK_ACE);
    
    /* Matching suit and ascending rank */
    bool suit_match = (top_foundation_card->suit == new_card->suit);
    bool rank_asc = (new_card->rank == top_foundation_card->rank + 1);
    
    return (suit_match && rank_asc);
}

CardList* find_list_for_card(GameState *state, Card *card) {
    if (!card) return NULL;
    
    Card *curr = state->waste.head;
    while(curr) { if(curr == card) return &state->waste; curr = curr->next; }
    
    for(int i=0; i<7; i++) {
        curr = state->tableau[i].head;
        while(curr) { if(curr == card) return &state->tableau[i]; curr = curr->next; }
    }
    
    for(int i=0; i<4; i++) {
        curr = state->foundation[i].head;
        while(curr) { if(curr == card) return &state->foundation[i]; curr = curr->next; }
    }
    
    curr = state->deck.head;
    while(curr) { if(curr == card) return &state->deck; curr = curr->next; }
    
    return NULL;
}

void apply_scoring_event(GameState *state, int event_type) {
    switch(event_type) {
        case 0: state->score += 5; break;  /* Tableau move */
        case 1: state->score += 10; break; /* Foundation move */
        case 4: state->score += 5; break;  /* Card flipped face up */
    }
    
    if (state->score_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
        gtk_label_set_text(GTK_LABEL(state->score_label), buf);
    }
}

void check_win_condition(GameState *state) {
    uint32_t total = 0;
    for(int i=0; i<4; i++) {
        total += state->foundation[i].count;
    }
    
    if (total == 52) {
        state->is_active = false;
        
        char msg[128];
        snprintf(msg, sizeof(msg), "Congratulations! You won with a score of %d in %02d:%02d!", 
                 state->score, state->timer_sec / 60, state->timer_sec % 60);
        
        GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", msg);
        gtk_alert_dialog_show(dialog, GTK_WINDOW(state->window));
        g_object_unref(dialog);
    }
}