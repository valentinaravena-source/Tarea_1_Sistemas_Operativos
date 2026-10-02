# cree este archivo para no tener que poner gcc todo el rato, mas simple poner make y listo

planificador: planificador.c lector_de_plan.c planificador.h
	gcc -Wall -Wextra -std=c17 -o planificador planificador.c lector_de_plan.c -lpthread

#para borrar el ejecutable pon make clean
clean:
	rm -f planificador