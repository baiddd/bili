#include <QtTest>
#include "emulators/RetroArchAutoconfig.h"

class RetroArchAutoconfigTest : public QObject {
    Q_OBJECT
private slots:
    // Fixture copied verbatim (see research notes in task-7-report.md) from
    // the Windows/"xinput"-family "Xbox 360 Controller" entries in SDL2's
    // own bundled gamecontrollerdb.txt (mdqinc/SDL_GameControllerDB, the
    // upstream project SDL2 vendors this database from), unmodified. It
    // exercises every distinct value form buildProfile() has to translate:
    // plain digital buttons ("bN"), a hat-based d-pad ("hX.Y"), and plain
    // full axes used both for the analog sticks and for the triggers
    // ("aN").
    void buildsAProfileContainingTheControllerName() {
        const QString sdlMapping =
            "03000000380700001647000000000000,Xbox 360 Controller,"
            "a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,"
            "leftshoulder:b4,leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,"
            "rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,"
            "start:b7,x:b2,y:b3,platform:Windows,";
        const QString profile = RetroArchAutoconfig::buildProfile("Xbox 360 Controller", sdlMapping);

        QVERIFY(profile.contains("input_device = \"Xbox 360 Controller\""));
        QVERIFY(profile.contains("input_driver = \"sdl2\""));

        // Face buttons and start/select/shoulders/stick-clicks: plain "bN"
        // SDL values translate to RetroArch's plain decimal button index.
        QVERIFY(profile.contains("input_a_btn = \"0\""));
        QVERIFY(profile.contains("input_b_btn = \"1\""));
        QVERIFY(profile.contains("input_x_btn = \"2\""));
        QVERIFY(profile.contains("input_y_btn = \"3\""));
        QVERIFY(profile.contains("input_select_btn = \"6\""));
        QVERIFY(profile.contains("input_start_btn = \"7\""));
        QVERIFY(profile.contains("input_l_btn = \"4\""));
        QVERIFY(profile.contains("input_r_btn = \"5\""));
        QVERIFY(profile.contains("input_l3_btn = \"8\""));
        QVERIFY(profile.contains("input_r3_btn = \"9\""));

        // D-pad: SDL's hat notation "hX.Y" (Y = SDL_HAT_* bitmask) becomes
        // RetroArch's "h<index><direction>" digital-hat key value.
        QVERIFY(profile.contains("input_up_btn = \"h0up\""));
        QVERIFY(profile.contains("input_down_btn = \"h0down\""));
        QVERIFY(profile.contains("input_left_btn = \"h0left\""));
        QVERIFY(profile.contains("input_right_btn = \"h0right\""));

        // Triggers: a plain "aN" axis becomes a "+N" RetroArch axis value
        // (SDL always normalizes trigger axes to 0..max, never negative).
        QVERIFY(profile.contains("input_l2_axis = \"+2\""));
        QVERIFY(profile.contains("input_r2_axis = \"+5\""));

        // Analog sticks: a plain "aN" axis splits into a same-index
        // plus/minus RetroArch key pair.
        QVERIFY(profile.contains("input_l_x_plus_axis = \"+0\""));
        QVERIFY(profile.contains("input_l_x_minus_axis = \"-0\""));
        QVERIFY(profile.contains("input_l_y_plus_axis = \"+1\""));
        QVERIFY(profile.contains("input_l_y_minus_axis = \"-1\""));
        QVERIFY(profile.contains("input_r_x_plus_axis = \"+3\""));
        QVERIFY(profile.contains("input_r_x_minus_axis = \"-3\""));
        QVERIFY(profile.contains("input_r_y_plus_axis = \"+4\""));
        QVERIFY(profile.contains("input_r_y_minus_axis = \"-4\""));
    }
};

QTEST_MAIN(RetroArchAutoconfigTest)
#include "RetroArchAutoconfigTest.moc"
