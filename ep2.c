#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define LANES 10

pthread_t **race_track;
pthread_mutex_t **track_mutex;

pthread_barrier_t step_start;
pthread_barrier_t step_end;

struct timespec time_step = { 0, 60000 };

typedef struct {
    int x;
    int y;
} pair;

typedef struct {
    pthread_t id;
    int       speed;
    int       laps_remaining;
    pair      position;
} Biker;

void init_track(int d) {
    race_track = malloc(sizeof(pthread_t*) * d);
    track_mutex = malloc(sizeof(pthread_mutex_t*) * d);

    for (int i = 0; i < d; i++) {
        race_track[i] = malloc(sizeof(pthread_t) * LANES);
        track_mutex[i] = malloc(sizeof(pthread_mutex_t) * LANES);

        for (int j = 0; j < LANES; j++) {
            pthread_mutex_init(&(track_mutex[i][j]), NULL);
        }
    }
}

void free_track(int d) {
    for (int i = 0; i < d; i++) {
        free(race_track[i]);
        free(track_mutex[i]);
    }
    free(race_track);
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

void* cycle(void* arg) {
    Biker *biker = (Biker*) arg;

    while (biker->laps_remaining > 0) {
        pthread_barrier_wait(&step_start);

        choose_speed(biker);
        /*! TODO: move foward in race_track
        */

        nanosleep(&time_step, NULL);
        biker->laps_remaining--;

        pthread_barrier_wait(&step_end);
    }

    return NULL;
}

Biker* create_bikers(int n) {
    Biker *bikers = malloc(sizeof(Biker) * n);
    pthread_t id;

    for (int i = 0; i < n; i++) {
        pthread_create(&id, NULL, &cycle, &bikers[i]);
        bikers[i].id = id;
        bikers[i].speed = 30;
        bikers[i].laps_remaining = n;
    }

    return bikers;
}

// adds bikers to race_track 'randomly' with Fisher-Yates shuffle
void position_bikers(Biker *bikers, int n) {
    for (int i = 0; i < n; i++) {
        int j = (int) rand() % (i+1);
        pair pos = { j / 5, j % 5 };

        if (i != j) {
            race_track[i / 5][i % 5] = race_track[pos.x][pos.y];
        }

        race_track[pos.x][pos.y] = bikers[i].id;

        bikers[i].position = pos;
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

    int d = atoi(argv[1]), n = atoi(argv[2]);
    int laps = n;
    Biker *bikers;

    srand(12345);

    init_track(d);

    pthread_barrier_init(&step_start, NULL, n+1);
    pthread_barrier_init(&step_end, NULL, n+1);

    bikers = create_bikers(n);
    position_bikers(bikers, n);

    while (laps > 0) {
        /*! TODO: it gets stuck on the first barrier sometimes
        */
        pthread_barrier_wait(&step_start); // let threads run
        pthread_barrier_wait(&step_end);   // wait for threads to finish

        laps = bikers[0].laps_remaining;
    }

    for (int i = 0; i < n; i++) {
        pthread_join(bikers[i].id, NULL);
    }

    pthread_barrier_destroy(&step_start);
    pthread_barrier_destroy(&step_end);

    free(bikers);
    free_track(d);

    return 0;
}
