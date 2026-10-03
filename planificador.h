


#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H  
#include <sys/types.h>   // de aqui sale pid_t, que uso en la struct


#define TOPE_ACTIVIDADES 15000     
// se pide 10000 actividades pero deje un poco mas de margen por si acaso
#define LARGO_TEXTO 64    
// le puse 64 porque un id o nombre puede tener hasta 64 letras 
#define LARGO_MENSAJE 128     
// es un mensaje corto asi q no necesita mucho

// estados posibles de una actividad
#define PENDIENTE 0
#define CORRIENDO 1
#define TERMINADA 2
#define FALLIDA   3
#define ABORTADA  4   
// // en este ultimo se hace el uso de ctrl + c cuando llega Seremi o cuando una actividad depende de otra que fallo



typedef struct {

    char id[LARGO_TEXTO];
    char nombre[LARGO_TEXTO];
    int tiempo_ms;

    char *dependencias_texto;    // las dependencias que pase a  texto
    int cuantas_dependencias;  // numero de dependencias
    int *dependencias;    // lo mismo pero convertido en posiciones del arreglo


    int estado;
    pid_t pid;  //pid del hijo 
    int tuberia_lectura;  // el padre lee lo que le manda ese hijo
    char mensaje[LARGO_MENSAJE]; // lo que mando el hijo al terminar


} Actividad;


// estas dos se crean en lector_de_plan.c
// extern significa "existen oero estan en otro archivo
//asiplanificador.c tambien las usa sin crear una copia
extern Actividad actividades[TOPE_ACTIVIDADES];
extern int cuantas_actividades;

// funciones de lector_de_plan.c
int leer_plan(char *nombre_archivo);
int conectar_dependencias(void);
int buscar_actividad(char *id);
void mostrar_plan(void);
void liberar_plan(void);

#endif
