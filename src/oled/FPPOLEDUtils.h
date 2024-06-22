#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2023 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <list>
#include <string>
#include <vector>

class FPPStatusOLEDPage;

extern "C" {
struct gpiod_chip;
struct gpiod_line;
}

class FPPOLEDUtils {
public:
    FPPOLEDUtils(int ledType);
    ~FPPOLEDUtils();

    void run();
    void cleanup();

    class InputAction {
    public:
        InputAction() {}
        ~InputAction();

        class Action {
        public:
            Action(const std::string& a, int min, int max, long long mai) :
                action(a),
                actionValueMin(min),
                actionValueMax(max),
                minActionInterval(mai),
                nextActionTime(0) {}
            virtual ~Action() {}

            std::string action;
            int actionValueMin;
            int actionValueMax;
            long long minActionInterval;
            long long nextActionTime;

            virtual bool checkAction(int i, long long ntimeus);
        };

        std::string pin;
        std::string mode;
        std::string edge;
        int file = -1;
        int pollIndex = -1;
        std::list<Action*> actions;

        virtual const std::string& checkAction(int i, long long time);

        struct gpiod_line* gpiodLine = nullptr;
        int gpioChipIdx = -1;
        int gpioChipLine = -1;
        int kernelGPIO = -1;
    };

private:
    gpiod_chip* getChip(const std::string& n);

    std::vector<gpiod_chip*> gpiodChips;

    int _ledType;
    FPPStatusOLEDPage* statusPage;

    std::vector<InputAction*> actions;

    bool setupControlPin(const std::string& file);
    bool parseInputActions(const std::string& file);
    bool parseInputActionFromGPIO(const std::string& file);

    InputAction* configureGPIOPin(const std::string& pin,
                                  const std::string& mode,
                                  const std::string& edge);
    bool checkStatusAbility();

    void setInputFlag(const std::string& action);
    int inputFlags = 0;
};
