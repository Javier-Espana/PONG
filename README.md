# PONG
Octubre de 2024 | José Mérida, Ángel Esquit, Javier España, Daniel Barillas
## Dependencias
Pendiente

## Corrida / Compilación

Compilación | Folder de SDL2 en la raiz del disco C

```
g++ -I C:/SDL2/include -L C:/SDL2/lib -L C:/SDL2_image/lib -o bin/pong.exe src/pong.cpp -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lpthread -lstdc++
```

Compilación | Folder de SDL2 en otro directorio

```
g++ -I ../SDL2/include -L ../SDL2/lib -o bin/pong.exe src/pong.cpp -lmingw32 -lSDL2main -lSDL2 
```

Ejecución

```
./bin/pong
```

Para activar
```
xhost +local:docker
```

./run-docker.sh