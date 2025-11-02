#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>

#define MAX_PATH 100
#define MAX_MONSTERS 50
#define MAX_HEROES 10
#define MAX_LINE 256

// estructura para posicion
typedef struct
{
    int x;
    int y;
} coords;

// configuración de heroe y monstros
typedef struct
{
    int id;
    int hp;
    int dmg;
    int range;
    coords start;
    coords current;
    coords path[MAX_PATH];
    bool combat;
    bool alive;
    int path_length;
    pthread_t thread;
    int index;
} hero;

typedef struct
{
    int id;
    int hp;
    int dmg;
    int range;
    int vision;
    coords current;
    bool alert;
    bool alive;
    pthread_t thread;
} monster;

typedef struct
{
    int ancho;
    int largo;
    hero heroes[MAX_HEROES];
    int contador_heroes;
    monster monstruos[MAX_MONSTERS];
    int contador_monstruos;
    bool turno_heroes;
    int heroes_listos;
    int heroes_esperando_sync;
    int monstruos_listos;
    bool juego_terminado;
} GameState;

GameState game;
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turno_cond = PTHREAD_COND_INITIALIZER;

void config(const char *nombreArchivo)
{
    FILE *file = fopen(nombreArchivo, "r");
    if (!file)
    {
        perror("Error al abrir el archivo");
        exit(1);
    }

    char line[MAX_LINE];
    game.contador_heroes = 0;
    game.contador_monstruos = 0;

    int current_hero = -1;

    while (fgets(line, sizeof(line), file))
    {
        char key[50];
        sscanf(line, "%s", key);
        //leer tablero
        if (strcmp(key, "GRID_SIZE") == 0)
        {
            sscanf(line, "%*s %d %d", &game.ancho, &game.largo);
        }
        //leer heroes
        else if (strcmp(key, "HERO_COUNT") == 0)
        {
            sscanf(line, "%*s %d", &game.contador_heroes);
        }
        else if (strncmp(key, "HERO_", 5) == 0)
        {
            // Verificar si es uno o más heroes
            int hero_num = 0;
            char rest[50];
            if (sscanf(key, "HERO_%d_%s", &hero_num, rest) == 2)
            {
                // Formato HERO_X_PROPERTY 
                hero_num--;
                current_hero = hero_num;

                if (hero_num >= 0 && hero_num < MAX_HEROES)
                {
                    if (hero_num >= game.contador_heroes)
                        game.contador_heroes = hero_num + 1;

                    game.heroes[hero_num].id = hero_num;
                    game.heroes[hero_num].alive = true;
                    game.heroes[hero_num].combat = false;
                    game.heroes[hero_num].index = 0;
                    game.heroes[hero_num].path_length = 0;

                    if (strcmp(rest, "HP") == 0)
                        sscanf(line, "%*s %d", &game.heroes[hero_num].hp);
                    else if (strcmp(rest, "ATTACK_DAMAGE") == 0)
                        sscanf(line, "%*s %d", &game.heroes[hero_num].dmg);
                    else if (strcmp(rest, "ATTACK_RANGE") == 0)
                        sscanf(line, "%*s %d", &game.heroes[hero_num].range);
                    else if (strcmp(rest, "START") == 0)
                    {
                        sscanf(line, "%*s %d %d", &game.heroes[hero_num].current.x,
                               &game.heroes[hero_num].current.y);
                        game.heroes[hero_num].start = game.heroes[hero_num].current;
                    }
                    else if (strcmp(rest, "PATH") == 0)
                    {
                        char *ptr = strchr(line, '(');
                        while (ptr && game.heroes[hero_num].path_length < MAX_PATH)
                        {
                            int x, y;
                            if (sscanf(ptr, "(%d,%d)", &x, &y) == 2)
                            {
                                game.heroes[hero_num].path[game.heroes[hero_num].path_length].x = x;
                                game.heroes[hero_num].path[game.heroes[hero_num].path_length].y = y;
                                game.heroes[hero_num].path_length++;
                            }
                            ptr = strchr(ptr + 1, '(');
                        }
                    }
                }
            }
            else
            {
                current_hero = 0;
                game.contador_heroes = 1;
                game.heroes[0].id = 0;
                game.heroes[0].alive = true;
                game.heroes[0].combat = false;
                game.heroes[0].index = 0;

                if (strcmp(key, "HERO_HP") == 0)
                {
                    sscanf(line, "%*s %d", &game.heroes[0].hp);
                }
                else if (strcmp(key, "HERO_ATTACK_DAMAGE") == 0)
                {
                    sscanf(line, "%*s %d", &game.heroes[0].dmg);
                }
                else if (strcmp(key, "HERO_ATTACK_RANGE") == 0)
                {
                    sscanf(line, "%*s %d", &game.heroes[0].range);
                }
                else if (strcmp(key, "HERO_START") == 0)
                {
                    sscanf(line, "%*s %d %d", &game.heroes[0].current.x, &game.heroes[0].current.y);
                    game.heroes[0].start = game.heroes[0].current;
                }
                else if (strcmp(key, "HERO_PATH") == 0)
                {
                    game.heroes[0].path_length = 0;
                    char *ptr = strchr(line, '(');
                    while (ptr && game.heroes[0].path_length < MAX_PATH)
                    {
                        int x, y;
                        if (sscanf(ptr, "(%d,%d)", &x, &y) == 2)
                        {
                            game.heroes[0].path[game.heroes[0].path_length].x = x;
                            game.heroes[0].path[game.heroes[0].path_length].y = y;
                            game.heroes[0].path_length++;
                        }
                        ptr = strchr(ptr + 1, '(');
                    }
                }
            }
        }
        //leer monstros
        else if (strcmp(key, "MONSTER_COUNT") == 0)
        {
            sscanf(line, "%*s %d", &game.contador_monstruos);
        }
        else if (strncmp(key, "MONSTER_", 8) == 0)
        {
            int monster_id;
            char property[30];
            sscanf(key, "MONSTER_%d_%s", &monster_id, property);
            monster_id--;

            if (monster_id >= 0 && monster_id < MAX_MONSTERS)
            {
                game.monstruos[monster_id].id = monster_id;
                game.monstruos[monster_id].alive = true;
                game.monstruos[monster_id].alert = false;

                if (strcmp(property, "HP") == 0)
                    sscanf(line, "%*s %d", &game.monstruos[monster_id].hp);
                else if (strcmp(property, "ATTACK_DAMAGE") == 0)
                    sscanf(line, "%*s %d", &game.monstruos[monster_id].dmg);
                else if (strcmp(property, "VISION_RANGE") == 0)
                    sscanf(line, "%*s %d", &game.monstruos[monster_id].vision);
                else if (strcmp(property, "ATTACK_RANGE") == 0)
                    sscanf(line, "%*s %d", &game.monstruos[monster_id].range);
                else if (strcmp(property, "COORDS") == 0)
                    sscanf(line, "%*s %d %d",
                           &game.monstruos[monster_id].current.x,
                           &game.monstruos[monster_id].current.y);
            }
        }
    }

    game.turno_heroes = false;
    game.heroes_listos = 0;
    game.heroes_esperando_sync = 0;
    game.monstruos_listos = 0;
    game.juego_terminado = false;

    fclose(file);
}

// Funciones auxiliares
double calcular_distancia(coords a, coords b)
{
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

void alertar_monstruos_cercanos(int monster_id)
{
    monster *alerter = &game.monstruos[monster_id];

    printf("  [Monstruo %d] ¡Sapeando a aliados cercanos!\n", monster_id + 1);

    for (int i = 0; i < game.contador_monstruos; i++)
    {
        if (i == monster_id || !game.monstruos[i].alive)
            continue;

        double dist = calcular_distancia(alerter->current, game.monstruos[i].current);
        if (dist <= alerter->vision && !game.monstruos[i].alert)
        {
            game.monstruos[i].alert = true;
            printf("  [Monstruo %d] ¡Monstruo %d ha sido alertado!\n",
                   monster_id + 1, i + 1);
        }
    }
}

int verificar_estado()
{       // 0 no hay alertas, 1, si lol
    for (int i = 0; i < game.contador_monstruos; i++)
    {
        if (game.monstruos[i].alive && game.monstruos[i].alert)
        {
            return 1;
        }
    }
    return 0;
}

void verificar_vision_monstruos(int hero_id)
{
    hero *h = &game.heroes[hero_id];

    for (int i = 0; i < game.contador_monstruos; i++)
    {
        if (!game.monstruos[i].alive)
            continue;

        double dist = calcular_distancia(h->current, game.monstruos[i].current);

        if (dist <= game.monstruos[i].vision && !game.monstruos[i].alert)
        {
            printf("  [Monstruo %d] ¡HÉROE %d DETECTADO a distancia %.1f!\n",
                   i + 1, hero_id + 1, dist);
            game.monstruos[i].alert = true;
            alertar_monstruos_cercanos(i);
        }
    }
}

coords encontrar_heroe_mas_cercano(monster *m)
{
    double min_dist = 999999;
    coords objetivo = m->current;

    for (int i = 0; i < game.contador_heroes; i++)
    {
        if (!game.heroes[i].alive)
            continue;

        double dist = calcular_distancia(m->current, game.heroes[i].current);
        if (dist < min_dist)
        {
            min_dist = dist;
            objetivo = game.heroes[i].current;
        }
    }

    return objetivo;
}

void mover_hacia_heroe(monster *m)
{
    coords objetivo = encontrar_heroe_mas_cercano(m);
    coords old_pos = m->current;

    if (m->current.x < objetivo.x)
        m->current.x++;
    else if (m->current.x > objetivo.x)
        m->current.x--;

    if (m->current.y < objetivo.y)
        m->current.y++;
    else if (m->current.y > objetivo.y)
        m->current.y--;

    double new_dist = calcular_distancia(objetivo, m->current);
    printf("  [Monstruo %d] Se mueve de (%d,%d) a (%d,%d) | Distancia: %.1f\n",
           m->id + 1, old_pos.x, old_pos.y, m->current.x, m->current.y, new_dist);
}

void *heroe_thread(void *arg)
{
    int id = *(int *)arg;//dado que esta en un trhead
    hero *h = &game.heroes[id];//obtiene el id del trhead

    printf("\n[HÉROE %d] Iniciando aventura desde (%d, %d)!\n",
           id + 1, h->current.x, h->current.y);
    sleep(1);

    // Verificación inicial
    pthread_mutex_lock(&game_mutex);//bloqueo de verificar estados para ver un monstro lo detecto
    verificar_vision_monstruos(id);
    if (verificar_estado())
    {
        h->combat = true;
        printf("[HÉROE %d] ¡Monstruos detectados desde el inicio, its a trap!\n", id + 1);
    }
    pthread_mutex_unlock(&game_mutex);

    while (h->alive && h->index < h->path_length)
    {
        // === FASE 1: ESPERAR A QUE SEA EL TURNO DE LOS HÉROES ===
        pthread_mutex_lock(&game_mutex);
        while (game.turno_heroes && h->alive)
        {
            pthread_cond_wait(&turno_cond, &game_mutex);
        }
        pthread_mutex_unlock(&game_mutex);

        if (!h->alive)
            break;

        // === FASE 2: ACTUAR (MOVER O ATACAR) ===
        pthread_mutex_lock(&game_mutex);

        printf("\n--- TURNO DEL HÉROE %d ---\n", id + 1);

        if (!h->combat)
        {
            // Avanzar
            h->current = h->path[h->index];
            printf("[HÉROE %d] Se mueve a (%d, %d) [%d/%d]\n",
                   id + 1, h->current.x, h->current.y,
                   h->index + 1, h->path_length);

            verificar_vision_monstruos(id);
            if (verificar_estado())
            {
                h->combat = true;
                printf("[HÉROE %d] ¡ENTRANDO EN COMBATE!\n", id + 1);
            }
            else
            {
                h->index++;
            }
        }
        else
        {
            // Atacar
            printf("[HÉROE %d] Atacando...\n", id + 1);
            bool ataco = false;
            for (int i = 0; i < game.contador_monstruos; i++)
            {
                if (!game.monstruos[i].alive || !game.monstruos[i].alert)
                    continue;

                double dist = calcular_distancia(h->current, game.monstruos[i].current);
                if (dist <= h->range)
                {
                    game.monstruos[i].hp -= h->dmg;
                    printf("  [HÉROE %d] Ataca a Monstruo %d (Daño: %d, HP restante: %d)\n",
                           id + 1, i + 1, h->dmg, game.monstruos[i].hp);
                    ataco = true;

                    if (game.monstruos[i].hp <= 0)
                    {
                        game.monstruos[i].alive = false;
                        game.monstruos[i].alert = false;
                        printf("  [Monstruo %d] ¡DERROTADO!\n", i + 1);
                    }
                }
            }

            if (!ataco)
            {
                printf("  [HÉROE %d] No hay monstruos en rango\n", id + 1);
            }

            if (!verificar_estado())
            {
                h->combat = false;
                printf("[HÉROE %d] Combate terminado. Continuando ruta...\n", id + 1);
                h->index++;
            }
        }

        // === FASE 3: SEÑALAR QUE TERMINÓ ===
        game.heroes_listos++;

        // Si es el último héroe, activar turno de monstruos
        if (game.heroes_listos >= game.contador_heroes)
        {
            game.turno_heroes = true;
            pthread_cond_broadcast(&turno_cond);
        }

        pthread_mutex_unlock(&game_mutex);

        sleep(1);
    }

    pthread_mutex_lock(&game_mutex);
    if (h->alive)
    {
        printf("\n[HÉROE %d] ¡Aventura completada exitosamente!\n", id + 1);
    }
    else
    {
        printf("\n[HÉROE %d] Ha sido derrotado...\n", id + 1);
    }

    // Verificar si todos los héroes terminaron
    bool todos_terminados = true;
    for (int i = 0; i < game.contador_heroes; i++)
    {
        if (game.heroes[i].alive && game.heroes[i].index < game.heroes[i].path_length)
        {
            todos_terminados = false;
            break;
        }
    }

    if (todos_terminados)
    {
        game.juego_terminado = true;
        pthread_cond_broadcast(&turno_cond);
    }

    pthread_mutex_unlock(&game_mutex);

    return NULL;
}

void *monstruo_thread(void *arg)
{
    int id = *(int *)arg;
    monster *m = &game.monstruos[id];

    while (true)
    {
        pthread_mutex_lock(&game_mutex);

        // Esperar a que sea el turno de los monstruos
        while (!game.turno_heroes && !game.juego_terminado)
        {
            pthread_cond_wait(&turno_cond, &game_mutex);
        }

        if (game.juego_terminado || !m->alive)
        {
            game.monstruos_listos++;

            // Si es el último monstruo, resetear y dar turno a héroes
            if (game.monstruos_listos >= game.contador_monstruos)
            {
                game.monstruos_listos = 0;
                game.heroes_listos = 0;
                game.turno_heroes = false;
                pthread_cond_broadcast(&turno_cond);
            }

            pthread_mutex_unlock(&game_mutex);
            break;
        }

        // Actuar si está alertado
        if (m->alert)
        {
            // Encontrar héroe más cercano
            double min_dist = 999999;
            int target_hero = -1;

            for (int i = 0; i < game.contador_heroes; i++)
            {
                if (!game.heroes[i].alive)
                    continue;

                double dist = calcular_distancia(m->current, game.heroes[i].current);
                if (dist < min_dist)
                {
                    min_dist = dist;
                    target_hero = i;
                }
            }

            if (target_hero != -1)
            {
                if (min_dist > m->range)
                {
                    // Moverse hacia el héroe más cercano
                    mover_hacia_heroe(m);
                }
                else
                {
                    // Atacar al héroe más cercano
                    game.heroes[target_hero].hp -= m->dmg;
                    printf("  [Monstruo %d] Ataca a Héroe %d (Daño: %d, HP héroe: %d)\n",
                           m->id + 1, target_hero + 1, m->dmg, game.heroes[target_hero].hp);

                    if (game.heroes[target_hero].hp <= 0)
                    {
                        game.heroes[target_hero].alive = false;
                        printf("\n  ¡¡¡HÉROE %d HA SIDO DERROTADO!!!\n", target_hero + 1);
                    }
                }
            }
        }

        game.monstruos_listos++;

        // Si es el último monstruo, resetear y dar turno a héroes
        if (game.monstruos_listos >= game.contador_monstruos)
        {
            game.monstruos_listos = 0;
            game.heroes_listos = 0;
            game.turno_heroes = false;
            pthread_cond_broadcast(&turno_cond);
        }

        pthread_mutex_unlock(&game_mutex);

        usleep(100000);
    }

    return NULL;
}

void imprimir_estado_inicial()
{
    printf("<===========ESTADO INICIAL===========>\n");
    printf("<---------------TABLERO-------------->\n");
    printf("Grid: %d x %d \n", game.ancho, game.largo);

    printf("<----------------HEROES-------------->\n");
    printf("Cantidad de héroes: %d\n", game.contador_heroes);
    for (int i = 0; i < game.contador_heroes; i++)
    {
        printf(">Héroe %d:\n", i + 1);
        printf("HP: %d \n", game.heroes[i].hp);
        printf("DMG: %d \n", game.heroes[i].dmg);
        printf("Rango: %d \n", game.heroes[i].range);
        printf("Posición inicial: %d,%d \n", game.heroes[i].current.x, game.heroes[i].current.y);
        printf("Ruta: %d puntos\n", game.heroes[i].path_length);
        printf("\n");
    }

    printf("<--------------MONSTRUOS------------->\n");
    printf("Cantidad de monstruos: %d \n", game.contador_monstruos);
    for (int i = 0; i < game.contador_monstruos; i++)
    {
        printf(">Monstruo %d : HP:%d | DMG:%d | Visión:%d | Rango:%d | Pos:(%d,%d)\n",
               i + 1, game.monstruos[i].hp, game.monstruos[i].dmg,
               game.monstruos[i].vision, game.monstruos[i].range,
               game.monstruos[i].current.x, game.monstruos[i].current.y);
    }
    printf("<====================================>\n");
}

int main()
{
    config("config.txt");
    imprimir_estado_inicial();

    printf("\nIniciando simulación...\n");
    sleep(2);

    // Crear threads
    int hero_ids[MAX_HEROES];
    int monster_ids[MAX_MONSTERS];

    for (int i = 0; i < game.contador_heroes; i++)
    {
        hero_ids[i] = i;
        pthread_create(&game.heroes[i].thread, NULL, heroe_thread, &hero_ids[i]);
    }

    for (int i = 0; i < game.contador_monstruos; i++)
    {
        monster_ids[i] = i;
        pthread_create(&game.monstruos[i].thread, NULL, monstruo_thread, &monster_ids[i]);
    }

    // Esperar a que terminen
    for (int i = 0; i < game.contador_heroes; i++)
    {
        pthread_join(game.heroes[i].thread, NULL);
    }

    for (int i = 0; i < game.contador_monstruos; i++)
    {
        pthread_join(game.monstruos[i].thread, NULL);
    }

    printf("\n<=========SIMULACIÓN TERMINADA=========>\n");
    printf("\nEstado final:\n");

    for (int i = 0; i < game.contador_heroes; i++)
    {
        printf("  Héroe %d: %s (HP: %d)\n",
               i + 1,
               game.heroes[i].alive ? "VIVO" : "MUERTO",
               game.heroes[i].hp > 0 ? game.heroes[i].hp : 0);
    }

    printf("\n");

    for (int i = 0; i < game.contador_monstruos; i++)
    {
        printf("  Monstruo %d: %s (HP: %d)\n",
               i + 1,
               game.monstruos[i].alive ? "VIVO" : "MUERTO",
               game.monstruos[i].hp > 0 ? game.monstruos[i].hp : 0);
    }

    pthread_mutex_destroy(&game_mutex);
    pthread_cond_destroy(&turno_cond);

    return 0;
}