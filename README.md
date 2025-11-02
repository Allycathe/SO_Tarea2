# Tarea 2 sistemas operativos
## Doom que no es doom

### Intrucciones de uso
1. Colocar archivo de prueba en la carpeta del código, este archivo se debe llamar config.txt.
2. Compilar código mediante:
```bash 
gcc tarea2.c -o tarea2 -lpthread -lm
```
3.  Ejectuar salida mediante 
```bash 
./tarea2
```

### Breves explicaciones del funcionamiento del código
#### Estructs
1. gameState: es la estructura que almacena todo el juego, tiene la grid, heroes, monstruos y variables de sincronización
2. Hero: el heroe de la historia, vida, rango, daño y estado de combate
3. Monster: otra entidad con vida, rango, danó, campo de visión y estado de alerta
#### Leer config
Soporta uno o más heroes mediante el prefijo "HERO_HP" ó "HERO_1_HP", máximo 10, puede cambiarse en el #DEFINE.
Soporta múltiple montruos, máximo 50.
Soporta hasta 100 rutas por heroe.
La razón de las limitaciones, es que se usan arreglos para los heroes, monstruos y rutas, y dado que estos requieren ser limitados, al final son números arbitrarios.
#### Sistema de concurrencia y  sincronización
La forma que se me hizo más fácil de hacer, fue con turnos por heroes, todos están en paralelos, pero para hacer una acción deben parar el tiempo (ZA WARUDO) y moverse, para evitar condiciones de carrera, ademas funciona que cada heroe tiene un turno para moverse, y una vez todos listos, los monstruos revisan si es que hay un heroe en su rango, ineficiente si es que hay muchos monstruos.
Como mencione anteriomente, para los monstruos también hay turnos, en caso de estar en rango, el monstro i sapea a los que tenga en rango, mediante la función "calcular_distancia", que alerta a los monstruos en un rango circular.
Finalmente las variables que permiten el funcionamiento son: 
- game_mutex: Mutex global que protege todo el estado del juego
- turno_cond: Variable de condición para coordinar turnos entre héroes y monstruos
- pthread_cond_wait(): Bloquea threads hasta que sea su turno
- pthread_cond_broadcast(): Despierta a todos los threads esperando
#### Mecanicas 
-  Cada monstruo tiene un rango de visión, basado en la distancia euclidiana $sqrt((x2-x1)² + (y2-y1)²)$, cuando un monstruo detecta al heroe, se alerta así mismo(lol) y llama a la función "alertar_monstruos_cercanos()".
- Para los heroes, siempre y cuando el estado de combate sea false, estarán moviendose casilla a casilla verificando el rango de los monstruos
- Los monstruos alertados, si no están en rango de ataque, caminan hasta el heroe.
- El combate continua hasta que todos los monstruos alertados hayan muerto, o en su defecto el heroe
#### Problemas de sincronización
El código evita 3 problemas que suelen venir acompañados del paralelismo
- Race conditions: "pthread_mutex_lock/unlock"
- Deadlocks: Sistema de turnos, orden de ejecución
- Starvation: cada thread tiene su turno

