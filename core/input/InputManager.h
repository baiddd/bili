#pragma once
#include <QObject>

class InputManager : public QObject {
    Q_OBJECT
public:
    explicit InputManager(QObject *parent = nullptr);
    Q_INVOKABLE void handleKeyPress(int key);

signals:
    void navigateUp();
    void navigateDown();
    void navigateLeft();
    void navigateRight();
    void accept();
    void cancel();
    void menu();
    void capture();
    void homeMenuRequested();
};
