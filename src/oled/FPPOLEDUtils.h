#ifndef  __FPPOLEDUTILS__
#define  __FPPOLEDUTILS__

#include <list>
#include <vector>
#include <string>

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
            Action(const std::string &a, int min, int max, long long mai)
                : action(a), actionValueMin(min), actionValueMax(max), minActionInterval(mai), nextActionTime(0) {}
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
        std::list<Action *> actions;
        
        const std::string &checkAction(int i, long long time);
        
        struct gpiod_line *gpiodLine = nullptr;
        int gpioChipIdx = -1;
        int gpioChipLine = -1;
        int kernelGPIO = -1;
    };

private:
    gpiod_chip *getChip(const std::string &n);
    
    std::vector<gpiod_chip*> gpiodChips;

    int _ledType;
    FPPStatusOLEDPage *statusPage;
    
    std::vector<InputAction*> actions;

    bool setupControlPin(const std::string &file);
    bool parseInputActions(const std::string &file);
    bool parseInputActionFromGPIO(const std::string &file);
    
    InputAction* configureGPIOPin(const std::string &pin,
                                  const std::string &mode,
                                  const std::string &edge);
    bool checkStatusAbility();
};

#endif
