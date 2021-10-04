#include "SDL_net.h"

#include "MyGame.h"

using namespace std;

const char* IP_NAME = "localhost";
const Uint16 PORT = 55555;

enum class screenProg { 
    MENU, GAME, CONN_ERROR, GAME_OVER, EXIT
};
screenProg currentScreen = screenProg::MENU; //what screen should be being displayed

MyGame* game = new MyGame();

/**
 * When received message from server.
 * @param socket_ptr pointer to socket.
 */
static int on_receive(void* socket_ptr) {
    TCPsocket socket = (TCPsocket)socket_ptr;

    const int message_length = 1024;

    char message[message_length];
    int received;

    do {
        received = SDLNet_TCP_Recv(socket, message, message_length);

        if (received != -1) {
            message[received] = '\0';
        }
        else if (received >= message_length) {
            currentScreen = screenProg::EXIT;
            std::cout << "An unknown error occurred. 'Received' was out of array range" << std::endl;
        }
        else {
            if (currentScreen != screenProg::EXIT) {
                currentScreen = screenProg::CONN_ERROR;
                game->setErrorMessage("Connection was terminated.");
            }
        }
        
        char* pch = strtok(message, ",");

        // get the command, which is the first string in the message
        if (pch == NULL) {
            pch = strtok("CLOSING", ",");
            currentScreen = screenProg::EXIT;
        }
        string cmd(pch);

        // then get the arguments to the command
        vector<string> args;

        while (pch != NULL) {
            pch = strtok(NULL, ",");

            if (pch != NULL) {
                args.push_back(string(pch));
            }
        }

        game->on_receive(cmd, args);

    } while (received > 0 && currentScreen == screenProg::GAME);

    return 0;
}

/**
 * When sending to server.
 * @param socket_ptr pointer to socket.
 */
static int on_send(void* socket_ptr) {
    TCPsocket socket = (TCPsocket)socket_ptr;

    while (currentScreen == screenProg::GAME) {
        if (game->messages.size() > 0) {
            string message = "CLIENT_DATA";

            for (auto m : game->messages) {
                message += "," + m;
            }

            game->messages.clear();

            cout << "Sending_TCP: " << message << endl;

            SDLNet_TCP_Send(socket, message.c_str(), message.length());
        }

        SDL_Delay(1);
    }

    return 0;
}

/**
 * Send final message to server to indicate closure with User intent.
 * @param socket_ptr pointer to socket.
 */
void sendTerminate(void* socket_ptr) {
    TCPsocket socket = (TCPsocket)socket_ptr;

        if (game->messages.size() > 0) {
            string message = "CLIENT_DATA";

            for (auto m : game->messages) {
                message += "," + m;
            }

            game->messages.clear();

            cout << "Sending_TCP: " << message << endl;

            SDLNet_TCP_Send(socket, message.c_str(), message.length());
        }

        SDL_Delay(1);
}

/**
 * Handle the loop of MyGame, including what screen should be displayed.
 * break on screen change.
 * @param renderer pointer to the renderer to use.
 */
void loop(SDL_Renderer* renderer) {
    SDL_Event event;
    screenProg initial = currentScreen;
    if (currentScreen == screenProg::CONN_ERROR) {
        game->setErrorScreen();
    }
    while (initial == currentScreen) {
        // input
        while (SDL_PollEvent(&event)) {
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.repeat == 0) {
                game->input(event);

                if (event.key.state == SDL_PRESSED) { //only when pressed, not released
                    switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        currentScreen = screenProg::EXIT;
                        break;
                    case SDLK_RETURN:
                        if (currentScreen == screenProg::MENU) {
                            currentScreen = screenProg::GAME;
                            game->setMenu();
                        }
                        if (currentScreen == screenProg::CONN_ERROR) {
                            currentScreen = screenProg::MENU;
                            game->setMenu();
                            game->setErrorScreen();
                            game->resetWin();
                        }
                        if (currentScreen == screenProg::GAME_OVER) {
                            currentScreen = screenProg::MENU;
                            game->setMenu();
                            game->resetWin();
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            if (event.type == SDL_QUIT) {
                currentScreen = screenProg::EXIT;
                break;
            }
        }
        //exit loop immediately if expected screen no longer equals the same as when the loop was started.
        if (currentScreen != initial) {
            break;
        }
        if (game->getWinCondition() != "0") {
            currentScreen = screenProg::GAME_OVER;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        game->update();

        game->render(renderer);

        SDL_RenderPresent(renderer);

        SDL_Delay(17);
    }
}

/**
 * Called from main, initialises the window and renderer. Only connect to server if User has
 * indicated that they want to.
 */
int run_game() {
    SDL_Window* window = SDL_CreateWindow(
        "Multiplayer Pong Client",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    if (nullptr == window) {
        std::cout << "Failed to create window" << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (nullptr == renderer) {
        std::cout << "Failed to create renderer" << SDL_GetError() << std::endl;
        return -1;
    }
    else {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

    game->setup(renderer);

    while (currentScreen != screenProg::EXIT) {
        loop(renderer);

        if (currentScreen == screenProg::EXIT) {
            break;
        }

        if (currentScreen == screenProg::GAME) {
            IPaddress ip;

            // Resolve host (ip name + port) into an IPaddress type
            if (SDLNet_ResolveHost(&ip, IP_NAME, PORT) == -1) {
                printf("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
                exit(3); 
            }

            // Open the connection to the server
            TCPsocket socket = SDLNet_TCP_Open(&ip);

            if (!socket) {
                printf("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
                currentScreen = screenProg::CONN_ERROR;
                if (SDLNet_GetError() != NULL) {
                    game->setErrorMessage(SDLNet_GetError());
                }
            }
            else {

                SDL_CreateThread(on_receive, "ConnectionReceiveThread", (void*)socket);
                SDL_CreateThread(on_send, "ConnectionSendThread", (void*)socket);

                loop(renderer);

                // Close connection to the server
                if (currentScreen == screenProg::EXIT || currentScreen == screenProg::GAME_OVER) {
                    game->send("CON_CLOSE");
                    sendTerminate(socket);

                    SDLNet_TCP_Close(socket);
                }
            }
        }
    }
    delete game;
    return 0;
}

int main(int argc, char** argv) {

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1) {
        printf("SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    // Initialise IMG
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG) {
        printf("IMG_Init: %s\n", IMG_GetError());
        exit(6);
    }

    // Initialize SDL_net
    if (SDLNet_Init() == -1) {
        printf("SDLNet_Init: %s\n", SDLNet_GetError());
        exit(2);
    }

    // Initialise TTF
    if (TTF_Init() == -1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        exit(5);
    }

    // Open audio device
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cout << Mix_GetError() << std::endl;
    }

    run_game();

    // Shutdown SDL_net
    SDLNet_Quit();
    // Shutdown SDL
    SDL_Quit();

    return 0;
}