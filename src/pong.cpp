#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <pthread.h>
#include <atomic>
#include <unistd.h>
#include <semaphore.h>
#include <SDL2/SDL_ttf.h>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdlib>  // For rand()

// Constantes de juego / variables globales
const int SCREEN_WIDTH = 800;       // Ancho de la pantalla
const int SCREEN_HEIGHT = 600;      // ALtura de la pantalla
const int WALL_THICKNESS = 10;      // Ancho de las paredes
const int PADDLE_HEIGHT = 100;      // Altura de las paletas
const int BALL_SIZE = 20;           // Tamano de la pelota
int BALL_SPEED = 7;                // Velocidad de la pelota
const int PADDLE_WIDTH = 20;        // Ancho de paleta
const int MAX_BALL_SPEED = 20;      // Velocidad Maxima de Pelota
const int MIN_BALL_SPEED = 5;      // Velocidad Minima de Pelota
const int MIN_PADDLE_SPEED = 7;     // Velocidad Minima de Paleta
const int MAX_PADDLE_SPEED = 17;    // Velocidad Maxima de Paleta
const int WINNING_SCORE = 10;
const double MAX_ANGLE = 0.8;
int paddleSpeed = 10;              // Velocidad de paleta
TTF_Font* font = nullptr;
int player1Score;
int player2Score;
int winner = 3;

// Enumeración para diferentes estados de juego
enum GameState {
    MAIN_MENU,      // Menu Principal
    SINGLE_PLAYER,  // Jugador vs Computadora
    TWO_PLAYER,     // Jugador vs Jugador
    SIMULATION,     // Computadora vs Computadora
    GAME_OVER,      // Juego terminado
    SETTINGS,       // Configuracion
    QUIT            // Salir
};

// Variable global para almacenar el estado de juego
std::atomic<GameState> currentState(MAIN_MENU);

// Variables globales de recursos de SDL
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* mainMenuTexture = nullptr;
SDL_Texture* settingsTexture = nullptr;
SDL_Texture* gameOverTexture = nullptr;

// Rectángulos para botones del menú
SDL_Rect singlePlayerButton = { 175, 210, 450, 75 };
SDL_Rect twoPlayerButton = { 125, 310, 560, 75 };
SDL_Rect simulationButton = { 125, 400, 560, 75 };
SDL_Rect settingsButton = { 660, 45, 95, 95 };
SDL_Rect lessBallSpeedButton = {300, 245, 50, 50};
SDL_Rect moreBallSpeedButton = {445, 245, 50, 50};
SDL_Rect lessPaddleSpeedButton = {300, 385, 50, 50};
SDL_Rect morePaddleSpeedButton = {445, 385, 50, 50};

// Variables para sincronización de hilos
pthread_mutex_t gameMutex;          // Mutex
pthread_cond_t paddleUpdateCond;    // Variable condicional para actualizar paletas
pthread_cond_t renderCond;          // Variable condicional para avisar al renderer
sem_t ballUpdateSem;                // Semaforo para actualizar pelota

// Estructura para representar una paleta
struct Paddle {
    SDL_Rect rect;      // El rectangulo para dibujar en SDL
    bool isMovingUp;    // Representa el estado de la paleta en un momento dado (cuando se lee el estado del teclado)
    bool isMovingDown;  // Representa el estado de la paleta en un  momento dado (cuando se lee el estado del teclado)
};

// Estructura para representar la pelota
struct Ball {
    SDL_Rect rect;      // El rectangulo para dibujar en SDL
    double velX, velY;     // Representa la velocidad en cada coordenada de la pelota
};

// Estructura para almacenar informacion sobre la pelota, dentro del hilo que controla la pelota se modifican puntajes
struct BallData {
    Ball* ball;         // La pelota que se mueve
    int* player1Score;  // Puntaje del jugador 1
    int* player2Score;  // Puntaje del jugador 2
};

// Pelota global, se necesita acceder desde diferentes hilos
Ball global_ball = {{SCREEN_WIDTH / 2 - BALL_SIZE / 2, SCREEN_HEIGHT / 2 - BALL_SIZE / 2, BALL_SIZE, BALL_SIZE}, cos(0.785398) * BALL_SPEED, sin(0.785398) * BALL_SPEED};


// Variable para controlar la finalización del juego
std::atomic<bool> quit{false};

// Paleta Izquierda
SDL_Rect leftPaddleRect = { 50, SCREEN_HEIGHT / 2 - 50, 20, 100 }; // Rectangulo de SDL
Paddle leftPaddle = {leftPaddleRect, false, false}; // Estructura para almacenar la paleta y su estado de movimiento

// Paleta Derecha
SDL_Rect rightPaddleRect = { SCREEN_WIDTH - 70, SCREEN_HEIGHT / 2 - 50, 20, 100 };  // Rectangulo de SDL
Paddle rightPaddle = {rightPaddleRect, false, false};   // Estructura para almacenar la paleta y su estado de movimiento


// Función para manejar una paleta controlada por un jugador
void* playerPaddleFunc(void* arg) {
    Paddle* paddle = (Paddle*)arg;
    
    while (!quit) {
        /**
         * Espera a variable condicional señalizada desde el hilo de lógica de juego. De
         * esta manera se actualiza correctamente el estado de la paleta (moviéndose hacia
         * arriba / abajo) y se mantiene un "tickrate" consistente con los demás elementos.
         */
        pthread_mutex_lock(&gameMutex);
        pthread_cond_wait(&paddleUpdateCond, &gameMutex);
        pthread_mutex_unlock(&gameMutex);

        // Verifica el estado de la paleta y realiza los movimientos respectivos
        if (paddle->isMovingUp && paddle->rect.y > WALL_THICKNESS) {
            paddle->rect.y -= paddleSpeed;
        }
        if (paddle->isMovingDown && paddle->rect.y + PADDLE_HEIGHT < SCREEN_HEIGHT - WALL_THICKNESS) {
            paddle->rect.y += paddleSpeed;
        }
        // Postea el semaforo de actualizacion de la pelota
        sem_post(&ballUpdateSem);
                if (quit) break;

    }
    return NULL;
}

// Funcion para manejar una paleta controlada por IA
void* computerPaddleFunc(void* arg) {
    Paddle* paddle = (Paddle*)arg;
        srand(time(NULL));

    while (!quit) {
        /**
         * Espera a variable condicional señalizada desde el hilo de lógica de juego. De
         * esta manera se actualiza correctamente el estado de la paleta (moviéndose hacia
         * arriba / abajo) y se mantiene un "tickrate" consistente con los demás elementos.
         */        
        pthread_mutex_lock(&gameMutex);
        pthread_cond_wait(&paddleUpdateCond, &gameMutex);
        pthread_mutex_unlock(&gameMutex);

        // Verifica el estado de la paleta y la posicion de la pelota, se mueve directamente
        // hacia la altura de la pelota respecto a la paleta
        if (global_ball.rect.y < paddle->rect.y && paddle->rect.y > WALL_THICKNESS){
            paddle->rect.y -= paddleSpeed;
        }
        if (global_ball.rect.y > paddle->rect.y + PADDLE_HEIGHT && paddle->rect.y + PADDLE_HEIGHT < SCREEN_HEIGHT - WALL_THICKNESS){
            paddle->rect.y += paddleSpeed;
        }
        // Postea el semaforo de actualizacion de la pelota
        sem_post(&ballUpdateSem);
        if (quit) break;
    }
    return NULL;
}


// Funcion para manejar los movimientos de la pelota
void* ballFunc(void* arg) {
    BallData* ballData = (BallData*)arg;
    Ball* ball = ballData->ball;
    int* player1Score = ballData->player1Score;
    int* player2Score = ballData->player2Score;
    int ballFrozen = 0;

    while (!quit) {
        // Actualizacion de Posicion, esto se puede realizar de manera independiente de los demas hilos
        if (ballFrozen == 0){
        ball->rect.x += ball->velX;
        ball->rect.y += ball->velY;

        // Verificacion de colisiones con paredes
        if (ball->rect.y <= WALL_THICKNESS) {
            ball->rect.y = WALL_THICKNESS;  // Ajustar posición para evitar clipping
            ball->velY = -ball->velY;       // Rebotar hacia abajo
        }
        if (ball->rect.y + BALL_SIZE >= SCREEN_HEIGHT - WALL_THICKNESS) {
            ball->rect.y = SCREEN_HEIGHT - WALL_THICKNESS - BALL_SIZE;  // Ajustar posición para evitar clipping
            ball->velY = -ball->velY;  // Rebotar hacia arriba
        }

        // Espera a que ambas paletas actualicen su posicion por medio de un semaforo.
        sem_wait(&ballUpdateSem);
        sem_wait(&ballUpdateSem);
        pthread_mutex_lock(&gameMutex);

        // Verifica colisiones con la paleta izquierda
        if (ball->rect.x <= leftPaddle.rect.x + PADDLE_WIDTH &&
            ball->rect.x + BALL_SIZE >= leftPaddle.rect.x) {
            // Si la pelota está dentro de la altura de la paleta
            if (ball->rect.y + BALL_SIZE >= leftPaddle.rect.y &&
                ball->rect.y <= leftPaddle.rect.y + PADDLE_HEIGHT) {
                
                // Evitar que la pelota se sobreponga en la paleta
                ball->rect.x = leftPaddle.rect.x + PADDLE_WIDTH;


                // Ajustar velocidad vertical basado en donde golpea la paleta
                // Punto Contacto - Centro
                int hitPos = (ball->rect.y + BALL_SIZE / 2) - (leftPaddle.rect.y + PADDLE_HEIGHT / 2);

                float normalizedHitPos = static_cast<float>(hitPos) / (PADDLE_HEIGHT / 2);

                double bounceAngle = normalizedHitPos * MAX_ANGLE;
                ball->velX = fabs(cos(bounceAngle) * BALL_SPEED);
                ball->velY = sin(bounceAngle) * BALL_SPEED;

            }
        }

        // Verifica colisiones con la paleta derecha
        if (ball->rect.x + BALL_SIZE >= rightPaddle.rect.x &&
            ball->rect.x <= rightPaddle.rect.x + PADDLE_WIDTH) {
            // Si la pelota está dentro de la altura de la paleta
            if (ball->rect.y + BALL_SIZE >= rightPaddle.rect.y &&
                ball->rect.y <= rightPaddle.rect.y + PADDLE_HEIGHT) {

                // Evitar que la pelota se sobreponga en la paleta
                ball->rect.x = rightPaddle.rect.x  - PADDLE_WIDTH;

                // Ajustar velocidad vertical basado en donde golpea la paleta
                // Punto Contacto - Centro
                int hitPos = (ball->rect.y + BALL_SIZE / 2) - (rightPaddle.rect.y + PADDLE_HEIGHT / 2);

                float normalizedHitPos = static_cast<float>(hitPos) / (PADDLE_HEIGHT / 2);

                double bounceAngle = normalizedHitPos * MAX_ANGLE;

                ball->velX = -fabs(cos(bounceAngle) * BALL_SPEED); 
                ball->velY = sin(bounceAngle) * BALL_SPEED;
            }
        }


        // Manejo de la puntuacion
        if (ball->rect.x < 0) {
            // Puntaje para jugador 2
            (*player2Score)++;
            // Reiniciar la pelota
            ball->rect.x = SCREEN_WIDTH / 2 - BALL_SIZE / 2;
            ball->rect.y = SCREEN_HEIGHT / 2 - BALL_SIZE / 2;

            double angle = 0.785398; 

            ball->velX = (-1) * cos(angle) * BALL_SPEED;
            ball->velY = ((rand() % 2 == 0) ? 1 : -1) * sin(angle) * BALL_SPEED;
            ballFrozen = 12;
        }

        if (ball->rect.x > SCREEN_WIDTH) {
            // Puntaje para jugador 1
            (*player1Score)++;
            // Reiniciar la pelota
            ball->rect.x = SCREEN_WIDTH / 2 - BALL_SIZE / 2;
            ball->rect.y = SCREEN_HEIGHT / 2 - BALL_SIZE / 2;


            double angle = 0.785398; 

            ball->velX = (1) * cos(angle) * BALL_SPEED;
            ball->velY = ((rand() % 2 == 0) ? 1 : -1) * sin(angle) * BALL_SPEED;
            ballFrozen = 12;
        }

        // Desbloquear el Mutex de juego y señalar al hilo de renderizado que el juego está actualizado
        pthread_mutex_unlock(&gameMutex);
        pthread_cond_signal(&renderCond);
        } else {
            ballFrozen--;
            sem_wait(&ballUpdateSem);
            sem_wait(&ballUpdateSem);
            pthread_cond_signal(&renderCond);
        }
        if (quit) break;
    }
    return NULL;
}


// Función de lógica del juego
void* logicThreadFunc(void* arg) {
    /**
     * Al ser creado inicializa los 3 hilos hijos que necesita, siendo
     * 2 paletas y 1 pelota. El juego inicializa con un puntaje de 0-0.
     * El programa esta diseñado para que siempre se manejen estos hilos 
     * y únicamente se les pasen diferentes rutinas como parámetro.
     */
    pthread_t leftPaddleThread, rightPaddleThread, ballThread;
    player1Score = 0;
    player2Score = 0;
    rightPaddle.rect = { SCREEN_WIDTH - 70, SCREEN_HEIGHT / 2 - 50, 20, 100 };
    leftPaddle.rect = { 50, SCREEN_HEIGHT / 2 - 50, 20, 100 };
    BallData ballData = BallData{&global_ball, &player1Score, &player2Score};

    // Si la modalidad de juego es de un unico jugador, crea un hilo de jugador
    // a la izquierda y una paleta de computadora a la derecha
    if (currentState == SINGLE_PLAYER) {
        pthread_create(&leftPaddleThread, NULL, playerPaddleFunc, &leftPaddle);
        pthread_create(&rightPaddleThread, NULL, computerPaddleFunc, &rightPaddle);
        pthread_create(&ballThread, NULL, ballFunc, &ballData);
    }
    // Si la modalidad de juego es de dos jugadores, crea un hilo de jugador
    // a la izquierda y uno a la derecha
    else if (currentState == TWO_PLAYER) {
        pthread_create(&leftPaddleThread, NULL, playerPaddleFunc, &leftPaddle);
        pthread_create(&rightPaddleThread, NULL, playerPaddleFunc, &rightPaddle);
        pthread_create(&ballThread, NULL, ballFunc, &ballData);
    }
    // Si la modalidad de juego es de simulacion, crea un hilo de computadora
    // a la izquierda y uno a la derecha
    else if (currentState == SIMULATION){
        pthread_create(&leftPaddleThread, NULL, computerPaddleFunc, &leftPaddle);
        pthread_create(&rightPaddleThread, NULL, computerPaddleFunc, &rightPaddle);
        pthread_create(&ballThread, NULL, ballFunc, &ballData);
    }

    /**
     * Entra a un loop donde obtiene el estado del teclado para verificar que teclas
     * estan siendo presionadas en un momento dado.
     */
    while (currentState != QUIT) {
        // Actualizar el estado de las teclas
        const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);


        pthread_mutex_lock(&gameMutex);

        // Actualizar el movimiento de las paletas basado en la entrada
        leftPaddle.isMovingUp = currentKeyStates[SDL_SCANCODE_W];
        leftPaddle.isMovingDown = currentKeyStates[SDL_SCANCODE_S];
        rightPaddle.isMovingUp = currentKeyStates[SDL_SCANCODE_UP];
        rightPaddle.isMovingDown = currentKeyStates[SDL_SCANCODE_DOWN];

        // Señalar que las paletas deben actualizarse
        pthread_cond_broadcast(&paddleUpdateCond);
        pthread_mutex_unlock(&gameMutex);
        if (player1Score >= WINNING_SCORE){
            quit = true;
            winner = 1;
            break;
        } else if (player2Score >= WINNING_SCORE){
            quit = true;
            winner = 2;
            break;
        }
        usleep(16000);
    }
    sem_post(&ballUpdateSem);
    sem_post(&ballUpdateSem);
    pthread_join(leftPaddleThread, NULL);
    pthread_join(rightPaddleThread, NULL);
    pthread_join(ballThread, NULL);
    currentState = GAME_OVER;

    return nullptr;
}

// Rutina del hilo de renderizado
void* renderThreadFunc(void* arg) {
    SDL_Color textColor = {255, 255, 255, 255};  // Color blanco común para todo el texto

    while (currentState != QUIT) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Color de fondo
        SDL_RenderClear(renderer);

        // Renderizado según el estado del juego
        if (currentState == MAIN_MENU) {
            if (mainMenuTexture) {
                SDL_RenderCopy(renderer, mainMenuTexture, NULL, NULL);
            }
            SDL_Delay(16);  // Aproximadamente 60 FPS
        } else if (currentState == SINGLE_PLAYER || currentState == TWO_PLAYER || currentState == SIMULATION) {
            // Limpia la pantalla del renderer
            pthread_mutex_lock(&gameMutex);
            pthread_cond_wait(&renderCond, &gameMutex);
            pthread_mutex_unlock(&gameMutex);

            // Render paletas y pelota
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  // Color blanco para las paletas y la pelota
            SDL_RenderFillRect(renderer, &leftPaddle.rect);
            SDL_RenderFillRect(renderer, &rightPaddle.rect);
            SDL_RenderFillRect(renderer, &global_ball.rect);

            // Render de la red
            for (int y = 0; y < SCREEN_HEIGHT; y += 40) {
                SDL_Rect netSegment = {SCREEN_WIDTH / 2 - 1, y, 2, 20};
                SDL_RenderFillRect(renderer, &netSegment);
            }

            // Renderizar el puntaje del jugador 1
            std::string player1ScoreText = std::to_string(player1Score);
            SDL_Surface* surface = TTF_RenderText_Solid(font, player1ScoreText.c_str(), textColor);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect player1ScoreRect = {SCREEN_WIDTH / 4 - surface->w / 2, 50, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, NULL, &player1ScoreRect);
                SDL_FreeSurface(surface);
                SDL_DestroyTexture(texture);
            }

            // Renderizar el puntaje del jugador 2
            std::string player2ScoreText = std::to_string(player2Score);
            surface = TTF_RenderText_Solid(font, player2ScoreText.c_str(), textColor);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect player2ScoreRect = {3 * SCREEN_WIDTH / 4 - surface->w / 2, 50, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, NULL, &player2ScoreRect);
                SDL_FreeSurface(surface);
                SDL_DestroyTexture(texture);
            }

        } else if (currentState == GAME_OVER) {
            if (gameOverTexture) {
                SDL_RenderCopy(renderer, gameOverTexture, NULL, NULL);
            }

            // Renderizar el texto del ganador
            std::string winnerText = "Gana el Jugador " + std::to_string(winner);
            SDL_Surface* surface = TTF_RenderText_Solid(font, winnerText.c_str(), textColor);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect winnerTextRect = {SCREEN_WIDTH / 2 - surface->w / 2 + 15, SCREEN_HEIGHT / 2 - surface->h / 2, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, NULL, &winnerTextRect);
                SDL_FreeSurface(surface);
                SDL_DestroyTexture(texture);
            }
            SDL_Delay(16);  // Aproximadamente 60 FPS

        } else if (currentState == SETTINGS) {
            if (settingsTexture) {
                SDL_RenderCopy(renderer, settingsTexture, NULL, NULL);
            }

            // Renderizar la variable de Paddle Speed
            std::string paddleSpeedText = std::to_string(paddleSpeed);
            SDL_Surface* surface = TTF_RenderText_Solid(font, paddleSpeedText.c_str(), textColor);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect paddleSpeedRect = {SCREEN_WIDTH / 2 + 125, 370, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, NULL, &paddleSpeedRect);
                SDL_FreeSurface(surface);
                SDL_DestroyTexture(texture);
            }

            // Renderizar la variable Ball Speed
            std::string ballSpeedText = std::to_string(BALL_SPEED);
            surface = TTF_RenderText_Solid(font, ballSpeedText.c_str(), textColor);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect ballSpeedRect = {SCREEN_WIDTH / 2 + 125, 230, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, NULL, &ballSpeedRect);
                SDL_FreeSurface(surface);
                SDL_DestroyTexture(texture);
            }
            SDL_Delay(16);  // Aproximadamente 60 FPS
        }

        // Actualizar el renderizado
        SDL_RenderPresent(renderer);
    }

    return nullptr;
}


// Función para comprobar si el mouse está dentro de un botón
bool isInside(int x, int y, SDL_Rect buttonRect) {
    return (x >= buttonRect.x) && (x < buttonRect.x + buttonRect.w) && (y >= buttonRect.y) && (y < buttonRect.y + buttonRect.h);
}

// Función de inicialización de SDL
bool init(SDL_Window*& window, SDL_Renderer*& renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "¡SDL no pudo inicializarse! Error SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow("SDL2 Pong Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "¡No se pudo crear la ventana! Error SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "¡No se pudo crear el renderer! Error SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "¡SDL_image no pudo inicializarse! Error IMG: " << IMG_GetError() << std::endl;
        return false;
    }
    if (TTF_Init() == -1) {
        std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        return false;
    }

    // Cargar la fuente
    font = TTF_OpenFont("res/ARCADECLASSIC.ttf", 72);
    if (!font) {
        std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << std::endl;
        return false;
    }
    return true;
}

// Función para cargar texturas
SDL_Texture* loadTexture(const std::string& path, SDL_Renderer* renderer) {
    SDL_Texture* newTexture = nullptr;
    SDL_Surface* loadedSurface = IMG_Load(path.c_str());

    if (!loadedSurface) {
        std::cerr << "¡No se pudo cargar la imagen " << path << "! Error SDL_image: " << IMG_GetError() << std::endl;
        return nullptr;
    }

    newTexture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
    if (!newTexture) {
        std::cerr << "¡No se pudo crear la textura de " << path << "! Error SDL: " << SDL_GetError() << std::endl;
    }

    SDL_FreeSurface(loadedSurface);

    return newTexture;
}

// Función para cerrar SDL y limpiar recursos
void close(SDL_Window* window, SDL_Renderer* renderer) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    if (!init(window, renderer)) {
        return -1;
    }
    sem_init(&ballUpdateSem, 0, 0);

    mainMenuTexture = loadTexture("res/mainmenu.png", renderer);
    if (!mainMenuTexture) {
        close(window, renderer);
        return -1;
    }
    gameOverTexture = loadTexture("res/gameover.png", renderer);
    if (!gameOverTexture) {
        close(window, renderer);
        return -1;
    }
    settingsTexture = loadTexture("res/config.png", renderer);
    if (!settingsTexture) {
        close(window, renderer);
        return -1;
    }
    // Inicializar el mutex y la variable de condición
    pthread_mutex_init(&gameMutex, NULL);
    pthread_cond_init(&paddleUpdateCond, NULL);

    // Crear el hilo de renderizado
    pthread_t renderThread, gameLogicThread;
    pthread_create(&renderThread, nullptr, renderThreadFunc, nullptr);

    bool gameActive = false;
    // Bucle principal para manejar la entrada y las transiciones de estado
    SDL_Event e;
    while (currentState != QUIT) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                currentState = QUIT;
                quit = true;
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);

                // Comprobar clics de botones en el estado MAIN_MENU
                if (currentState == MAIN_MENU) {
                    if (gameActive){
                        pthread_join(gameLogicThread, NULL);
                    }
                    if (isInside(x, y, singlePlayerButton)) {
                        currentState = SINGLE_PLAYER;
                        pthread_create(&gameLogicThread, NULL, logicThreadFunc, NULL);
                    }
                    if (isInside(x, y, twoPlayerButton)) {
                        currentState = TWO_PLAYER;
                        pthread_create(&gameLogicThread, NULL, logicThreadFunc, NULL);
                    }
                    if (isInside(x, y, simulationButton)){
                        currentState = SIMULATION;
                        pthread_create(&gameLogicThread, NULL, logicThreadFunc, NULL);
                    }
                    if (isInside(x, y, settingsButton)){
                        currentState = SETTINGS;
                    }
                }
                else if (currentState == GAME_OVER) {
                    pthread_join(gameLogicThread, NULL);
                    quit = false;
                    currentState = MAIN_MENU;
                }
                else if (currentState == SETTINGS 
                    ){
                    if (isInside(x, y, settingsButton)){
                        currentState = MAIN_MENU;
                    }
                    else if (isInside(x, y, lessPaddleSpeedButton) && paddleSpeed > MIN_PADDLE_SPEED){
                        paddleSpeed--;
                    }
                    else if (isInside(x, y, morePaddleSpeedButton) && MAX_PADDLE_SPEED > paddleSpeed){
                        paddleSpeed++;
                    }
                    else if (isInside(x, y, lessBallSpeedButton) && BALL_SPEED > MIN_BALL_SPEED){
                        BALL_SPEED--;
                    }
                    else if (isInside(x, y, moreBallSpeedButton) && MAX_BALL_SPEED > BALL_SPEED){
                        BALL_SPEED++;
                    }
                }
            }
        }
        SDL_Delay(16);  // Aproximadamente 60 FPS
    }

    // Limpiar recursos
    pthread_mutex_destroy(&gameMutex);
    pthread_cond_destroy(&paddleUpdateCond);
    SDL_DestroyTexture(mainMenuTexture);
    close(window, renderer);

    return 0;
}