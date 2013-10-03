//
// events.h
#ifndef EVENTS_H_
#define EVENTS_H_

typedef struct fppevent {
	char  majorID;
	char  minorID;
	char *name;
	char *effect;
	int   startChannel;
	char *script;
} FPPevent;

int TriggerEvent(char major, char minor);
int TriggerEventByID(char *ID);

#endif
