**Tarea 1: El Planificador Dieciochero**  
   
 Autor: Valentín Aravena (trabajo individual)  
   
 **Cómo compilar y ejecutar**  
   
 make  
   
  ./planificador plan.txt K  
   
    
   
 make es solo un atajo para no escribir el comando largo. Sin make, el comando es:  
   
 gcc -Wall -Wextra -std=c17 -o planificador planificador.c lector_de_plan.c  
   
    
- plan.txt: el archivo con las actividades.  
- K: cuántas actividades (procesos hijos) pueden correr al mismo tiempo, como máximo.  
- Opcional: un número extra al final que define la probabilidad de falla de cada actividad, de 0 a 100 (por defecto 10). Por ejemplo, “./planificador plan.txt 2 0” corre sin fallas y “./planificador plan.txt 2 50” hace fallar más o menos la mitad. Aun que al probarlo, casi siempre fallaba más del porcentaje.  
   
 Para la prueba de estrés:  
   
 ./generar_plan.sh 10000 > plan_grande.txt  
   
  ./planificador plan_grande.txt 50  
   
    
 **Archivos**  
- planificador.h: la ficha de cada actividad (struct Actividad), los estados y las funciones compartidas.  
- lector_de_plan.c: lee plan.txt y arma el arreglo de actividades.  
- planificador.c: el main y el planificador (fork, pipes, wait y Ctrl+C).  
- Makefile: atajo para compilar.  
- generar_plan.sh: genera el plan de 10.000 actividades para la prueba de estrés (lo hice con ayuda de la IA).  
- plan.txt: el plan de ejemplo del enunciado.  
- pruebas/: planes para probar casos raros (ciclo, dependencia que no existe, id repetido, archivo vacío y un archivo de Windows con tiempos vacíos).  
 **Cómo funciona**  
1. Se lee plan.txt. Cada línea se separa por los ":" y se guarda en el arreglo actividades. Si el tiempo viene vacío, se sortea entre 100 y 5000 ms. Las dependencias se guardan primero como texto y, cuando ya se leyó todo el archivo, se convierten en posiciones del arreglo. Se hace en dos pasadas porque una actividad puede depender de otra que aparece más abajo.  
2. Cada actividad es un nodo del grafo y guarda las posiciones de las actividades de las que depende. Para saber si puede partir, basta revisar el estado de esas dependencias.  
3. Cada actividad tiene un estado: PENDIENTE, CORRIENDO, TERMINADA, FALLIDA o ABORTADA. En cada vuelta del ciclo principal (correr_plan), el padre aborta las pendientes que dependen de algo que falló, lanza con fork() las que están listas mientras haya menos de K hijos vivos, y espera con wait() a que termine alguno.  
4. Límite K: la variable corriendo_ahora cuenta los hijos vivos. Solo se lanza una actividad si corriendo_ahora < K, y cada wait() resta uno. Así nunca hay más de K procesos creados.  
5. No hay busy waiting: cuando el padre no puede lanzar nada, se bloquea en wait() hasta que termine un hijo, sin gastar CPU.  
6. Pipes: cuando una actividad termina, su dependiente todavía no existe como proceso, así que el padre hace de cartero. Cada actividad usa dos pipes. Con padre_a_hijo, el padre le entrega los mensajes ("insumo de X listo") de sus dependencias. Con hijo_a_padre, el hijo manda al terminar su propio mensaje (máximo 128 caracteres), que el padre guarda para entregarlo después. Se crean al lanzar la actividad y se cierran apenas se usan.  
7. Fallos: cada actividad puede fallar con cierta probabilidad. Si falla, hace exit(1). El padre lo detecta con WIFEXITED y WEXITSTATUS, la marca FALLIDA y se abortan todas las que dependían de ella. El resto del plan sigue normal. Si un hijo muere por una señal, también cuenta como fallido.  
8. Ctrl+C (la Seremi): el manejador de SIGINT solo pone seremi_llego = 1. El ciclo principal lo revisa, hace kill(pid, SIGKILL) a los hijos que estén corriendo, luego waitpid a cada uno para que no queden zombies, marca todo como abortado e imprime el resumen.  
 **Funciones**  
   
 lector_de_plan.c  
- es_espacio: dice si un carácter es espacio, tab o salto de línea (incluye el \r de Windows).  
- copiar_sin_espacios: copia un pedazo de texto quitándole los espacios de los bordes.  
- leer_linea: separa una línea por los ":" y la guarda como actividad nueva.  
- leer_plan: abre el archivo y llama a leer_linea por cada línea.  
- buscar_actividad: busca una actividad por su id y devuelve su posición (o -1).  
- conectar_dependencias: revisa ids repetidos y convierte las dependencias de texto a posiciones del arreglo.  
- mostrar_plan: muestra el plan en pantalla.  
- liberar_plan: libera la memoria pedida con malloc y strdup.  
   
 planificador.c  
- llego_la_seremi: se ejecuta con Ctrl+C; solo marca seremi_llego = 1.  
- dormir_ms: duerme una cantidad de milisegundos (simula el trabajo).  
- esta_lista: dice si todas las dependencias de una actividad terminaron bien.  
- abortar_dependientes: aborta las actividades que dependen de algo que falló o fue abortado.  
- contar_pendientes: cuenta cuántas actividades faltan por lanzar.  
- buscar_por_pid: dice qué actividad estaba haciendo el hijo con ese pid.  
- trabajo_del_hijo: lo que hace cada proceso hijo: recibe insumos, duerme, puede fallar y manda su mensaje.  
- entregar_insumos: el padre le escribe al hijo los mensajes de sus dependencias.  
- avisar_limite: avisa una sola vez si el sistema no deja crear más procesos o pipes.  
- lanzar_actividad: crea los dos pipes y hace el fork() de una actividad.  
- recoger_resultado: lee el mensaje del hijo que terminó y decide si terminó bien o falló.  
- inspeccion_seremi: mata a los hijos que están corriendo y los recoge con waitpid.  
- correr_plan: el ciclo principal del planificador.  
- mostrar_resumen: muestra cuántas actividades terminaron, fallaron o se abortaron.  
- main: revisa los argumentos, lee el plan, instala los manejadores de señales y ejecuta.  
 **Decisiones de diseño**  
- Cómo falla una actividad: el enunciado no lo dice, así que la falla se simula con una probabilidad (10% por defecto, se cambia con el tercer argumento).  
- K cuenta solo los procesos hijos (las actividades), no al planificador.  
- No uso hilos ni mecanismos de sincronización de hilos: solo procesos, pipes y señales, como pide el enunciado.  
- Si una dependencia no existe o un id está repetido, el plan se considera inválido y no se ejecuta.  
- Ciclos (1 depende de 2 y 2 depende de 1): se detectan cuando no corre nadie, quedan pendientes y ninguna puede partir. Esas se abortan con un aviso y el resto del plan se ejecuta normal.  
- Líneas mal escritas: se avisa y se saltan.  
- Máximo de actividades: 15.000 (el enunciado pide hasta 10.000). Si un plan trae más, las que no caben se saltan con un aviso.  
- Para buscar una actividad por su id uso un for simple. Con 10.000 actividades demora menos de un segundo, y prefiero código simple.  
- Si el sistema no deja crear más procesos o pipes (K muy grande), el programa no se cae: avisa una vez y espera a que se libere un cupo.  
 **Detalles técnicos**  
- fflush(stdout) antes del fork(): si queda texto en el buffer de printf, el hijo lo hereda y se imprime dos veces.  
- signal(SIGINT, SIG_DFL) en el hijo: el hijo es copia del padre y heredó el manejador de Ctrl+C; se deja normal para que muera con Ctrl+C.  
- sigaction con sa_flags = 0 en vez de signal(): así el wait() del padre siempre se despierta con Ctrl+C. Con signal() depende de cómo se compile.  
- seremi_llego es volatile sig_atomic_t porque la modifica el manejador de la señal.  
- srand(getpid()) en el hijo: si no, todos los hijos heredan la misma semilla y sortean lo mismo.  
- signal(SIGPIPE, SIG_IGN): si un hijo muere antes de leer sus insumos, escribirle al pipe mataría al planificador.  
- El padre cierra los extremos de los pipes que no usa: si no cierra su extremo de escritura, el hijo nunca recibe el fin de archivo y se queda esperando.  
 **Pruebas que hice**  
- Plan del enunciado con K = 1, 3 y 6, y con probabilidad de falla 0, 35 y 100.  
- Casos de la carpeta pruebas/ (ciclo, dependencia inexistente, id repetido, archivo vacío, y un archivo de Windows con tiempos vacíos, corchetes y una línea mal escrita).  
- Ctrl+C a mitad de ejecución: el programa termina con código 130 y no quedan procesos (revisado con pgrep).  
- Espera sin gastar CPU: mientras las actividades duermen, el padre queda en estado S con 0 segundos de CPU (revisado con ps).  
- Salida redirigida a un archivo (> log.txt): no se duplican líneas.  
- 10.000 actividades con K = 50 (plan hecho con generar_plan.sh): terminaron las 10.000, sin zombies, y al revisar con pgrep nunca se vieron más de 50 hijos vivos.  
 **Declaración de uso de Inteligencia Artificial**  
   
 Herramientas utilizadas: Gemini.  
   
 Para qué se usó:  
1. Le pedí a la IA que me explicara cómo aplicar fork, wait, pipes y señales a esta tarea, y que revisara mi diseño contra cada requisito, contra la rúbrica y contra casos borde (ciclos, dependencias inexistentes, límite de archivos abiertos, Ctrl+C).  
2. Antes del programa final pedí ejemplos cortos y separados (uno de fork + pipe + wait y otro del límite de concurrencia K), que compilé y ejecuté para entender cada mecanismo por separado.  
3. El código entregado (lector_de_plan.c, planificador.c, planificador.h), el Makefile y el script generar_plan.sh fueron generados con asistencia de Gemini.  
4. Una vez teniendo el código final, le pedí a Gemini que me hiciera un README destacando las partes esenciales del funcionamiento del programa. Esto me ayudó a una mejor redacción y ahorro de tiempo. Igual ya había dejado hartos comentarios en el código mismo.  
   
 Dado que no poseo un nivel decente de programación en C (ni se nos instruyó lo suficiente), tuve que apoyarme bastante en la IA para aprender. Hay partes que la IA tuvo que resolver porque yo no podría ni por si acaso. Esto no quiere decir que le pedí el trabajo, sino que me diera una solución para analizarla, entenderla y finalmente tomar la decisión de incluirla o no.  
   
 Qué hice yo:  
- Decidí trabajar función por función para entender cada parte antes de juntarlas, y compilé y ejecuté los ejemplos intermedios.  
- Revisé el código final, ejecuté las pruebas, usé Ctrl+C, leí y entendí la gran mayoría de las funciones (no me las sé de memoria) y preparé la explicación.  
- Tomé las "Decisiones de diseño”.  
   
 Revisé el código entregado y me hago responsable de su contenido. Puedo explicar su funcionamiento en una defensa oral.  
