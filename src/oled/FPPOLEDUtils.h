#pragma once

#include <list>
#include <memory>
#include <string>
#include <vector>

class FPPStatusOLEDPage;
class PinCapabilities;

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
            Action(const std::string& a, int min, int max, long long mai, const PinCapabilities* pin = nullptr) :
                action(a),
                actionValueMin(min),
                actionValueMax(max),
                minActionInterval(mai),
                nextActionTime(0),
                gpiodPin(pin) {}
            virtual ~Action() {}

            std::string action;
            int actionValueMin;
            int actionValueMax;
            long long minActionInterval;
            long long nextActionTime;

            virtual bool checkAction(int i, long long ntimeus);
            const PinCapabilities* gpiodPin = nullptr;
        };

        std::string pin;
        std::string mode;
        std::string edge;
        const PinCapabilities* gpiodPin = nullptr;
        int file = -1;
        int pollIndex = -1;
        std::list<Action*> actions;

        virtual const std::string& checkAction(int i, long long time);
    };

private:
    int _ledType;
    FPPStatusOLEDPage* statusPage;

    std::vector<InputAction*> actions;

    bool setupControlPin(const std::string& file);
    bool parseInputActions(const std::string& file);
    bool parseInputActionFromGPIO(const std::string& file);

    InputAction* configureGPIOPin(const std::string& pin,
                                  const std::string& mode,
                                  const std::string& edge,
                                  const std::string& name = "");

    void setInputFlag(const std::string& action);
    int inputFlags = 0;
};
