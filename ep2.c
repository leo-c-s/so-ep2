#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LANES 10

struct timespec time_step = { 0, 60000000 }; // 60 milisec

typedef struct {
    int x;
    int y;
} pair;

typedef struct {
    pthread_t id;
    int       speed;
    int       lap;
    pair      position;
    int       moved;
    int       broke;
    int       is_alive;
} Biker;

typedef struct _BikerList {
    Biker* biker;
    long long lap_time;
    int size;
    struct _BikerList *prev;
    struct _BikerList *next;
} BikerList;

pthread_t **pista;
pthread_mutex_t **track_mutex;

pthread_barrier_t step_start;
pthread_barrier_t step_end;

int d, n, n0, t;

Biker *bikers;
BikerList **ranking;
pthread_mutex_t *ranking_mutex;

BikerList *final_ranking;

void init_track() {
    pista = malloc(sizeof(pthread_t*) * d);
    track_mutex = malloc(sizeof(pthread_mutex_t*) * d);

    for (int i = 0; i < d; i++) {
        pista[i] = malloc(sizeof(pthread_t) * LANES);
        track_mutex[i] = malloc(sizeof(pthread_mutex_t) * LANES);

        for (int j = 0; j < LANES; j++) {
            pthread_mutex_init(&(track_mutex[i][j]), NULL);
            pista[i][j] = 0;
        }
    }
}

void free_track() {
    for (int i = 0; i < d; i++) {
        free(pista[i]);
        free(track_mutex[i]);
    }
    free(pista);
    free(track_mutex);
}

void init_ranking() {
    ranking = malloc(sizeof(BikerList*) * 2 * n);
    final_ranking = malloc(sizeof(BikerList));
    ranking_mutex = malloc(sizeof(pthread_mutex_t) * 2 * n);

    for (int i = 0; i < 2 * n; i++) {
        ranking[i] = malloc(sizeof(BikerList));
        ranking[i]->prev = ranking[i]->next = ranking[i];
        ranking[i]->lap_time = 0;
        ranking[i]->size = 0;
        pthread_mutex_init(&ranking_mutex[i], NULL);
    }

    final_ranking->prev = final_ranking->next = NULL;
}

void append_ranking(Biker *biker) {
    BikerList* list = ranking[biker->lap];
    BikerList* new = malloc(sizeof(BikerList));

    new->biker = biker;

    new->prev = list->prev;
    list->prev->next = new;

    new->next = list;
    list->prev = new;

    new->lap_time = t;
    list->size ++;
}

void free_ranking() {
    BikerList *p, *q;
    for (int i = 0; i < 2 * n0; i++) {
        p = ranking[i]->next;
        while (p != ranking[i]) {
            q = p->next;
            free(p);
            p = q;
        }
        free(ranking[i]);
    }
    free(ranking);

    while (final_ranking != NULL) {
        q = final_ranking->next;
        free(final_ranking);
        final_ranking = q;
    }

    free(ranking_mutex);
}

void choose_speed(Biker *biker) {
    switch (biker->speed) {
        case 30:
            biker->speed = rand() % 100 < 80 ? 60 : 30;
            break;
        case 60:
            biker->speed = rand() % 100 < 60 ? 60 : 30;
            break;
    }
}

void breakBiker(Biker* biker) {
    printf("Ciclista de id %lu quebrou na volta %d!\n", biker->id, biker->lap);
    pair pos = biker->position;
    pthread_mutex_lock(&track_mutex[pos.x][pos.y]);
    if (pista[pos.x][pos.y] == biker->id) {
        pista[pos.x][pos.y] = 0;
    }
    pthread_mutex_unlock(&track_mutex[pos.x][pos.y]);
    biker->broke = 1;
}

Biker* get_biker(pthread_t biker_id) {
    for (int i = 0; i < n0; i++) {
        if (bikers[i].id == biker_id) {
            return &bikers[i];
        }
    }
    return NULL;
}

void overtake(Biker *biker) {
    int x0 = biker->position.x, y0 = biker->position.y;
    int x = (x0 + 1) % d, y = y0 + 1;
    int tries = 0;
    struct timespec wait = { 0, 1000000 }; // 1 milisec

    while (y < LANES && !biker->moved && tries < 5) {
        if (pthread_mutex_trylock(&track_mutex[x][y]) == 0) {
            if (pista[x][y] == 0) {
                // leave current position
                pthread_mutex_lock(&track_mutex[x0][y0]);
                if (pista[x0][y0] == biker->id) {
                    pista[x0][y0] = 0;
                }
                pthread_mutex_unlock(&track_mutex[x0][y0]);

                // go to next position
                pista[x][y] = biker->id;
                pthread_mutex_unlock(&track_mutex[x][y]);
                biker->position.x = x;
                biker->position.y = y;
                biker->moved = 1;
            } else if (get_biker(pista[x][y])->moved) {
                pthread_mutex_unlock(&track_mutex[x][y]);
                y++;
            } else {
                pthread_mutex_unlock(&track_mutex[x][y]);
                nanosleep(&wait, NULL);
            }
        } else {
            tries ++;
            nanosleep(&wait, NULL);
        }
    }

    // if failed to move
    if (y == LANES || tries >= 5) {
        biker->moved = 1;
    }
}

void move(Biker *biker) {
    int x0 = biker->position.x, y0 = biker->position.y;
    int x = (x0 + 1) % d, y = y0;
    struct timespec wait = { 0, 1000000 }; // 1 milisec
    int tries = 0;

    while (!biker->moved && tries < 10) {
        if (pthread_mutex_lock(&track_mutex[x][y]) == 0) {

            if (pista[x][y] == 0) {
                // leave current position
                pthread_mutex_lock(&track_mutex[x0][y0]);
                if (pista[x0][y0] == biker->id) {
                    pista[x0][y0] = 0;
                }
                pthread_mutex_unlock(&track_mutex[x0][y0]);

                // go to next position
                pista[x][y] = biker->id;
                pthread_mutex_unlock(&track_mutex[x][y]);
                biker->position.x = x;
                biker->moved = 1;
            } else if (get_biker(pista[x][y])->moved) {
                pthread_mutex_unlock(&track_mutex[x][y]);
                if (biker->speed != 30) {
                    overtake(biker);
                }
                else {
                    biker->moved = 1;
                }
            } else {
                pthread_mutex_unlock(&track_mutex[x][y]);
                nanosleep(&wait, NULL);
                tries ++;
            }
        } else {
            nanosleep(&wait, NULL);
            tries ++;
        }
    }

    if (tries >= 10) {
        biker->moved = 1;
    }

    // if moved and finished lap
    if (biker->moved && biker->position.x == 0 && biker->position.x != x0) {
        if ((biker->lap + 1) % 6 == 0 && n > 5 && rand() % 20 == 0) {
            breakBiker(biker);
        }

        pthread_mutex_lock(&ranking_mutex[biker->lap]);
        append_ranking(biker);
        pthread_mutex_unlock(&ranking_mutex[biker->lap]);

        biker->lap ++;
    }
}

void* cycle(void* arg) {
    Biker *biker = (Biker*) arg;

    while (n > 0) {
        biker->moved = 0;
        pthread_barrier_wait(&step_start);
        if (!biker->is_alive) {
            break;
        }

        if (biker->lap != 0) {
            choose_speed(biker);
        }

        if (biker->speed != 30 || t % 2) {
            move(biker);
        } else {
            biker->moved = 1;
        }

        pthread_barrier_wait(&step_end);
    }

    pthread_exit(0);
}

Biker* create_bikers() {
    Biker *bikers = malloc(sizeof(Biker) * n);
    pthread_t id = 0;

    for (int i = 0; i < n; i++) {
        bikers[i].speed = 30;
        bikers[i].lap = 0;
        bikers[i].moved = 0;
        bikers[i].broke = 0;
        bikers[i].is_alive = 1;
        bikers[i].position = (pair) { 0, 0 };
        pthread_create(&id, NULL, &cycle, &bikers[i]);
        bikers[i].id = id;
    }

    return bikers;
}

// adds bikers to race_track 'randomly' with Fisher-Yates shuffle
void position_bikers(Biker *bikers) {
    for (int i = 0; i < n; i++) {
        int j = (int) rand() % (i+1);
        pair pos = { j / 5, j % 5 };

        if (i != j) {
            pista[i / 5][i % 5] = pista[pos.x][pos.y];
            get_biker(pista[pos.x][pos.y])->position = (pair) { i / 5, i % 5 };
        }

        pista[pos.x][pos.y] = bikers[i].id;

        bikers[i].position = pos;
    }
}

void print_race_state() {
    int rank = 1;
    int empty = 1;

    for (int i = d - 1; i >= 0; i--) {
        for (int j = 0; j < LANES; j++) {
            if (pista[i][j] && get_biker(pista[i][j])->is_alive) {
                printf("thread %lu is in rank %d and position %d %d\n",
                        pista[i][j],
                        rank,
                        i,
                        j);
                empty = 0;
            }
        }
        if (!empty) {
            rank++;
        }
    }
}

void print_lap_ranking(int lap) {
    BikerList *list = ranking[lap];
    int rank = 1;
    int t0 = list->next->lap_time;

    for (BikerList *p = list->next; p != list; p = p->next) {
        if (p->lap_time > t0) {
            t0 = p->lap_time;
            rank++;
        }

        printf("biker %lu finished lap %d in rank %d\n",
                p->biker->id, lap, rank);
    }
    printf("\n");
}

void print_final_ranking() {
    int rank = 1;

    for (BikerList *p = final_ranking->next; p != NULL; p = p->next) {
        printf("rank %d, biker %lu\n", rank, p->biker->id);
        rank ++;
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <d> <n>\n"
               "    d - length of race track\n"
               "    n - number of bikers\n",
               argv[0]);
        return 0;
    }

    int debug = 0;
    if (argc == 4) {
    printf("argv[3] == %s\n", argv[3]);
        debug = strncmp(argv[3], "debug", 5) == 0;
    }
    d = atoi(argv[1]);
    n0 = n = atoi(argv[2]);
	int last_finished_lap = 0;

    srand(12345);

    init_track();
    init_ranking();

    pthread_barrier_init(&step_start, NULL, n+1);
    pthread_barrier_init(&step_end, NULL, n+1);

    bikers = create_bikers();
    position_bikers(bikers);

    unsigned long eliminated = 0;
    t = 0;
    while (n > 0) {
        if (debug) {
            print_race_state();
        }

        pthread_barrier_wait(&step_start); // let threads run
        // kill thread of biker that was eliminated in the previous iteration
        if (eliminated) {
            pthread_join(eliminated, NULL);
        }
        pthread_barrier_destroy(&step_start);
        pthread_barrier_init(&step_start, NULL, n+1);

        pthread_barrier_wait(&step_end); // wait for threads to finish

        if (ranking[last_finished_lap]->size == n) {
            if (last_finished_lap % 2) {
                Biker *b = ranking[last_finished_lap]->prev->biker;
                b->is_alive = 0;
                eliminated = b->id;
                pista[b->position.x][b->position.y] = 0;
                n--;
                
                BikerList *new = malloc(sizeof(BikerList));
                new->lap_time = t;
                new->biker = b;
                new->prev = final_ranking;
                new->next = final_ranking->next;
                final_ranking->next = new;
            }
            print_lap_ranking(last_finished_lap);
            last_finished_lap ++;
        }

        pthread_barrier_destroy(&step_end);
        pthread_barrier_init(&step_end, NULL, n+1);

        t++;
    }

    for (int i = 0; i < n0; i++) {
        if (bikers[i].id == eliminated) {
            printf("biker %lu won\n", bikers[i].id);
            pthread_join(bikers[i].id, NULL);
        }
    }

    print_final_ranking();

    pthread_barrier_destroy(&step_start);
    pthread_barrier_destroy(&step_end);

    free(bikers);
    free_track();
    free_ranking();

    return 0;
}
