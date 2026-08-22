// app/GameFrameImageProvider.h
#pragma once
#include <QQuickImageProvider>
#include <QImage>

// Passe l'image figée du jeu (capturée par GameMenuOverlay juste avant
// d'ouvrir le menu en jeu) au QML du menu, sous l'URL
// "image://gameframe/<n>". C'est ce qui permet un VRAI voile translucide :
// l'image du jeu devient du contenu QML appartenant à Bili, que Qt compose
// lui-même avec le voile semi-transparent et le panneau, dans une seule
// surface -- au lieu de dépendre d'une transparence de fenêtre native
// entre deux fenêtres soeurs, qui ne fonctionne pas au-dessus de RetroArch
// (voir GameMenuOverlay.h).
//
// L'instance est donnée à un QQmlEngine via addImageProvider(), qui en
// prend la propriété -- ne pas la détruire soi-même.
class GameFrameImageProvider : public QQuickImageProvider {
public:
    GameFrameImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    // L'id de l'URL n'est qu'un numéro de révision servant à casser le cache
    // de QML (voir GameMenuOverlay::frameRevision) -- il n'y a jamais qu'une
    // seule image, la dernière capturée.
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        Q_UNUSED(id);
        Q_UNUSED(requestedSize);
        if (size) *size = m_frame.size();
        return m_frame;
    }

    void setFrame(const QImage &frame) { m_frame = frame; }
    bool hasFrame() const { return !m_frame.isNull(); }

private:
    QImage m_frame;
};
