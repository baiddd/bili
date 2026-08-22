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

QString ScreenManager::selectedGameRomPath() const { return m_selectedGameRomPath; }
void ScreenManager::setSelectedGameRomPath(const QString &romPath) {
    if (m_selectedGameRomPath == romPath) return;
    m_selectedGameRomPath = romPath;
    emit selectedGameChanged();
}
QString ScreenManager::selectedGameSystem() const { return m_selectedGameSystem; }
void ScreenManager::setSelectedGameSystem(const QString &system) {
    if (m_selectedGameSystem == system) return;
    m_selectedGameSystem = system;
    emit selectedGameChanged();
}
QString ScreenManager::selectedGameTitle() const { return m_selectedGameTitle; }
void ScreenManager::setSelectedGameTitle(const QString &title) {
    if (m_selectedGameTitle == title) return;
    m_selectedGameTitle = title;
    emit selectedGameChanged();
}
