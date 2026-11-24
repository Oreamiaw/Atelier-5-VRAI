#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>

/* ================================================================== */
/*  CONSTANTES                                                         */
/* ================================================================== */

/* Monde */
#define WIDTH               60
#define HEIGHT              20
#define CELL_COUNT          (WIDTH * HEIGHT)

/* Codage d'une cellule : unité = état actuel, dizaine = état futur */
#define DEAD                0
#define ALIVE               1
#define STATE_BASE          10

/* Règles du jeu de la vie (voisinage de 1 case autour de la cellule) */
#define NEIGHBOUR_RANGE     1
#define SURVIVE_MIN         2
#define SURVIVE_MAX         3
#define BIRTH_MAX           3
#define BIRTH_MIN_NORMAL    3   /* naissance normale : 3 voisines            */
#define BIRTH_MIN_RADIO     2   /* herbe fluo : naissance dès 2 voisines     */

/* Troupeau de départ */
#define SHEEP_MIN           60
#define SHEEP_MAX           120

/* Durée de la partie (1 tour = 100 jours = 100 générations) */
#define DAYS_PER_TURN       100
#define SURVIVAL_DAYS       500
#define CARNIVORE_END_DAY   400

/* Effets des événements */
#define EAT_NORMAL          1   /* moutons mangés par jour                   */
#define EAT_CARNIVORE       2
#define FORTIFIER_DIVISOR   4   /* le fortifiant tue un quart du troupeau    */
#define EPIDEMIC_DIVISOR    3   /* l'abattage préventif tue un tiers         */
#define EPIDEMIC_DEATHS     3   /* morts par jour si on laisse l'épidémie    */
#define INVINCIBLE_DAYS     50  /* durée de l'effet du laser                 */

/* Événements */
#define EVENT_RADIO         0
#define EVENT_FORTIFIER     1
#define EVENT_CARNIVORE     2
#define EVENT_ALIENS        3
#define EVENT_EPIDEMIC      4
#define NB_EVENTS           5
#define QUESTION_LEN        400

/* Entrées / sorties */
#define ANSWER_LEN          16
#define ANSWER_YES          "oui"
#define ANSWER_NO           "non"
#define CHAR_DEAD           '.'
#define CHAR_SHEEP          '#'
#define CHAR_PROTECTED      '@'
#define ANSI_CLEAR          "\033[2J\033[3J\033[H"

/* Textes des questions, dans l'ordre des numéros d'événements */
static const char QUESTIONS[NB_EVENTS][QUESTION_LEN] = {
    "Vous voyez une parcelle avec de l'herbe vert fluo, c'est peut-être radioactif, "
    "ou peut-être juste EXTREMEMENT naturel. Autorisez-vous vos moutons à y prendre de l'herbe ?",

    "Vous trouvez une caisse de bouteilles avec marqué fortifiant pour moutons, "
    "voulez-vous leur en donner ?",

    "Voulez-vous devenir carnivore et manger 2 moutons par jour ? "
    "La partie se finira alors à 400 jours.",

    "Des aliens arrivent et vous proposent de tester un laser miracle sur vos moutons. "
    "Ils ont l'air louche mais vous sourient de façon joviale ^^. "
    "Voulez-vous leur faire confiance et passer vos moutons au laser ?",

    "Une épidémie touche une petite partie de vos moutons, voulez-vous tuer un tiers "
    "du troupeau pour éviter qu'elle ne se développe ? "
    "(3 moutons mourront chaque jour si vous ne le faites pas)"
};

/* ================================================================== */
/*  ÉTAT DE LA PARTIE                                                  */
/* ================================================================== */

static int  world[HEIGHT][WIDTH];
static int  day;                    /* jours écoulés                          */
static int  end_day;                /* jour de la fin de partie               */
static int  eat_per_day;            /* moutons mangés par jour                */
static int  birth_min;              /* voisines nécessaires pour une naissance*/
static int  invincible_days;        /* jours restants d'invincibilité         */
static bool epidemic_active;        /* l'épidémie tue chaque jour             */
static bool event_used[NB_EVENTS];  /* un événement n'apparaît qu'une fois    */
static char answer[ANSWER_LEN];     /* dernière ligne saisie par le joueur    */

/* ================================================================== */
/*  LOGIQUE DU JEU (aucun affichage, aucune saisie)                    */
/* ================================================================== */

static bool cell_cur_state(int x, int y)
{
    return world[y][x] % STATE_BASE == ALIVE;
}

static bool cell_next_state(int x, int y)
{
    return world[y][x] / STATE_BASE == ALIVE;
}

static int count_sheep(void)
{
    int count = 0;

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (cell_cur_state(x, y))
                count++;
    return count;
}

/* Voisines vivantes de (x, y) : monde torique */
static int count_neighbours(int x, int y)
{
    int count = 0;

    for (int dy = -NEIGHBOUR_RANGE; dy <= NEIGHBOUR_RANGE; dy++) {
        for (int dx = -NEIGHBOUR_RANGE; dx <= NEIGHBOUR_RANGE; dx++) {
            if (dx == 0 && dy == 0)
                continue;
            if (cell_cur_state((x + dx + WIDTH) % WIDTH, (y + dy + HEIGHT) % HEIGHT))
                count++;
        }
    }
    return count;
}

/* Phase 1 pour une cellule : calcul de l'état futur (dizaine) */
static void cell_compute_next_state(int x, int y)
{
    int  neighbours = count_neighbours(x, y);
    bool alive      = cell_cur_state(x, y);
    bool next;

    if (alive)
        next = (neighbours >= SURVIVE_MIN && neighbours <= SURVIVE_MAX);
    else
        next = (neighbours >= birth_min && neighbours <= BIRTH_MAX);

    if (alive && invincible_days > 0)
        next = true;                    /* sous le laser, rien ne meurt */

    world[y][x] = world[y][x] % STATE_BASE + (next ? STATE_BASE : DEAD);
}

/* Phase 1 : calcul de l'état futur de toutes les cellules */
static void compute_next_generation(void)
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            cell_compute_next_state(x, y);
}

/* Phase 2 : le futur devient l'actuel */
static void apply_next_generation(void)
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            world[y][x] = cell_next_state(x, y) ? ALIVE : DEAD;
}

/* Tue n moutons choisis au hasard (aucun si invincibles). Retourne le nombre de morts. */
static int kill_random_sheep(int n)
{
    int cells[CELL_COUNT];
    int count = 0;

    if (invincible_days > 0)
        return 0;

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (cell_cur_state(x, y))
                cells[count++] = y * WIDTH + x;

    for (int i = count - 1; i > 0; i--) {           /* mélange de Fisher-Yates */
        int j   = rand() % (i + 1);
        int tmp = cells[i];
        cells[i] = cells[j];
        cells[j] = tmp;
    }

    if (n > count)
        n = count;
    for (int i = 0; i < n; i++)
        world[cells[i] / WIDTH][cells[i] % WIDTH] = DEAD;
    return n;
}

/* Remet la partie à zéro */
static void reset_game(void)
{
    memset(world, DEAD, sizeof world);
    memset(event_used, false, sizeof event_used);
    day             = 0;
    end_day         = SURVIVAL_DAYS;
    eat_per_day     = EAT_NORMAL;
    birth_min       = BIRTH_MIN_NORMAL;
    invincible_days = 0;
    epidemic_active = false;
}

/* Place un troupeau de taille aléatoire à des positions toutes différentes */
static int place_start_sheep(void)
{
    int count = SHEEP_MIN + rand() % (SHEEP_MAX - SHEEP_MIN + 1);

    for (int placed = 0; placed < count; ) {
        int x = rand() % WIDTH;
        int y = rand() % HEIGHT;
        if (!cell_cur_state(x, y)) {
            world[y][x] = ALIVE;
            placed++;
        }
    }
    return count;
}

/* Tire au hasard un événement pas encore apparu dans cette partie */
static int pick_event(void)
{
    int remaining = 0;
    int target;

    for (int e = 0; e < NB_EVENTS; e++)
        if (!event_used[e])
            remaining++;

    target = rand() % remaining;
    for (int e = 0; e < NB_EVENTS; e++) {
        if (event_used[e])
            continue;
        if (target == 0) {
            event_used[e] = true;
            return e;
        }
        target--;
    }
    return 0;                           /* inatteignable si remaining > 0 */
}

/* Applique la réponse du joueur. Retourne un nombre utile à l'affichage :
 * moutons morts (fortifiant, épidémie) ou jour de fin de partie (carnivore). */
static int apply_event(int event, bool accepted)
{
    switch (event) {
    case EVENT_RADIO:
        if (accepted)
            birth_min = BIRTH_MIN_RADIO;
        return 0;
    case EVENT_FORTIFIER:
        return accepted ? kill_random_sheep(count_sheep() / FORTIFIER_DIVISOR) : 0;
    case EVENT_CARNIVORE:
        if (accepted) {
            eat_per_day = EAT_CARNIVORE;
            end_day = (day + DAYS_PER_TURN > CARNIVORE_END_DAY) ? day + DAYS_PER_TURN
                                                                  : CARNIVORE_END_DAY;
        }
        return end_day;
    case EVENT_ALIENS:
        if (accepted)
            invincible_days = INVINCIBLE_DAYS;
        return 0;
    default:                            /* EVENT_EPIDEMIC */
        if (accepted)
            return kill_random_sheep(count_sheep() / EPIDEMIC_DIVISOR);
        epidemic_active = true;
        return 0;
    }
}

/* Un jour : une génération du jeu de la vie, puis le repas et l'épidémie */
static void simulate_day(void)
{
    compute_next_generation();
    apply_next_generation();

    kill_random_sheep(eat_per_day);
    if (epidemic_active)
        kill_random_sheep(EPIDEMIC_DEATHS);

    if (invincible_days > 0)
        invincible_days--;
    day++;
}

/* ================================================================== */
/*  ENTRÉES / SORTIES                                                  */
/* ================================================================== */

static void clear_screen(void)
{
    printf(ANSI_CLEAR);
    fflush(stdout);
}

/* Lit une ligne dans answer (en minuscules). Retourne false si l'entrée est fermée. */
static bool read_line(void)
{
    int c = getchar();
    int len = 0;

    if (c == EOF)
        return false;
    while (c != '\n' && c != EOF) {
        if (len < ANSWER_LEN - 1)
            answer[len++] = (char)tolower(c);
        c = getchar();
    }
    answer[len] = '\0';
    return true;
}

static void wait_enter(void)
{
    printf("[Entrée pour continuer]");
    fflush(stdout);
    read_line();
}

/* Pose oui/non jusqu'à une réponse valide. Une entrée fermée vaut non. */
static bool ask_yes_no(void)
{
    for (;;) {
        printf("(%s/%s) : ", ANSWER_YES, ANSWER_NO);
        fflush(stdout);
        if (!read_line())
            return false;
        if (strcmp(answer, ANSWER_YES) == 0)
            return true;
        if (strcmp(answer, ANSWER_NO) == 0)
            return false;
        printf("Réponse non reconnue, répondez par %s ou %s.\n", ANSWER_YES, ANSWER_NO);
    }
}

static void show_intro(int sheep)
{
    printf("Bonjour berger, vous venez avec votre troupeau de %d moutons de survivre "
           "à l'apocalypse, vous devez survivre %d jours afin d'obtenir une colonie stable. "
           "Vous mangez %d mouton par jour, alors si vous n'en n'avez plus, vous mourrez. "
           "Bonne chance !\n\n", sheep, SURVIVAL_DAYS, EAT_NORMAL);
}

static void show_world(void)
{
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (!cell_cur_state(x, y))
                putchar(CHAR_DEAD);
            else
                putchar(invincible_days > 0 ? CHAR_PROTECTED : CHAR_SHEEP);
        }
        putchar('\n');
    }
}

static void show_status(void)
{
    printf("Jour %d / %d - Moutons : %d - Vous mangez %d mouton(s) par jour\n\n",
           day, end_day, count_sheep(), eat_per_day);
    show_world();
    putchar('\n');
}

static void show_event_result(int event, bool accepted, int value)
{
    if (!accepted && event != EVENT_EPIDEMIC) {
        printf("Vous refusez : il ne se passe rien.\n\n");
        return;
    }
    switch (event) {
    case EVENT_RADIO:
        printf("Vos moutons broutent l'herbe fluo : ce tour-ci, ils se reproduisent "
               "dès %d voisins.\n\n", BIRTH_MIN_RADIO);
        break;
    case EVENT_FORTIFIER:
        printf("Le fortifiant était bien trop fort : %d moutons meurent.\n\n", value);
        break;
    case EVENT_CARNIVORE:
        printf("Vous devenez carnivore : %d moutons par jour, la partie finira au jour %d.\n\n",
               EAT_CARNIVORE, value);
        break;
    case EVENT_ALIENS:
        printf("Le laser fonctionne : vos moutons ne peuvent plus mourir pendant %d jours.\n\n",
               INVINCIBLE_DAYS);
        break;
    default:
        if (accepted)
            printf("Vous abattez un tiers du troupeau : %d moutons meurent.\n\n", value);
        else
            printf("L'épidémie se propage : %d moutons mourront chaque jour jusqu'à la fin.\n\n",
                   EPIDEMIC_DEATHS);
        break;
    }
}

static void show_defeat(void)
{
    printf("Partie perdue, vous n'avez plus de moutons, vous êtes mort\n");
}

static void show_victory(void)
{
    printf("Bravo, vous avez gagné, votre troupeau est devenu prospère.\n");
}

/* ================================================================== */
/*  DÉROULEMENT D'UNE PARTIE                                           */
/* ================================================================== */

/* Retourne true si le joueur survit jusqu'à la fin de la partie */
static bool play_game(void)
{
    reset_game();
    int start = place_start_sheep();

    clear_screen();
    show_intro(start);
    wait_enter();

    while (day < end_day) {
        birth_min = BIRTH_MIN_NORMAL;           /* l'herbe fluo ne dure qu'un tour */

        int event = pick_event();
        clear_screen();
        show_status();
        printf("%s\n", QUESTIONS[event]);

        bool accepted = ask_yes_no();
        int  value    = apply_event(event, accepted);
        show_event_result(event, accepted, value);

        if (count_sheep() == 0) {
            show_defeat();
            return false;
        }
        wait_enter();

        for (int d = 0; d < DAYS_PER_TURN && day < end_day && count_sheep() > 0; d++)
            simulate_day();

        if (count_sheep() == 0) {
            clear_screen();
            show_status();
            show_defeat();
            return false;
        }
    }

    clear_screen();
    show_status();
    show_victory();
    return true;
}

static void show_replay_question(void)
{
    printf("\nVoulez-vous relancer une partie ? ");
}

int main(void)
{
    bool replay;

    srand((unsigned)time(NULL));
    do {
        play_game();
        show_replay_question();
        replay = ask_yes_no();
    } while (replay);
    return 0;
}