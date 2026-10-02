
#define _POSIX_C_SOURCE 200809L  // sin esto getline y strdup no compilan con -std=c17

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "planificador.h"

Actividad actividades[TOPE_ACTIVIDADES];
int cuantas_actividades = 0;

int es_espacio(char c) {

    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        return 1;
    }

    return 0;
}

// copia lo que hay entre inicio y fin, pero sin los espacios de los bordes. El archivo tiene espacios por todos lados (" asar_longaniza ") y los ids tienen que quedar limpios para poder compararlos despues
void copiar_sin_espacios(char *destino, int largo_max, char *inicio, char *fin) {

    while (inicio < fin && es_espacio(*inicio)) {
        inicio++;
    }


    while (fin > inicio && es_espacio(*(fin - 1))) {
        fin--;
    }


    int largo = fin - inicio;
    if (largo > largo_max - 1) {
        largo = largo_max - 1;

    }

    for (int k = 0; k < largo; k++) {
        destino[k] = inicio[k];
    }

    destino[largo] = '\0';
}




int leer_linea(char *linea, int numero_linea) {

    int vacia = 1;
    for (int k = 0; linea[k] != '\0'; k++) {

        if (!es_espacio(linea[k])) {
            vacia = 0;
        }
    }



    if (vacia) {
        return 0;
    }


    // los ':' son los separadores, hay 3 por linea, asi que queda partida en 4 partes
    char *dos_puntos1 = strchr(linea, ':');
    char *dos_puntos2 = NULL;
    char *dos_puntos3 = NULL;


    if (dos_puntos1 != NULL) {
        dos_puntos2 = strchr(dos_puntos1 + 1, ':');
    }


    if (dos_puntos2 != NULL) {
        dos_puntos3 = strchr(dos_puntos2 + 1, ':');
    }


    if (dos_puntos3 == NULL) {
        fprintf(stderr, "Aviso: la linea %d esta mal escrita, la salto\n", numero_linea);
        return -1;
    }


    if (cuantas_actividades >= TOPE_ACTIVIDADES) {
        fprintf(stderr, "Aviso: hay demasiadas actividades, maximo %d\n", TOPE_ACTIVIDADES);
        return -1;
    }

    Actividad *a = &actividades[cuantas_actividades];
    copiar_sin_espacios(a->id, LARGO_TEXTO, linea, dos_puntos1);


    if (strlen(a->id) == 0) {
        fprintf(stderr, "Aviso: la linea %d no tiene id, la salto\n", numero_linea);
        return -1;
    }


    copiar_sin_espacios(a->nombre, LARGO_TEXTO, dos_puntos1 + 1, dos_puntos2);

    // si el tiempo viene vacio me toca sortearlo entre 100 y 5000
    char tiempo[LARGO_TEXTO];
    copiar_sin_espacios(tiempo, LARGO_TEXTO, dos_puntos2 + 1, dos_puntos3);

    if (strlen(tiempo) == 0) {
        a->tiempo_ms = 100 + rand() % 4901;
    } 
    
    else {
        a->tiempo_ms = atoi(tiempo);
    }

    if (a->tiempo_ms < 0) {
        a->tiempo_ms = 0;
    }

    // las dependencias las dejo como texto por ahora, todavia no las puedo buscar porque pueden apuntar a una actividad que esta mas abajo en el archivo
    a->dependencias_texto = strdup(dos_puntos3 + 1);
    a->cuantas_dependencias = 0;
    a->dependencias = NULL;
    a->estado = PENDIENTE;
    a->pid = 0;
    a->tuberia_lectura = -1;
    a->mensaje[0] = '\0';

    cuantas_actividades++;
    return 0;
}

int leer_plan(char *nombre_archivo) {
    FILE *archivo = fopen(nombre_archivo, "r");

    if (archivo == NULL) {
        perror("No se pudo abrir el plan");
        return -1;
    }

    
    char *linea = NULL;
    size_t tam = 0;
    int numero_linea = 0;
    
    while (getline(&linea, &tam, archivo) != -1) { // lee la linea entera
        numero_linea++;
        leer_linea(linea, numero_linea);
    }

    free(linea);
    fclose(archivo);
    return 0;
}

int buscar_actividad(char *id) {
    for (int i = 0; i < cuantas_actividades; i++) {
        if (strcmp(actividades[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

// ahora paso las dependencias de texto a posiciones del arreglo.
int conectar_dependencias(void) {
    int hay_error = 0;

    for (int i = 0; i < cuantas_actividades; i++) {
        if (buscar_actividad(actividades[i].id) != i) {
            fprintf(stderr, "Error: el id '%s' esta repetido\n", actividades[i].id);
            hay_error = 1;
        }
    }

    for (int i = 0; i < cuantas_actividades; i++) {
        Actividad *a = &actividades[i];

        // por si las quieren escribir en corchetes. ej [1, 2]
        for (int k = 0; a->dependencias_texto[k] != '\0'; k++) {
            if (a->dependencias_texto[k] == '[' || a->dependencias_texto[k] == ']') {
                a->dependencias_texto[k] = ' ';
            }
        }

        // cuento las comxs para saber cuanta memoria pedir (2 comas = 3 dependencias como maximo)
        int comas = 0;
        for (int k = 0; a->dependencias_texto[k] != '\0'; k++) {
            if (a->dependencias_texto[k] == ',') {
                comas++;
            }
        }
        a->dependencias = malloc(sizeof(int) * (comas + 1));

        char *pedazo = strtok(a->dependencias_texto, ",");
        while (pedazo != NULL) {
            char dependencia_id[LARGO_TEXTO];
            copiar_sin_espacios(dependencia_id, LARGO_TEXTO, pedazo, pedazo + strlen(pedazo));

            if (strlen(dependencia_id) > 0) {
                int posicion = buscar_actividad(dependencia_id);
                if (posicion == -1) {
                    fprintf(stderr, "Error: '%s' depende de '%s', que no existe\n",
                            a->id, dependencia_id);
                    hay_error = 1;
                } else {
                    a->dependencias[a->cuantas_dependencias] = posicion;
                    a->cuantas_dependencias++;
                }
            }
            pedazo = strtok(NULL, ",");
        }
    }

    if (hay_error) {
        return -1;
    }
    return 0;
}

void mostrar_plan(void) {

    for (int i = 0; i < cuantas_actividades; i++) {

        Actividad *a = &actividades[i];
        printf("  %s : %s : %d ms : depende de [", a->id, a->nombre, a->tiempo_ms);

        for (int d = 0; d < a->cuantas_dependencias; d++) {

            printf("%s", actividades[a->dependencias[d]].id);
            
            if (d < a->cuantas_dependencias - 1) {
                printf(", ");
            }
        }
        printf("]\n");
    }
}

void liberar_plan(void) {
    for (int i = 0; i < cuantas_actividades; i++) {
        free(actividades[i].dependencias_texto);
        free(actividades[i].dependencias);
    }
    cuantas_actividades = 0;
}