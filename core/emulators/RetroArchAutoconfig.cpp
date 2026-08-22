#include "RetroArchAutoconfig.h"
#include <QMap>
#include <QStringList>

namespace {

// SDL_GameControllerMapping()'s per-field value syntax (SDL2's own
// SDL_gamecontroller.h documents the three base forms -- "bX" a joystick
// button, "hX.Y" hat X with SDL_HAT_* bitmask Y, "aX" an axis -- and SDL2's
// own bundled gamecontrollerdb.txt, verified directly, additionally uses a
// leading "+"/"-" on an axis for a half-axis and a trailing "~" to invert
// it, e.g. "lefttrigger:+a2", "dpleft:-a0", "lefty:a1~").
//
// Returns the plain decimal button index for "bX", or RetroArch's own
// "h<index><direction>" digital-hat notation (confirmed against
// docs.libretro.com/guides/controller-autoconfiguration and real bundled
// .cfg files) for a "hX.Y" whose Y is a single cardinal direction. Returns
// an empty string for anything else (an axis value, or a diagonal/combined
// hat value RetroArch's autoconfig format has no single button-key spelling
// for) so the caller can skip emitting a line rather than guess one.
QString formatDigitalValue(const QString &value) {
    if (value.startsWith('b')) {
        const QString digits = value.mid(1);
        bool ok = false;
        digits.toInt(&ok);
        return ok ? digits : QString();
    }
    if (value.startsWith('h')) {
        const int dot = value.indexOf('.');
        if (dot <= 1) return QString();
        const QString hatIndex = value.mid(1, dot - 1);
        bool ok = false;
        const int mask = value.mid(dot + 1).toInt(&ok);
        if (!ok) return QString();
        QString direction;
        switch (mask) {
            // SDL_HAT_UP/RIGHT/DOWN/LEFT bitmask values, confirmed in SDL2's
            // own SDL_joystick.h (SDL_HAT_UP=0x01, SDL_HAT_RIGHT=0x02,
            // SDL_HAT_DOWN=0x04, SDL_HAT_LEFT=0x08).
            case 1: direction = "up"; break;
            case 2: direction = "right"; break;
            case 4: direction = "down"; break;
            case 8: direction = "left"; break;
            default: return QString();
        }
        return "h" + hatIndex + direction;
    }
    return QString();
}

// Extracts the bare axis index from an "aX"/"+aX"/"-aX"/"aX~" SDL mapping
// value, or an empty string if value isn't an axis reference at all (e.g.
// it's a "bX" button, for a control that this particular controller maps as
// a digital button instead of an axis).
QString axisIndexOf(const QString &value) {
    QString v = value;
    if (v.startsWith('+') || v.startsWith('-')) v = v.mid(1);
    if (!v.startsWith('a')) return QString();
    v = v.mid(1);
    if (v.endsWith('~')) v.chop(1);
    bool ok = false;
    v.toInt(&ok);
    return ok ? v : QString();
}

} // namespace

QString RetroArchAutoconfig::buildProfile(const QString &controllerName, const QString &sdlMapping) {
    // SDL_gamecontroller.h documents SDL_GameControllerMapping()'s string
    // format as "GUID,name,mappings" (mappings being the remaining
    // comma-separated "key:value" list, which also carries a trailing,
    // non-input "platform:<OS name>" entry -- skipped below along with any
    // other key this function doesn't recognize). The GUID/name fields
    // themselves are skipped in favor of the caller's own controllerName,
    // since that's what EmulatorProvider actually has on hand via
    // SDL_GameControllerNameForIndex().
    QMap<QString, QString> sdlFields;
    const QStringList parts = sdlMapping.split(',');
    for (int i = 2; i < parts.size(); ++i) {
        const QString &part = parts.at(i);
        const int colon = part.indexOf(':');
        if (colon <= 0) continue; // empty trailing field from a trailing comma, or malformed
        sdlFields.insert(part.left(colon), part.mid(colon + 1));
    }

    QString profile;
    profile += "input_device = \"" + controllerName + "\"\n";
    profile += "input_driver = \"sdl2\"\n";

    // RetroArch's autoconfig button-key names, confirmed against
    // docs.libretro.com/guides/controller-autoconfiguration and real .cfg
    // files bundled in libretro/retroarch-joypad-autoconfig's sdl2/
    // directory. This is a default/best-effort profile (not a
    // hand-calibrated one), so RetroPad's face buttons and the rest of the
    // layout map onto SDL_GameControllerMapping()'s own key names 1:1.
    static const QMap<QString, QString> kButtonKeys = {
        {"a", "input_a_btn"}, {"b", "input_b_btn"},
        {"x", "input_x_btn"}, {"y", "input_y_btn"},
        {"back", "input_select_btn"}, {"start", "input_start_btn"},
        {"guide", "input_menu_toggle_btn"},
        {"leftshoulder", "input_l_btn"}, {"rightshoulder", "input_r_btn"},
        {"leftstick", "input_l3_btn"}, {"rightstick", "input_r3_btn"},
        {"dpup", "input_up_btn"}, {"dpdown", "input_down_btn"},
        {"dpleft", "input_left_btn"}, {"dpright", "input_right_btn"},
    };
    // leftx/lefty/rightx/righty each split into a "_plus_axis"/"_minus_axis"
    // pair sharing the same underlying axis index (confirmed from the same
    // bundled .cfg files, e.g. input_l_x_plus_axis = "+0" alongside
    // input_l_x_minus_axis = "-0").
    static const QMap<QString, QString> kStickAxisPrefixes = {
        {"leftx", "input_l_x"}, {"lefty", "input_l_y"},
        {"rightx", "input_r_x"}, {"righty", "input_r_y"},
    };

    for (auto it = sdlFields.constBegin(); it != sdlFields.constEnd(); ++it) {
        const QString &key = it.key();
        const QString &value = it.value();

        if (kButtonKeys.contains(key)) {
            const QString btnValue = formatDigitalValue(value);
            if (!btnValue.isEmpty()) {
                profile += kButtonKeys.value(key) + " = \"" + btnValue + "\"\n";
            }
            continue;
        }

        if (key == "lefttrigger" || key == "righttrigger") {
            // SDL2's own bundled gamecontrollerdb.txt maps triggers as
            // either a digital button ("lefttrigger:b6") or an axis
            // ("lefttrigger:a2") depending on the physical pad -- RetroArch
            // has a distinct _btn/_axis key for exactly this reason.
            const QString retroPrefix = (key == "lefttrigger") ? "input_l2" : "input_r2";
            const QString digital = formatDigitalValue(value);
            if (!digital.isEmpty()) {
                profile += retroPrefix + "_btn = \"" + digital + "\"\n";
                continue;
            }
            const QString axisIndex = axisIndexOf(value);
            if (!axisIndex.isEmpty()) {
                // SDL2's SDL_gamecontroller.h documents that
                // SDL_GameControllerGetAxis() always normalizes trigger axes
                // to "0 (released) to SDL_JOYSTICK_AXIS_MAX (fully
                // pressed)... never return[ing] a negative value", whatever
                // raw sign the underlying joystick axis has -- so the "+"
                // sign RetroArch's own bundled trigger .cfg entries always
                // use (e.g. input_l2_axis = "+4") is the correct
                // translation regardless of any "+"/"-"/"~" the SDL mapping
                // string's own value carries.
                profile += retroPrefix + "_axis = \"+" + axisIndex + "\"\n";
            }
            continue;
        }

        if (kStickAxisPrefixes.contains(key)) {
            const QString axisIndex = axisIndexOf(value);
            if (!axisIndex.isEmpty()) {
                const QString prefix = kStickAxisPrefixes.value(key);
                profile += prefix + "_plus_axis = \"+" + axisIndex + "\"\n";
                profile += prefix + "_minus_axis = \"-" + axisIndex + "\"\n";
            }
            continue;
        }

        // Unrecognized SDL mapping key (e.g. "platform", or a control this
        // task's confirmed RetroArch key set doesn't cover) -- skip rather
        // than guess at a translation.
    }

    return profile;
}
