#pragma once
#include <QObject>
#include <QThread>
#include <atomic>
#include "InputManager.h"

class GamepadBridge : public QObject {
    Q_OBJECT
public:
    explicit GamepadBridge(InputManager *inputManager, QObject *parent = nullptr);
    ~GamepadBridge() override;
    void start();
    void stop();

private:
    void pollLoop();

    InputManager *m_inputManager;
    QThread m_thread;
    std::atomic<bool> m_running{false};
};
