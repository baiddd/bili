#pragma once
#include <QString>

// Translates an SDL2 GameController mapping string (as returned by
// SDL_GameControllerMapping()/SDL_GameControllerMappingForDeviceIndex(), see
// SDL2's own SDL_gamecontroller.h) into the contents of a RetroArch
// autoconfig .cfg file (see docs.libretro.com/guides/controller-autoconfiguration),
// so a connected gamepad gets a working default RetroPad mapping without the
// user having to configure it by hand.
class RetroArchAutoconfig {
public:
    // controllerName is the display name to write into the file's
    // input_device field (SDL_GameControllerNameForIndex()'s result --
    // *not* necessarily the same string as the "name" field embedded inside
    // sdlMapping, which this function ignores in favor of the caller's own
    // value). sdlMapping is the full "GUID,name,key:value,..." string SDL2
    // returns.
    static QString buildProfile(const QString &controllerName, const QString &sdlMapping);
};
