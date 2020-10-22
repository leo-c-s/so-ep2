#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

pthread_t **race_track;

typedef struct {
    pthread_t id;
    int       speed;
} Biker;


void init_track(int d, int n) {
    race_track = malloc(sizeof(pthread_t*) * d);

    for (int i = 0; i < d; i++) {
        race_track[d] = malloc(sizeof(pthread_t) * 10);
    }
}

void free_track(int d) {
    for (int i = 0; i < d; i++) {
        free(race_track[i]);
    }
}

int choose_speed(int speed) {
    int new_speed;

    switch (speed) {
        case 30:
            new_speed = rand() % 100 < 80 ? 60 : 30;
            break;
        case 60:
            new_speed = rand() % 100 < 60 ? 60 : 30;
            break;
        default:
            new_speed = speed;
            break;
    }

    return new_speed;
}

void* cycle(void* arg) {
    Biker *biker = (Biker*) arg;
    struct timespec sleep_duration;
    sleep_duration.tv_sec = 0;

    switch (biker->speed) {
        case 30:
            sleep_duration.tv_nsec = 120000;
            break;
        case 60:
            sleep_duration.tv_nsec = 60000;
            break;
        default:
            fprintf(stderr, "Error: Invalid speed\n");
            sleep_duration.tv_nsec = 120000;
            break;
    }

    nanosleep(&sleep_duration, NULL);

    return NULL;
}

Biker* create_bikers(int n) {
    Biker *bikers = malloc(sizeof(Biker) * n);
    pthread_t id;

    for (int i = 0; i < n; i++) {
        id = pthread_create(&id, NULL, &cycle, &bikers[i]);
        bikers[i].id = id;
        bikers[i].speed = 30;
    }

    return bikers;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <d> <n>\n"
               "\td - length of race track\n"
               "\tn - number of bikers\n", argv[0]);
    }

    int d = atoi(argv[1]), n = atoi(argv[2]);
    Biker *bikers;

    init_track(d, n);
    bikers = create_bikers(n);
    
    for (int i = 0; i < n; i++) {
        pthread_join(bikers[i].id, NULL);
    }

    free(bikers);
    free_track(d);

    return 0;
}
