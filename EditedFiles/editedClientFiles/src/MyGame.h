#ifndef __MY_GAME_H__
#define __MY_GAME_H__

#include <iostream>
#include <vector>
#include <string>

#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_mixer.h"
#include "SDL_image.h"

//structure of game data.
static struct GameData {
    std::string countdown = "3";
    std::string playerWin = "0";
} game_data;

//structure of data relating to errors.
static struct ErrorData {
    bool errorScreen = false; //should we render the error screen for connection
    std::string errorMessage; //what is the current error
} currError;

//Entity Parent class.
class Entity {
protected:
    SDL_Rect bounds = { 0, 0, 0, 0 };
    SDL_Texture* texture;
public:
    ~Entity() {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    void setX(int xPos) {
        bounds.x = xPos;
    }
    void setY(int yPos) {
        bounds.y = yPos;
    }
    void setTex(SDL_Texture* surf) {
        texture = surf;
    }

    int getX() {
        return bounds.x;
    }
    int getY() {
        return bounds.y;
    }
    SDL_Rect getR() {
        return bounds;
    }
    SDL_Texture* getTex() {
        return texture;
    }
};

//Player Child class. Inherits from Entity.
class Player : public Entity {
protected:
    std::string score = "0";
public:
    Player() {
        bounds.w = 20;
        bounds.h = 60;
    }
    void setScore(std::string newScore) {
        score = newScore;
    }
    std::string getScore() {
        return score;
    }
};

//Ball Child class. Inherits from Entity.
class Ball : public Entity {
public:
    Ball() {
        bounds.w = 15;
        bounds.h = 15;
    }
};

class MyGame {
    private:
        TTF_Font* fontTitle; //font settings for title
        TTF_Font* fontInfo; //font settings for info text
        Mix_Chunk* batHit; //sound loaded for ball hit bat
        Mix_Chunk* wallHit; //sound loaded for ball hit wall
        Mix_Chunk* countdownClick; //sound loaded for countdown tick
        SDL_Color textColour{ 255, 255, 255, 255 }; //default colour to be used for text

        SDL_Surface* fileImgLoad; //load ball image
        SDL_Texture* tPongBall; //texture from ball image
        SDL_Texture* tPongBat; //texture from bat image

        double ballAngle = 0.0; //angle to rotate ball texture
        bool menu = true; //should we render the menu

        std::vector<SDL_Rect*> ballTrail; //data structure used to store the particles for trail of ball

        enum class clientRole { //enum class designating client role
            ONE, TWO, SPECTATOR, NONE
        };
        clientRole thisClient; //current role of client.

        Player* playerOne; //pointer to player 1 of class player
        Player* playerTwo; //pointer to player 2 of class player
        Ball ball;

    public:
        ~MyGame();

        std::vector<std::string> messages;

        //Functions found in original code
        void on_receive(std::string message, std::vector<std::string>& args);
        void send(std::string message);
        void input(SDL_Event& event);
        void setup(SDL_Renderer* renderer);
        void update();
        void render(SDL_Renderer* renderer);

        //functions created during project
        void printMessage(std::string cmd, std::vector<std::string>& args);
        void setMenu();
        void setErrorScreen();
        void setErrorMessage(std::string error);
        void resetWin();
        void ballParticle(SDL_Renderer* renderer);
        void drawText(SDL_Renderer* renderer, int x, int y, std::string text, TTF_Font * selectedFont);

        std::string getWinCondition();
};

#endif