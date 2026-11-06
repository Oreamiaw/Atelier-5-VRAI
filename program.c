#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH  60
#define HEIGHT 20

/* Tableau statique : HEIGHT lignes x WIDTH colonnes, initialisé à 0 (cellules mortes) */
static int world[HEIGHT][WIDTH] = {0};

/*
 * Codage de l'état d'une cellule : dizaine = état futur, unité = état actuel
 *   0  : morte, le reste
 *   1  : vivante, va mourir
 *   10 : morte, va naître
 *   11 : vivante, le reste
 */

/* Retourne true si la cellule (x, y) est vivante actuellement (unité == 1) */
static bool cell_cur_state(int world[HEIGHT][WIDTH], int x, int y)
{
    return world[y][x] % 10 == 1;
}

/* Retourne true si la cellule (x, y) sera vivante à la génération suivante (dizaine == 1) */
static bool cell_next_state(int world[HEIGHT][WIDTH], int x, int y)
{
    return world[y][x] / 10 == 1;
}

/* Place un motif (tableau de coordonnées relatives) à la position (x, y) */
static void place(const int pattern[][2], int n, int x, int y)
{
    for (int i = 0; i < n; i++)
        world[y + pattern[i][1]][x + pattern[i][0]] = 1;
}

static void init_world(void)
{
    /* Structure stable : bloc 2x2 */
    const int block[][2] = { {0,0}, {1,0}, {0,1}, {1,1} };

    /* Oscillateur : clignotant (blinker), période 2 */
    const int blinker[][2] = { {0,0}, {1,0}, {2,0} };

    /* Vaisseau : planeur (glider) */
    const int glider[][2] = { {1,0}, {2,1}, {0,2}, {1,2}, {2,2} };

    place(block,   4, 10, 10);
    place(blinker, 3, 30, 10);
    place(glider,  5, 50, 10);
}

static void display_world(void)
{
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++)
            putchar(cell_cur_state(world, x, y) ? '#' : '.');
        putchar('\n');
    }
}

/* Nombre de voisines vivantes (état ACTUEL) de (x, y) : monde torique,
 * les bords sont recollés (la colonne WIDTH-1 touche la colonne 0, etc.) */
static int count_neighbours(int world[HEIGHT][WIDTH], int x, int y)
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0)
                continue;                       /* on ne se compte pas soi-même */
            int nx = (x + dx + WIDTH)  % WIDTH;
            int ny = (y + dy + HEIGHT) % HEIGHT;
            if (cell_cur_state(world, nx, ny))
                n++;
        }
    }
    return n;
}

/* Définit l'état futur (dizaine) de (x, y) selon l'état actuel de ses voisines */
static void cell_compute_next_state(int world[HEIGHT][WIDTH], int x, int y)
{
    int  n    = count_neighbours(world, x, y);
    bool next = cell_cur_state(world, x, y) ? (n == 2 || n == 3)   /* survie */
                                            : (n == 3);            /* naissance */

    world[y][x] = world[y][x] % 10 + (next ? 10 : 0);
}

/* Phase 1 : calcul de l'état futur de toutes les cellules */
static void compute_next_generation(int world[HEIGHT][WIDTH])
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            cell_compute_next_state(world, x, y);
}

/* Phase 2 : le futur devient l'actuel (la dizaine passe à l'unité) */
static void apply_next_generation(int world[HEIGHT][WIDTH])
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            world[y][x] = cell_next_state(world, x, y) ? 1 : 0;
}

#define GENERATIONS    100   /* nombre de tours à simuler */
#define DISPLAY_EVERY   50   /* affichage tous les 50 tours */
#define DENSITY         25   /* % de cellules vivantes au départ */

/* Départ aléatoire : chaque cellule est vivante avec une probabilité DENSITY % */
static void init_world_random(void)
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            world[y][x] = (rand() % 100 < DENSITY) ? 1 : 0;
}

/*
 * ./life               : départ aléatoire, graine tirée de l'heure
 * ./life <graine>      : départ aléatoire reproductible
 * ./life --structures  : départ avec bloc, clignotant et planeur
 */
int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "--structures") == 0) {
        init_world();
    } else {
        unsigned seed = (argc > 1) ? (unsigned)strtoul(argv[1], NULL, 10)
                                   : (unsigned)time(NULL);
        srand(seed);
        printf("Graine : %u\n\n", seed);
        init_world_random();
    }

    for (int g = 1; g <= GENERATIONS; g++) {
        compute_next_generation(world);
        apply_next_generation(world);
        if (g % DISPLAY_EVERY == 0) {
            printf("Génération %d\n", g);
            display_world();
            putchar('\n');
        }
    }
    return 0;
}