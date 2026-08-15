#include "StubEmulatorProvider.h"

QString StubEmulatorProvider::installRetroArch() {
    return "RetroArch : non implémenté (sous-projet 3)";
}
QString StubEmulatorProvider::installCore(const QString &system) {
    return "Core " + system + " : non implémenté (sous-projet 3)";
}
QString StubEmulatorProvider::installStandaloneEmulator(const QString &name) {
    return "Émulateur " + name + " : non implémenté (sous-projet 3)";
}
