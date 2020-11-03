#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
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

pthread_t **pista;
pthread_mutex_t **track_mutex;

pthread_barrier_t step_start;
pthread_barrier_t step_end;

pthread_mutex_t mutexVolta;

int d, n, n0, terminouVolta;

Biker *bikers;

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
    printf("Ciclista de id %lu quebrou na volta %d!\n",
            biker->id,
            n - biker->lap);
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

                //printf("thread %lu will move to (%d, %d)\n", biker->id, x, y);
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

    while (!biker->moved) {
        if (pthread_mutex_trylock(&track_mutex[x][y]) == 0) {
            //printf("thread %lu locked position (%d, %d)\n", biker->id,x,y);
            if (pista[x][y] == 0) {
                // leave current position
                pthread_mutex_lock(&track_mutex[x0][y0]);
                if (pista[x0][y0] == biker->id) {
                    pista[x0][y0] = 0;
                }
                pthread_mutex_unlock(&track_mutex[x0][y0]);

                //printf("thread %lu will move to (%d, %d)\n", biker->id, x, y);
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
            }
        } else {
            nanosleep(&wait, NULL);
        }
    }

    // if moved and finished lap
    if (biker->moved && biker->position.x == 0) {
        biker->lap ++;
		pthread_mutex_lock(&mutexVolta);
		terminouVolta = 1;
		pthread_mutex_unlock(&mutexVolta);
        if (biker->lap % 6 == 0 && n > 5 && rand() % 20 == 0) {
            breakBiker(biker);
        }
    }
}

void* cycle(void* arg) {
    Biker *biker = (Biker*) arg;

    while (n > 0) {
        biker->moved = 0;
        pthread_barrier_wait(&step_start);
        if (!biker->is_alive) {
            pthread_exit(NULL);
        }

        if (biker->lap != 1) {
            choose_speed(biker);
        }
        move(biker);

        nanosleep(&time_step, NULL);
        pthread_barrier_wait(&step_end);
    }

    pthread_exit(0);
}

Biker* create_bikers() {
    Biker *bikers = malloc(sizeof(Biker) * n);
    pthread_t id = 0;

    for (int i = 0; i < n; i++) {
        bikers[i].speed = 30;
        bikers[i].lap = 1;
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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <d> <n>\n"
               "    d - length of race track\n"
               "    n - number of bikers\n",
               argv[0]);
        return 0;
    }

    d = atoi(argv[1]);
    n0 = n = atoi(argv[2]);
	terminouVolta = 0;
	int eliminados, lap = 1;

    printf("n = %d e d = %d\n", n, d);
    srand(12345);

    init_track();

    pthread_barrier_init(&step_start, NULL, n+1);
    pthread_barrier_init(&step_end, NULL, n+1);

    bikers = create_bikers();
    position_bikers(bikers);

    unsigned long eliminated = 0;
    while (n > 0) {
        print_race_state();

        /*! TODO: it gets stuck on the first barrier sometimes
        */
        if (terminouVolta) {
            lap++;
            printf("on lap %d\n", lap);
        }
		
		eliminados = 0;
		terminouVolta = 0;

        pthread_barrier_wait(&step_start); // let threads run
        // kill thread of biker that was eliminated in the previous iteration
        if (eliminated) {
            pthread_join(eliminated, NULL);
        }
        pthread_barrier_destroy(&step_start);
        pthread_barrier_init(&step_start, NULL, n+1);
        //printf("Vai esperar segunda barreira\n");
        pthread_barrier_wait(&step_end); // wait for threads to finish
        //printf("Passou da segunda\n");

        int last_pos = d - 1, last_lap = n;
        for (int i = 0; i < n0; i++) {
            if (bikers[i].is_alive &&
                    bikers[i].position.x < last_pos &&
                    bikers[i].lap < last_lap) {
                last_pos = bikers[i].position.x;
            }
        }

        int tied = 0;
        for (int i = 0; i < LANES; i++) {
            if (pista[last_pos][i]) {
                Biker *b = get_biker(pista[last_pos][i]);
                
                if (b->is_alive && b->lap == last_lap) {
                    tied++;
                }
            }
        }

        if (last_lap % 2 == 0 && last_pos == d - 1) {
            int eliminate = tied != 0 ? (int) rand() % tied : 0;
            for (int i = 0; i < LANES; i++) {
                if (pista[last_pos][i] != 0) {
                    Biker *b = get_biker(pista[last_pos][i]);
                    if (b->is_alive && eliminate == 0 && terminouVolta != 0) {
                        printf("biker %lu was eliminated\n",
                                b->id);
                        eliminated = b->id;
                        pista[last_pos][i] = 0;
                        b->is_alive = 0;
                        eliminados = 1;
                        break;
                    } else {
                        eliminate--;
                    }
                }
            }
        }

        if(eliminados != 0) n--;

        pthread_barrier_destroy(&step_end);
        pthread_barrier_init(&step_end, NULL, n+1);
    }
    //pthread_barrier_wait(&step_start);
    for (int i = 0; i < n0; i++) {
        if (bikers[i].id == eliminated) {
            printf("biker %lu won\n", bikers[i].id);
            pthread_join(bikers[i].id, NULL);
        }
    }

    // pthread_barrier_destroy(&step_start);
    // pthread_barrier_destroy(&step_end);

    free(bikers);
    free_track(d);

    return 0;
}
