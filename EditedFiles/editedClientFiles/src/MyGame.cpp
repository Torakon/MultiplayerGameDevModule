#include "MyGame.h"

/**
 * Called by Main on_receive, reacts to arguments sent by server.
 * @param cmd String of characters found before first ",".
 * @param args structure of strings found after first string. Split by ",".
 */
void MyGame::on_receive(std::string cmd, std::vector<std::string>& args) {
    //Print cmd and subsequent args if not explicitly GAME_DATA
    if (cmd != "GAME_DATA") {
        printMessage(cmd, args);
    }
    //React to args
    if (cmd.find("GAME_DATA") != std::string::npos) {
        if (args.size() == 5) {
            playerOne->setY(stoi(args.at(0)));
            playerTwo->setY(stoi(args.at(1)));
            ball.setX(stoi(args.at(2)));
            ball.setY(stoi(args.at(3)));
            if (args.at(4) == "1WIN") {
                std::cout << args.at(4) << std::endl;
                game_data.playerWin = "1";
            }
            else if (args.at(4) == "2WIN") {
                std::cout << args.at(4) << std::endl;
                game_data.playerWin = "2";

            }
        }
    }

    if (cmd.find("BALL_HIT_BAT") != std::string::npos) {
        Mix_PlayChannel(1, batHit, 0);
    }

    if (cmd.find("HIT_WALL") != std::string::npos) {
        Mix_PlayChannel(1, wallHit, 0);
        if (args.size() == 2) {
            playerOne->setScore(args.at(0).c_str());
            playerTwo->setScore(args.at(1).c_str());
        }
    }

    if (cmd.find("ROLE") != std::string::npos) {
        switch (stoi(args.at(0))) {
        case 1:
            thisClient = clientRole::ONE;
            break;
        case 2:
            thisClient = clientRole::TWO;
            break;
        case 3:
            thisClient = clientRole::SPECTATOR;
            break;
        default:
            std::cout << "Error assigning client role." << std::endl;
            break;
        }
    }

    if (cmd.find("COUNT") != std::string::npos) {
        game_data.countdown = args.at(0);
        Mix_PlayChannel(-1, countdownClick, 0);
    }

    if (cmd.find("CONN_CHECK") != std::string::npos) {
        std::cout << "Sending reply to server Active Check." << std::endl;
        send("CONFIRM");
    }
}

/**
 * Print out the command received followed by the arguments.
 * @param cmd the first 'token' received by Client.
 * @param args vector of 'tokens' received by Client, seperated by ",".
 */
void MyGame::printMessage(std::string cmd, std::vector<std::string>& args) {
    std::cout << "Command: " << cmd << std::endl;
    std::cout << "args: ";
    for (std::string s : args) {
        std::cout << s << ", ";
    }
    std::cout << std::endl;
}

/**
 * Pushes a string onto the vector of strings: "messages".
 * @param message data to be sent to server.
 */
void MyGame::send(std::string message) {
    messages.push_back(message);
}

/**
 * Passes a string to send() dependant on key event.
 * @param event SDL_Event
 */
void MyGame::input(SDL_Event& event) {
    switch (event.key.keysym.sym) {
        case SDLK_w:
            send(event.type == SDL_KEYDOWN ? "W_DOWN" : "W_UP");
            break;
        case SDLK_s:
            send(event.type == SDL_KEYDOWN ? "S_DOWN" : "S_UP");
            break;
        case SDLK_RETURN:
            if (event.key.state == SDL_PRESSED) {
                if (thisClient == clientRole::SPECTATOR) {
                    send("TAKE_OVER");
                }
            }
            break;
        default:
            break;
    }
}

/**
 * Run once, loads visual and auditory assets. Initialise Players and set relevant X.
 * @param renderer pointer to renderer in use.
 */
void MyGame::setup(SDL_Renderer* renderer) {
    //initialise Entities
    playerOne = new Player();
    playerTwo = new Player();
    playerOne->setX(800 / 4);
    playerTwo->setX(3 * 800 / 4 - 20);

    //load visual assets
    fontTitle = TTF_OpenFont("../res/font/pong.ttf", 64);
    if (nullptr == fontTitle) {
        std::cout << TTF_GetError() << std::endl;
    }
    fontInfo = TTF_OpenFont("../res/font/pong.ttf", 18);
    if (nullptr == fontInfo) {
        std::cout << TTF_GetError() << std::endl;
    }
    fileImgLoad = IMG_Load("../res/img/pBallv2.png");
    if (fileImgLoad == nullptr) {
        std::cout << "Error loading PNG Pong Ball: " << IMG_GetError() << std::endl;
    }
    else {
        ball.setTex(SDL_CreateTextureFromSurface(renderer, fileImgLoad));
        SDL_FreeSurface(fileImgLoad);
    }
    fileImgLoad = IMG_Load("../res/img/pBat.png");
    if (fileImgLoad == nullptr) {
        std::cout << "Error loading PNG Pong Bat: " << IMG_GetError() << std::endl;
    }
    else {
        playerOne->setTex(SDL_CreateTextureFromSurface(renderer, fileImgLoad));
        playerTwo->setTex(SDL_CreateTextureFromSurface(renderer, fileImgLoad));
        SDL_FreeSurface(fileImgLoad);
    }

    //load auditory assets
    batHit = Mix_LoadWAV("../res/sounds/hit_bat.wav");
    if (nullptr == batHit) {
        std::cout << "Bat Hit Sound: " << Mix_GetError() << std::endl;
    }
    else {
        batHit->volume = 20;
    }
    wallHit = Mix_LoadWAV("../res/sounds/hit_wall.wav");
    if (nullptr == wallHit) {
        std::cout << "Wall Hit Sound: " << Mix_GetError() << std::endl;
    }
    else {
        wallHit->volume = 20;
    }
    countdownClick = Mix_LoadWAV("../res/sounds/click.wav");
    if (nullptr == countdownClick) {
        std::cout << "Countdown Sound: " << Mix_GetError() << std::endl;
    }
    else {
        countdownClick->volume = 20;
    }
}

void MyGame::update() {
    if (!menu) { //used purely for visual effect, synchronisation between clients not needed
        ballAngle += 5;
        if (ballAngle > 360) {
            ballAngle = 0;
        }
    }
}

/**
 * Render relevant information.
 * @param renderer pointer to the renderer in use.
 */
void MyGame::render(SDL_Renderer* renderer) {
    if (menu) { //menu information
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        drawText(renderer, 400, 100, "Welcome to Pong", fontTitle);
        drawText(renderer, 400, 300, "Connect to localhost? ENTER to confirm.", fontInfo);
        drawText(renderer, 400, 525, "Has a Player disconnected? Press ENTER as a Spectator to take over.", fontInfo);
    } 
    else if (currError.errorScreen) { //show error to user
        drawText(renderer, 400, 100, "ERROR:", fontTitle);
        drawText(renderer, 400, 300, currError.errorMessage, fontInfo);
        drawText(renderer, 400, 525, "Press ENTER to return to the menu.", fontInfo);
    }
    else if (game_data.playerWin != "0") { //show game over
        std::string winnerText = "Player ";
        winnerText.append(game_data.playerWin);
        winnerText.append(" was the winner!");
        drawText(renderer, 400, 100, "GAME OVER", fontTitle);
        drawText(renderer, 400, 300, winnerText, fontInfo);
        drawText(renderer, 400, 525, "press ENTER to return to the menu.", fontInfo);
    }
    else { //render actual game
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderCopy(renderer, playerOne->getTex(), 0, &playerOne->getR());
        SDL_RenderCopy(renderer, playerTwo->getTex(), 0, &playerTwo->getR());
        ballParticle(renderer);
        SDL_RenderCopyEx(renderer, ball.getTex(), 0, &ball.getR(), ballAngle, 0, SDL_RendererFlip::SDL_FLIP_NONE);
        drawText(renderer, 100, 100, playerOne->getScore(), fontTitle);
        drawText(renderer, 600, 100, playerTwo->getScore(), fontTitle);

        if (game_data.countdown != "0") {
            drawText(renderer, 400, 400, game_data.countdown, fontTitle);
        }

        switch (thisClient) {
            case clientRole::ONE:
                drawText(renderer, 400, 30, "Player One", fontTitle);
                break;
            case clientRole::TWO:
                drawText(renderer, 400, 30, "Player Two", fontTitle);
                break;
            case clientRole::SPECTATOR:
                drawText(renderer, 400, 30, "Spectator", fontTitle);
                break;
            default:
                break;
        }
    }
}

/**
 * Flips if menu should be shown
 */
void MyGame::setMenu() {
    menu = !menu;
}

/**
 * Flips if error screen should be shown.
 */
void MyGame::setErrorScreen() {
    currError.errorScreen = !currError.errorScreen;
}

/**
 * Sets the message to be shown to User on error.
 * @param error The error to be shown.
 */
void MyGame::setErrorMessage(std::string error) {
    currError.errorMessage = error;
}

/**
 * Resets stored values for server reconnect.
 */
void MyGame::resetWin() {
    thisClient = clientRole::NONE;
    game_data.playerWin = "0";
    playerOne->setScore("0");
    playerTwo->setScore("0");
    ballTrail.clear();
}

/**
 * Rudimentary method to create and render "particles" to trail the ball.
 * @param renderer pointer to the renderer in use.
 */
void MyGame::ballParticle(SDL_Renderer* renderer) {
    SDL_Rect * particle = new SDL_Rect{ ball.getX(), ball.getY(), 10, 10 };
    ballTrail.insert(ballTrail.begin(), particle);
    if (ballTrail.size() > 50) {
        ballTrail.erase(ballTrail.end()-1);
    }
    int alpha = 255;
    for (SDL_Rect * r : ballTrail) {
        alpha -= 5;
        SDL_SetRenderDrawColor(renderer, 175, 175, 175, alpha);
        SDL_RenderFillRect(renderer, r);
        if ((rand() % 2) == 1) {
            r->x += 2;
        }
        else {
            r->x -= 2;
        }
        if ((rand()) % 2 == 1) {
            r->y -= 2;
        }
    }
}

/**
 * Called from render(), converts a surface and creates subsequent texture from provided text.
 * @param renderer pointer to the renderer in use
 * @param x horizontal position to render text
 * @param y vertical position to render text
 * @param text the string to render
 * @param selectedFont the TTF font to use
 */
void MyGame::drawText(SDL_Renderer* renderer, int x, int y, std::string text, TTF_Font * selectedFont) {
    SDL_Texture* textText = nullptr;
    SDL_Surface* textSurf = TTF_RenderText_Blended(selectedFont, text.c_str(), textColour);
    if (textSurf != nullptr) {
        textText = SDL_CreateTextureFromSurface(renderer, textSurf);
        SDL_FreeSurface(textSurf);
    }
    else {
        std::cout << "Failed to create texture from string: " << text << std::endl;
        std::cout << TTF_GetError() << std::endl;
    }
    int w, h;
    SDL_QueryTexture(textText, 0, 0, &w, &h);
    SDL_Rect dst{ x - (w/2), y - (h/2), w, h };
    SDL_RenderCopyEx(renderer, textText, 0, &dst, 0.0, 0, SDL_RendererFlip::SDL_FLIP_NONE);
    SDL_DestroyTexture(textText);
}

/**
 * Gets which player has been registered as winning.
 * @return
 */
std::string MyGame::getWinCondition() {
    return game_data.playerWin;
}

MyGame::~MyGame() {
    std::cout << "Deleting MyGame instance" << std::endl;
    delete playerOne;
    delete playerTwo;
    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontInfo);
    Mix_FreeChunk(wallHit);
    Mix_FreeChunk(batHit);
    ballTrail.clear();
}