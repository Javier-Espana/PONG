#!/bin/bash
# Script mejorado para habilitar X11 con mejor compatibilidad

# Crear archivo de autorización X11 temporal
XAUTH=/tmp/.docker.xauth
if [ ! -f $XAUTH ]; then
    xauth_list=$(xauth nlist $DISPLAY 2>/dev/null | sed -e 's/^..../ffff/')
    if [ ! -z "$xauth_list" ]; then
        echo $xauth_list | xauth -f $XAUTH nmerge -
    else
        touch $XAUTH
    fi
    chmod a+r $XAUTH
fi

# Habilitar acceso X11 para Docker
xhost +local:docker

# Exportar XAUTH para docker-compose
export XAUTH

# Construir y ejecutar
docker compose up --build