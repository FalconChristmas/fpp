#define COMMAND_FAILED        0   
#define COMMAND_SUCCESS       1   

void Command_Initialize();
void CloseCommand();
void Commandproc();
void ProcessCommand();
void exit_handler(int signum);
