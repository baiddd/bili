#include "InputManager.h"
#include <Qt>

InputManager::InputManager(QObject *parent) : QObject(parent) {}

void InputManager::handleKeyPress(int key) {
    switch (key) {
        case Qt::Key_Up: emit navigateUp(); break;
        case Qt::Key_Down: emit navigateDown(); break;
        case Qt::Key_Left: emit navigateLeft(); break;
        case Qt::Key_Right: emit navigateRight(); break;
        case Qt::Key_Return:
        case Qt::Key_Enter: emit accept(); break;
        case Qt::Key_Escape: emit cancel(); break;
        case Qt::Key_M: emit menu(); break;
        case Qt::Key_F12: emit capture(); break;
        default: break;
    }
}
