#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define WIDTH  60
#define HEIGHT 20

/* Tableau statique : HEIGHT lignes x WIDTH colonnes, initialisé à 0 (cellules mortes) */
static int world[HEIGHT][WIDTH] = {0};

/* Moutons invincibles (événement "aliens") : ils ne meurent jamais */
static bool invincible[HEIGHT][WIDTH] = {{false}};

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
            putchar(!cell_cur_state(world, x, y) ? '.'
                    : invincible[y][x]           ? '@' : '#');
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

    if (invincible[y][x] && cell_cur_state(world, x, y))
        next = true;                                               /* invincible : ne meurt jamais */

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
#define PAUSE_EVERY     10   /* affichage + pause tous les 10 tours */
#define CARNIVORE_END   40   /* génération à laquelle la partie s'arrête (fin carnivore, 40 jours) */
#define VODKA_DIVISOR    4   /* la vodka tue un quart de la colonie */
#define CANNABIS_FLYERS 50   /* nombre de moutons qui s'envolent après le cannabis */
#define RADIO_BIRTHS     2   /* nombre de nouvelles cases par mouton dans la parcelle radioactive */
#define START_MIN       60   /* nombre minimal de cellules vivantes au départ */
#define START_MAX      120   /* nombre maximal de cellules vivantes au départ */

/* Départ aléatoire : entre START_MIN et START_MAX cellules vivantes (incluses),
 * placées à des positions aléatoires toutes différentes. Retourne leur nombre. */
static int init_world_random(void)
{
    int count = START_MIN + rand() % (START_MAX - START_MIN + 1);

    for (int placed = 0; placed < count; ) {
        int x = rand() % WIDTH;
        int y = rand() % HEIGHT;
        if (!cell_cur_state(world, x, y)) {     /* évite de compter deux fois la même case */
            world[y][x] = 1;
            placed++;
        }
    }
    return count;
}

/* Pose une question fermée et boucle jusqu'à une réponse "oui" ou "non".
 * Retourne true pour oui, false pour non (ou si l'entrée est fermée). */
static bool ask_yes_no(const char *question)
{
    char line[64];

    for (;;) {
        printf("%s (oui/non) : ", question);
        fflush(stdout);
        if (fgets(line, sizeof line, stdin) == NULL)
            return false;                           /* fin de l'entrée : on considère non */

        line[strcspn(line, "\r\n")] = '\0';
        for (char *p = line; *p; p++)
            *p = (char)tolower((unsigned char)*p);

        if (strcmp(line, "oui") == 0 || strcmp(line, "o") == 0)
            return true;
        if (strcmp(line, "non") == 0 || strcmp(line, "n") == 0)
            return false;
        printf("Réponse non reconnue, répondez par oui ou non.\n");
    }
}

/* ------------------------------------------------------------------ */
/* Moutons = cellules vivantes                                          */
/* ------------------------------------------------------------------ */

static int count_sheep(void)
{
    int n = 0;
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (cell_cur_state(world, x, y))
                n++;
    return n;
}

/* Tue jusqu'à n moutons NON invincibles. Si random est faux, ce sont les premiers
 * dans l'ordre de lecture du monde ; sinon ils sont tirés au hasard.
 * Retourne le nombre de moutons morts. */
static int kill_sheep(int n, bool random)
{
    int cells[WIDTH * HEIGHT];
    int count = 0;

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (cell_cur_state(world, x, y) && !invincible[y][x])
                cells[count++] = y * WIDTH + x;

    if (random) {                                   /* mélange de Fisher-Yates */
        for (int i = count - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int tmp = cells[i]; cells[i] = cells[j]; cells[j] = tmp;
        }
    }

    if (n > count)
        n = count;
    for (int i = 0; i < n; i++)
        world[cells[i] / WIDTH][cells[i] % WIDTH] = 0;
    return n;
}

/* Parcelle radioactive : chaque mouton fait naître RADIO_BIRTHS moutons sur des
 * cases mortes voisines. Les voisines sont lues AVANT toute naissance (pas de
 * réaction en chaîne). Retourne le nombre de naissances. */
static int radioactive_births(void)
{
    static bool born[HEIGHT][WIDTH];
    int total = 0;

    memset(born, 0, sizeof born);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (!cell_cur_state(world, x, y))
                continue;

            int free_cells[8][2], nfree = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = (x + dx + WIDTH)  % WIDTH;
                    int ny = (y + dy + HEIGHT) % HEIGHT;
                    if ((dx || dy) && !cell_cur_state(world, nx, ny) && !born[ny][nx]) {
                        free_cells[nfree][0] = nx;
                        free_cells[nfree][1] = ny;
                        nfree++;
                    }
                }
            }
            for (int k = 0; k < RADIO_BIRTHS && nfree > 0; k++) {
                int r = rand() % nfree;
                born[free_cells[r][1]][free_cells[r][0]] = true;
                free_cells[r][0] = free_cells[nfree - 1][0];
                free_cells[r][1] = free_cells[nfree - 1][1];
                nfree--;
                total++;
            }
        }
    }

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (born[y][x])
                world[y][x] = 1;
    return total;
}

/* Laser des aliens : tous les moutons actuels deviennent invincibles */
static int make_invincible(void)
{
    int n = 0;
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (cell_cur_state(world, x, y)) {
                invincible[y][x] = true;
                n++;
            }
    return n;
}

/* ------------------------------------------------------------------ */
/* Événements : un par pause, dans l'ordre de la liste, puis on recommence */
/* ------------------------------------------------------------------ */

#define NB_EVENTS 5

static const char *const event_questions[NB_EVENTS] = {
    "Voulez vous mangez 2 moutons par jour = fin à 40 jours ? (fin carnivore) Neutre",
    "Voulez-vous mettre les moutons dans la parcelle radioactive ?",
    "Vous trouvez une caisse de vodka, voulez-vous en donner à vos moutons "
        "pour qu'ils deviennent plus résistants ?",
    "Des aliens arrivent et veulent tester un laser sur vos moutons. Acceptez-vous ?",
    "Vous trouvez un champ de cannabis, voulez-vous faire fumer les moutons ?",
};

/* Applique l'effet de l'événement accepté. Retourne true si le monde a changé.
 * *last_gen est la dernière génération à simuler (la fin carnivore la raccourcit). */
static bool apply_event(int event, int g, int *last_gen)
{
    int n;

    switch (event) {
    case 0:     /* Neutre : fin carnivore */
        *last_gen = (CARNIVORE_END > g) ? CARNIVORE_END : g;
        printf("Fin carnivore : la partie s'arrêtera à la génération %d.\n", *last_gen);
        return false;
    case 1:     /* Positif : reproduction */
        n = radioactive_births();
        printf("La radioactivité fait son effet : %d nouveaux moutons naissent à côté des autres.\n", n);
        return true;
    case 2:     /* Négatif : un quart de la colonie meurt (les premiers dans le monde) */
        n = count_sheep() / VODKA_DIVISOR;
        if (n == 0 && count_sheep() > 0)
            n = 1;
        n = kill_sheep(n, false);
        printf("La vodka était trop forte : %d moutons meurent.\n", n);
        return true;
    case 3:     /* Positif : invincibles */
        n = make_invincible();
        printf("Le laser rend vos %d moutons invincibles (affichés par @).\n", n);
        return true;
    default:    /* Négatif : les moutons s'envolent */
        n = kill_sheep(CANNABIS_FLYERS, true);
        printf("%d moutons se sentent pousser des ailes et s'envolent vers les cieux.\n", n);
        return true;
    }
}

/* Teste la fin de partie : plus aucun mouton */
static bool all_sheep_dead(void)
{
    if (count_sheep() > 0)
        return false;
    printf("fin de partie, tout vos moutons sont morts\n");
    return true;
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
        int alive = init_world_random();
        printf("Graine : %u, %d cellules vivantes au départ\n\n", seed, alive);
    }

    int last_gen = GENERATIONS;     /* dernière génération à simuler */

    for (int g = 1; g <= last_gen; g++) {
        compute_next_generation(world);
        apply_next_generation(world);

        bool pause = (g % PAUSE_EVERY == 0);
        if (pause || g == last_gen || count_sheep() == 0) {
            printf("Génération %d\n", g);
            display_world();
            printf("Moutons : %d\n\n", count_sheep());
        }
        if (all_sheep_dead())
            return 0;

        if (pause && g < last_gen) {
            /* À chaque pause : un événement, le joueur répond oui ou non */
            int event = (g / PAUSE_EVERY - 1) % NB_EVENTS;
            if (ask_yes_no(event_questions[event])) {
                bool changed = apply_event(event, g, &last_gen);
                if (changed) {
                    putchar('\n');
                    display_world();
                    printf("Moutons : %d\n", count_sheep());
                }
                if (all_sheep_dead())
                    return 0;
            } else {
                printf("Vous refusez : il ne se passe rien.\n");
            }
            putchar('\n');
        }
    }
    return 0;
}