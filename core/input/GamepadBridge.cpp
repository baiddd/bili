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

namespace {
// SDL2 axis values range -32768..32767; a real analog stick never rests
// exactly at 0, so a wide dead zone is needed to avoid drift falsely
// registering as a navigation press. Chosen at roughly half of full
// deflection -- deliberate enough to require an actual push, not a graze.
constexpr Sint16 kAxisDeadzone = 16000;
}

void GamepadBridge::pollLoop() {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    // SDL2 only generates SDL_CONTROLLERBUTTONDOWN events for devices that
    // have been explicitly opened with SDL_GameControllerOpen() - open every
    // controller already connected at startup, then handle hot-plug via
    // SDL_CONTROLLERDEVICEADDED/REMOVED below.
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameControllerOpen(i);
        }
    }

    // Bug fix (manual testing, real DualSense controller): only the D-pad
    // was ever handled -- the left analog stick generates
    // SDL_CONTROLLERAXISMOTION events instead of SDL_CONTROLLERBUTTONDOWN,
    // which this loop never listened for at all, so stick-based navigation
    // was a complete no-op. Tracked as a discrete -1/0/1 state per axis
    // (not the raw analog value) so a signal fires only once per crossing
    // into/out of the dead zone -- matching the D-pad's one-emit-per-press
    // behavior instead of spamming navigate* every ~8ms while the stick is
    // held over.
    int leftXState = 0;
    int leftYState = 0;

    SDL_Event event;
    while (m_running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERDEVICEADDED) {
                // event.cdevice.which is a device index for this event type.
                SDL_GameControllerOpen(event.cdevice.which);
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                // event.cdevice.which is an instance ID for this event type.
                if (SDL_GameController *controller =
                        SDL_GameControllerFromInstanceID(event.cdevice.which)) {
                    SDL_GameControllerClose(controller);
                }
            } else if (event.type == SDL_CONTROLLERAXISMOTION) {
                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                    int newState = 0;
                    if (event.caxis.value > kAxisDeadzone) newState = 1;
                    else if (event.caxis.value < -kAxisDeadzone) newState = -1;
                    if (newState != leftXState) {
                        leftXState = newState;
                        if (newState == 1) emit m_inputManager->navigateRight();
                        else if (newState == -1) emit m_inputManager->navigateLeft();
                    }
                } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                    int newState = 0;
                    if (event.caxis.value > kAxisDeadzone) newState = 1;
                    else if (event.caxis.value < -kAxisDeadzone) newState = -1;
                    if (newState != leftYState) {
                        leftYState = newState;
                        if (newState == 1) emit m_inputManager->navigateDown();
                        else if (newState == -1) emit m_inputManager->navigateUp();
                    }
                }
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
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
