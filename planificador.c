#define _POSIX_C_SOURCE 200809L  // para que compilen sigaction, kill, nanosleep y fdopen

// estas son todas las herramientas que necesite
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "planificador.h"


// parte en  10 y main la puede cambiar con el tercer numero
int probabilidad_falla = 10;

volatile sig_atomic_t seremi_llego = 0;
// puedo hacer ctrl + c en cualquier momento (La Seremi), por eso uso el volatile


void llego_la_seremi(int senal) {
    
    (void)senal;
    seremi_llego = 1;
}

void dormir_ms(int ms) {

    struct timespec t;
    t.tv_sec = ms / 1000;
    t.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
}


int esta_lista(int i) {  //  todas las cosas de las que depende ya terminaron bien

    Actividad *a = &actividades[i];
    for (int d = 0; d < a->cuantas_dependencias; d++) {

        if (actividades[a->dependencias[d]].estado != TERMINADA) {
            return 0;
        }
    }
    return 1;
}


// si algo de lo que depende fallo o es abortado, esta actividad nunca se va a poder hacer
int abortar_dependientes(void) {

    int abortadas = 0;
    for (int i = 0; i < cuantas_actividades; i++) {

        Actividad *a = &actividades[i];
        if (a->estado != PENDIENTE) {

            continue;
        }

        for (int d = 0; d < a->cuantas_dependencias; d++) {

            int estado_dependencia = actividades[a->dependencias[d]].estado;

            if (estado_dependencia == FALLIDA || estado_dependencia == ABORTADA) {

                a->estado = ABORTADA;
                printf("[planificador] '%s' se aborta porque '%s' no se pudo hacer\n",
                       a->nombre, actividades[a->dependencias[d]].nombre);
                abortadas++;
                break;
            }
        }
    }
    return abortadas;
}



int contar_pendientes(void) {

    int cuantas = 0;
    for (int i = 0; i < cuantas_actividades; i++) {

        if (actividades[i].estado == PENDIENTE) {
            cuantas++;
        }
    }
    return cuantas;
}



int buscar_por_pid(pid_t pid) {

    for (int i = 0; i < cuantas_actividades; i++) {

        if (actividades[i].estado == CORRIENDO && actividades[i].pid == pid) {
            return i;
        }
    }

    return -1;
}




void trabajo_del_hijo(int i, int por_donde_leo, int por_donde_escribo) {
    // el hijo es un clon del padre asi que hereda la accion del cuando se ejecuta ctrl + c
    signal(SIGINT, SIG_DFL); // lo dejo como viens de fabrica para que simplemente muera


    srand(getpid());  //para que cada hijo tenga un numero distinto
    Actividad *a = &actividades[i];

    // aqui leo lo qu e manda el pafre
    FILE *entrada = fdopen(por_donde_leo, "r");
    char linea[LARGO_MENSAJE + 2];
    while (fgets(linea, sizeof(linea), entrada) != NULL) {
        linea[strcspn(linea, "\n")] = '\0';
        printf("    [%s] recibio: %s\n", a->nombre, linea);
    }
    fclose(entrada);



    printf("    [%s] pid=%d trabajando %d ms...\n", a->nombre, getpid(), a->tiempo_ms);
    dormir_ms(a->tiempo_ms);

    // hago que falle aleatoriamente
    if (rand() % 100 < probabilidad_falla) {
        printf("    [%s] FALLO!\n", a->nombre);
        close(por_donde_escribo);
        exit(1);
    }

    char mensaje[LARGO_MENSAJE];
    snprintf(mensaje, sizeof(mensaje), "insumo de %s listo", a->nombre);
    if (write(por_donde_escribo, mensaje, strlen(mensaje)) == -1) {
        perror("write en el hijo");
    }

    close(por_donde_escribo);

    // si o si el hijo debe terminar aqui
    exit(0);
}



// el padre guarda el mensaje y se lo entrega a la actividad que depende de ella. Esto porque la actividad que termina ya no estara par qa pasar el mensaje a la nueva tarea

void entregar_insumos(int i, int por_donde_escribo) {

    Actividad *a = &actividades[i];
    for (int d = 0; d < a->cuantas_dependencias; d++) {

        char linea[LARGO_MENSAJE + 2];
        snprintf(linea, sizeof(linea), "%s\n", actividades[a->dependencias[d]].mensaje);

        if (write(por_donde_escribo, linea, strlen(linea)) == -1) {
            perror("write al hijo");
            return;

        }
    }
}


//tengo un limite de k procesos creados, el sistema debe esperar a que se libere un cupo para crear mas
int ya_avisamos_limite = 0;

void avisar_limite(char *que_fallo) {

    if (ya_avisamos_limite == 0) {

        perror(que_fallo);
        fprintf(stderr, "Aviso: el sistema no deja crear mas, se espera a que se libere un cupo\n");
        ya_avisamos_limite = 1;

    }
}



// devuelve 0 si pudo crear el hijo y -1 si no
int lanzar_actividad(int i) {
    Actividad *a = &actividades[i];

    int hijo_a_padre[2];  // el hijo escribe por [1], el padre lee por [0]
    int padre_a_hijo[2]; // el padre escribe por [1], el hijo lee por [0]


    if (pipe(hijo_a_padre) == -1) {

        avisar_limite("pipe");
        return -1;
    }


    if (pipe(padre_a_hijo) == -1) {

        avisar_limite("pipe");
        close(hijo_a_padre[0]);
        close(hijo_a_padre[1]);
        return -1;

    }

    // queda texto esperando en el buffer del printf (sale repetido), por eso lo vacio antes
    fflush(stdout);

    pid_t pid = fork();

    if (pid == -1) {

        avisar_limite("fork");
        close(hijo_a_padre[0]);
        close(hijo_a_padre[1]);
        close(padre_a_hijo[0]);
        close(padre_a_hijo[1]);
        return -1;

    }

    

    if (pid == 0) {
        
        close(hijo_a_padre[0]);
        close(padre_a_hijo[1]);
        trabajo_del_hijo(i, padre_a_hijo[0], hijo_a_padre[1]);
    }

    
    close(hijo_a_padre[1]);
    close(padre_a_hijo[0]);

    a->estado = CORRIENDO;
    a->pid = pid;
    a->tuberia_lectura = hijo_a_padre[0];
    printf("[planificador] lanzo '%s' (pid=%d)\n", a->nombre, pid);

   
    entregar_insumos(i, padre_a_hijo[1]);
    close(padre_a_hijo[1]);
    return 0;
}

void recoger_resultado(int i, int como_termino) {

    Actividad *a = &actividades[i];

    int n = read(a->tuberia_lectura, a->mensaje, LARGO_MENSAJE - 1);
    if (n < 0) {
        n = 0;
    }

    a->mensaje[n] = '\0';
    close(a->tuberia_lectura);
    a->tuberia_lectura = -1;

    
    if (seremi_llego) {

        a->estado = ABORTADA;
        printf("[planificador] '%s' abortada por la Seremi\n", a->nombre);
        return;
    }

    
    if (WIFEXITED(como_termino) && WEXITSTATUS(como_termino) == 0) {

        a->estado = TERMINADA;
        printf("[planificador] '%s' termino. Mensaje: \"%s\"\n", a->nombre, a->mensaje);
    } 
    
    else {
        a->estado = FALLIDA;
        printf("[planificador] '%s' FALLO, se aborta lo que dependia de ella\n", a->nombre);

    }

}

void inspeccion_seremi(void) {

    signal(SIGINT, SIG_IGN);  // ya estamos cerrando, otro Ctrl+C no importa

    printf("\n*** LLEGO LA SEREMI! Se abortan todas las actividades ***\n");

    for (int i = 0; i < cuantas_actividades; i++) {

        if (actividades[i].estado == CORRIENDO) {
            kill(actividades[i].pid, SIGKILL);

        }
    }
    // wl waitpid es para que los hijos muertos no queden como zombies
    for (int i = 0; i < cuantas_actividades; i++) {
        if (actividades[i].estado == CORRIENDO) {
            waitpid(actividades[i].pid, NULL, 0);
            close(actividades[i].tuberia_lectura);
            actividades[i].estado = ABORTADA;
            printf("[planificador] '%s' abortada por la Seremi\n", actividades[i].nombre);
        }
    }
    for (int i = 0; i < cuantas_actividades; i++) {
        if (actividades[i].estado == PENDIENTE) {
            actividades[i].estado = ABORTADA;
        }
    }
}



void correr_plan(int K) {
    int corriendo_ahora = 0;

    while (1) {

        if (seremi_llego) {

            inspeccion_seremi();
            return;
        }

        int abortadas = abortar_dependientes();

        int lanzadas = 0;
        for (int i = 0; i < cuantas_actividades && corriendo_ahora < K; i++) {

            if (actividades[i].estado == PENDIENTE && esta_lista(i)) {

                if (lanzar_actividad(i) == 0) {

                    corriendo_ahora++;
                    lanzadas++;
                } 
                else {
                    break;  
                }
            }
        }

        if (corriendo_ahora == 0) {

            if (contar_pendientes() == 0) {

                return;  
            }
            if (lanzadas == 0 && abortadas == 0) {
                
                printf("[planificador] Quedan actividades que nunca podran partir "
                       "(hay un ciclo en el plan). Se abortan:\n");
                for (int i = 0; i < cuantas_actividades; i++) {

                    if (actividades[i].estado == PENDIENTE) {
                        actividades[i].estado = ABORTADA;
                        printf("   - %s\n", actividades[i].nombre);
                    }
                }
                return;
            }
            continue;  
        }

        
        int como_termino;
        pid_t pid = wait(&como_termino);

        if (pid == -1) {
            continue;  // el wait se desperto por el Ctrl+C: arriba se revisa
        }

        int i = buscar_por_pid(pid);
        if (i == -1) {
            continue;
        }

        corriendo_ahora--;
        recoger_resultado(i, como_termino);
    }
}

void mostrar_resumen(void) {

    int terminadas = 0, fallidas = 0, abortadas = 0;
    for (int i = 0; i < cuantas_actividades; i++) {
        if (actividades[i].estado == TERMINADA) terminadas++;
        if (actividades[i].estado == FALLIDA) fallidas++;
        if (actividades[i].estado == ABORTADA) abortadas++;
    }

    printf("\n===== Resumen de la celebracion =====\n");
    printf("Terminadas: %d\n", terminadas);
    printf("Fallidas:   %d\n", fallidas);
    printf("Abortadas:  %d\n", abortadas);
    printf("Total:      %d\n", cuantas_actividades);
}

int main(int argc, char *argv[]) {

    if (argc != 3 && argc != 4) {

        printf("Uso: %s plan.txt K [probabilidad_de_falla]\n", argv[0]);
        return 1;
    }


    int K = atoi(argv[2]);
    if (K < 1) {

        printf("K tiene que ser un numero mayor o igual a 1\n");
        return 1;
    }

    if (argc == 4) {

        probabilidad_falla = atoi(argv[3]);

        if (probabilidad_falla < 0 || probabilidad_falla > 100) {

            printf("La probabilidad de falla tiene que estar entre 0 y 100\n");
            return 1;
        }
    }

    
    setvbuf(stdout, NULL, _IOLBF, 0);
    srand(time(NULL));

    if (leer_plan(argv[1]) != 0) {
        return 1;
    }

    if (cuantas_actividades == 0) {
        printf("El plan esta vacio, no hay nada que celebrar\n");
        return 0;
    }

    if (conectar_dependencias() != 0) {
        printf("El plan tiene errores, no se ejecuta\n");
        liberar_plan();
        return 1;
    }


    printf("Plan cargado: %d actividades, K = %d, probabilidad de falla = %d%%\n",
           cuantas_actividades, K, probabilidad_falla);

    if (cuantas_actividades <= 20) {
        mostrar_plan();
    }

    printf("\n");

   
    struct sigaction config_senal;
    memset(&config_senal, 0, sizeof(config_senal));
    config_senal.sa_handler = llego_la_seremi;
    sigemptyset(&config_senal.sa_mask);
    config_senal.sa_flags = 0;
    sigaction(SIGINT, &config_senal, NULL);
    signal(SIGPIPE, SIG_IGN);

    correr_plan(K);
    mostrar_resumen();
    liberar_plan();

    
    if (seremi_llego) {
        return 130;  // el codigo de siempre cuando algo termina por Ctrl+C
    }

    return 0;







}