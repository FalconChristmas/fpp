//
// events.h
#ifndef EVENTS_H_
#define EVENTS_H_

typedef struct fppevent {
	char *name;
	int   id;
	char *effect;
	int   startChannel;
	char *script;
} FPPevent;

int TriggerEvent(char *eventName);

#endif
