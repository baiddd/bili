#include "ScreenManager.h"

ScreenManager::ScreenManager(QObject *parent) : QObject(parent) {}

QString ScreenManager::currentScreen() const { return m_stack.last(); }

void ScreenManager::push(const QString &screenName) {
    m_stack.append(screenName);
    emit currentScreenChanged();
}

void ScreenManager::pop() {
    if (m_stack.size() > 1) {
        m_stack.removeLast();
        emit currentScreenChanged();
    }
}
