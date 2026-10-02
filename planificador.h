


#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H  
#include <sys/types.h>   // lei que es una buena practica poner esto


#define TOPE_ACTIVIDADES 15000     // se pide 10000 actividades pero deje un poco mas de margen por si acaso
#define LARGO_TEXTO 64    // le puse 64 porque un id o nombre puede tener hasta 64 letras
#define LARGO_MENSAJE 128     // es el maximo de caracteres para un mensaje

// estados posibles de una actividad
#define PENDIENTE 0
#define CORRIENDO 1
#define TERMINADA 2
#define FALLIDA   3
#define ABORTADA  4   // // aqui se hace el uso de ctrl + c cuando llega Seremi, o cuando una actividad depende de otra que fallo



typedef struct {

    char id[LARGO_TEXTO];
    char nombre[LARGO_TEXTO];
    int tiempo_ms;

    char *dependencias_texto;    // las dependencias las pase a  texto
    int cuantas_dependencias;
    int *dependencias;    // lo mismo pero como posiciones del arreglo, aun que no son ids


    int estado;
    pid_t pid;  // el pid del hijo que la esta haciendo
    int tuberia_lectura;  // por aca el padre lee lo que le manda ese hijo
    char mensaje[LARGO_MENSAJE]; // lo que mando el hijo al terminar

} Actividad;


// estas dos viven en lector_de_plan.c, pero planificador.c tambien las usa
extern Actividad actividades[TOPE_ACTIVIDADES];
extern int cuantas_actividades;

// funciones de lector_de_plan.c
int leer_plan(char *nombre_archivo);
int conectar_dependencias(void);
int buscar_actividad(char *id);
void mostrar_plan(void);
void liberar_plan(void);

#endif