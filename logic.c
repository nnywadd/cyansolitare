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

/* --- Deck & Memory Management --- */

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

void clear_undo_history(UndoStack *stack) {
    if (!stack) return;
    MoveRecord *curr = stack->head;
    while (curr) {
        MoveRecord *next = curr->next;
        g_free(curr);
        curr = next;
    }
    stack->head = stack->tail = NULL;
    stack->count = 0;
}

void cleanup_game_state(GameState *state) {
    if (!state) return;
    free_deck(&state->deck);
    free_deck(&state->waste);
    for(int i=0; i<7; i++) free_deck(&state->tableau[i]);
    for(int i=0; i<4; i++) free_deck(&state->foundation[i]);
    clear_undo_history(&state->history);
}

void init_game_state(GameState *state, int mode) {
    if (!state) return;
    
    cleanup_game_state(state);

    memset(&state->deck, 0, sizeof(CardList));
    memset(&state->waste, 0, sizeof(CardList));
    memset(state->tableau, 0, sizeof(state->tableau));
    memset(state->foundation, 0, sizeof(state->foundation));
    memset(&state->history, 0, sizeof(UndoStack));
    
    state->score = 0;
    state->timer_sec = 0;
    state->is_active = true;
    state->draw_mode = (mode == 3) ? 3 : 1; 
    
    if (state->score_label) {
        gtk_label_set_text(GTK_LABEL(state->score_label), "SCORE: 0");
    }
    if (state->timer_label) {
        gtk_label_set_text(GTK_LABEL(state->timer_label), "TIME: 00:00");
    }
    if (state->undo_button) {
        gtk_widget_set_sensitive(state->undo_button, FALSE);
    }

    init_deck(&state->deck);
    shuffle_deck(&state->deck);
}

void restart_game(GameState *state) {
    if (!state) return;
    int current_mode = state->draw_mode;
    init_game_state(state, current_mode);
    deal_initial_tableau(state);
    trigger_layout_update(state);
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

/* --- Gameplay & Klondike Rules --- */

void deal_initial_tableau(GameState *state) {
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j <= i; j++) {
            Card *c = card_list_pop_tail(&state->deck);
            if (!c) continue;
            if (j == i) c->is_face_up = true;
            card_list_push_tail(&state->tableau[i], c);
        }
    }
}

void draw_cards(GameState *state) {
    if (!state) return;

    if (state->deck.count == 0) {
        if (state->waste.count == 0) return; /* Nothing to do */
        
        uint32_t recycle_count = state->waste.count;
        /* Reverse waste into deck */
        while (state->waste.count > 0) {
            Card *c = card_list_pop_tail(&state->waste);
            c->is_face_up = false;
            card_list_push_tail(&state->deck, c);
        }
        record_move(state, MOVE_TYPE_RECYCLE, &state->waste, &state->deck, state->deck.head, recycle_count, false, 0);
    } else {
        uint32_t to_draw = (state->deck.count < (uint32_t)state->draw_mode) 
                           ? state->deck.count 
                           : (uint32_t)state->draw_mode;
        
        Card *first_drawn = state->deck.tail; /* The card that will end up at the bottom of the drawn batch in waste */
        for (uint32_t i = 0; i < to_draw; i++) {
            Card *c = card_list_pop_tail(&state->deck);
            c->is_face_up = true;
            card_list_push_tail(&state->waste, c);
            if (i == 0) first_drawn = c;
        }
        record_move(state, MOVE_TYPE_DRAW, &state->deck, &state->waste, first_drawn, to_draw, false, 0);
    }
}

bool is_red(Suit suit) {
    return (suit == SUIT_HEARTS || suit == SUIT_DIAMONDS);
}

bool can_place_on_tableau(Card *bottom_card, Card *new_card) {
    if (!new_card) return false;
    if (!bottom_card) return (new_card->rank == RANK_KING);
    if (!bottom_card->is_face_up) return false;
    
    bool colors_diff = is_red(bottom_card->suit) != is_red(new_card->suit);
    bool rank_desc = (bottom_card->rank == new_card->rank + 1);
    return (colors_diff && rank_desc);
}

bool can_place_on_foundation(Card *top_foundation_card, Card *new_card) {
    if (!new_card) return false;
    /* Cannot move multiple cards to foundation at once */
    if (new_card->next != NULL) return false;

    if (!top_foundation_card) return (new_card->rank == RANK_ACE);
    
    bool suit_match = (top_foundation_card->suit == new_card->suit);
    bool rank_asc = (new_card->rank == top_foundation_card->rank + 1);
    return (suit_match && rank_asc);
}

bool is_valid_sublist_move(Card *start_card) {
    if (!start_card || !start_card->is_face_up) return false;
    
    Card *curr = start_card;
    while (curr && curr->next) {
        if (!curr->next->is_face_up) return false;
        bool colors_diff = is_red(curr->suit) != is_red(curr->next->suit);
        bool rank_desc = (curr->rank == curr->next->rank + 1);
        if (!colors_diff || !rank_desc) return false;
        curr = curr->next;
    }
    return true;
}

CardList* find_list_for_card(GameState *state, Card *card) {
    if (!card || !state) return NULL;
    
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

void apply_scoring_event(GameState *state, int event_type, MoveRecord *record) {
    int delta = 0;
    switch(event_type) {
        case 0: delta = 5; break;   /* Tableau move */
        case 1: delta = 10; break;  /* Foundation move */
        case 2: delta = -15; break; /* Foundation to Tableau */
        case 4: delta = 5; break;   /* Card flipped face up */
    }
    
    state->score += delta;
    if (state->score < 0) state->score = 0;
    
    if (record) {
        record->score_delta += delta; 
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

/* --- Undo Engine --- */

void record_move(GameState *state, MoveType type, CardList *src, CardList *dest, Card *head, uint32_t count, bool flipped, int score_delta) {
    if (!state) return;
    
    MoveRecord *rec = g_malloc0(sizeof(MoveRecord));
    rec->type = type;
    rec->src = src;
    rec->dest = dest;
    rec->card_head = head;
    rec->card_count = count;
    rec->flipped_src_tail = flipped;
    rec->score_delta = score_delta;
    
    rec->prev = state->history.tail;
    rec->next = NULL;
    
    if (state->history.tail) {
        state->history.tail->next = rec;
    } else {
        state->history.head = rec;
    }
    state->history.tail = rec;
    state->history.count++;

    if (state->undo_button) gtk_widget_set_sensitive(state->undo_button, TRUE);
}

void undo_last_move(GameState *state) {
    if (!state || state->history.count == 0) return;
    
    MoveRecord *rec = state->history.tail;
    
    if (rec->type == MOVE_TYPE_STANDARD) {
        /* If a card was flipped face up by this move, flip it back down */
        if (rec->flipped_src_tail && rec->src->tail) {
            rec->src->tail->is_face_up = false;
        }
        card_list_move_sublist(rec->dest, rec->src, rec->card_head);
    } 
    else if (rec->type == MOVE_TYPE_DRAW) {
        for (uint32_t i = 0; i < rec->card_count; i++) {
            Card *c = card_list_pop_tail(rec->dest);
            if (c) {
                c->is_face_up = false;
                card_list_push_tail(rec->src, c);
            }
        }
    } 
    else if (rec->type == MOVE_TYPE_RECYCLE) {
        /* Put the deck back into the waste, flipping face up */
        while (rec->dest->count > 0) {
            Card *c = card_list_pop_tail(rec->dest);
            if (c) {
                c->is_face_up = true;
                card_list_push_tail(rec->src, c);
            }
        }
    }

    /* Revert Score */
    state->score -= rec->score_delta;
    if (state->score < 0) state->score = 0;
    if (state->score_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
        gtk_label_set_text(GTK_LABEL(state->score_label), buf);
    }

    /* Pop and free record */
    state->history.tail = rec->prev;
    if (state->history.tail) {
        state->history.tail->next = NULL;
    } else {
        state->history.head = NULL;
    }
    state->history.count--;
    g_free(rec);

    if (state->undo_button && state->history.count == 0) {
        gtk_widget_set_sensitive(state->undo_button, FALSE);
    }
}