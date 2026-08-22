// core/emulators/RetroArchNetworkCommand.h
#pragma once
#include <QString>
#include <QtGlobal>

// Envoie une commande texte à une instance RetroArch en cours d'exécution
// via son interface de commandes réseau officielle (UDP, protocole ASCII
// une commande par ligne) -- confirmée sur
// docs.libretro.com/development/retroarch/network-control-interface/
// pendant le brainstorming, pas devinée. RetroArch doit avoir
// network_cmd_enable=true et network_cmd_port fixé dans son retroarch.cfg
// (voir EmulatorProvider::writePortableRetroArchConfig()) pour écouter.
class RetroArchNetworkCommand {
public:
    static constexpr quint16 kDefaultPort = 55355;

    // Envoie command (ex. "PAUSE_TOGGLE", "QUIT") en UDP vers
    // 127.0.0.1:port. Retourne true si l'envoi local a réussi (le
    // protocole est fire-and-forget -- aucune confirmation que RetroArch
    // a effectivement reçu/exécuté la commande n'est possible par ce
    // canal).
    bool send(const QString &command, quint16 port = kDefaultPort);
};
