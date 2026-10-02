#!/bin/bash
#le pedi a la IA que creara este archivo para que genere un plan grande para la prueba de estres
# Uso: ./generar_plan.sh 10000 > plan_grande.txt


N=${1:-10000}

awk -v n="$N" 'BEGIN {
    srand();
    for (i = 1; i <= n; i++) {
        deps = "";
        cantidad = int(rand() * 3);   # 0, 1 o 2 dependencias
        for (d = 0; d < cantidad && i > 1; d++) {
            dep = int(rand() * (i - 1)) + 1;
            if (deps == "") deps = dep; else deps = deps ", " dep;
        }
        tiempo = int(rand() * 41) + 10;
        printf "%d : actividad_%d : %d : %s\n", i, i, tiempo, deps;
    }
}'