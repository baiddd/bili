#include "GamepadBridge.h"
#include <SDL.h>

GamepadBridge::GamepadBridge(InputManager *inputManager, QObject *parent)
    : QObject(parent), m_inputManager(inputManager) {}

GamepadBridge::~GamepadBridge() { stop(); }

void GamepadBridge::start() {
    if (m_running) return;
    m_running = true;

    QObject *worker = new QObject();
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, worker, [this]() { pollLoop(); });
    m_thread.start();
}

void GamepadBridge::stop() {
    if (!m_running) return;
    m_running = false;
    m_thread.quit();
    m_thread.wait();
}

void GamepadBridge::pollLoop() {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    SDL_Event event;
    while (m_running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        emit m_inputManager->navigateUp(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        emit m_inputManager->navigateDown(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        emit m_inputManager->navigateLeft(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        emit m_inputManager->navigateRight(); break;
                    case SDL_CONTROLLER_BUTTON_A:
                        emit m_inputManager->accept(); break;
                    case SDL_CONTROLLER_BUTTON_B:
                        emit m_inputManager->cancel(); break;
                    case SDL_CONTROLLER_BUTTON_START:
                        emit m_inputManager->menu(); break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        emit m_inputManager->capture(); break;
                    default: break;
                }
            }
        }
        SDL_Delay(8); // ~120Hz poll
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
}
