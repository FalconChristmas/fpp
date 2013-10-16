#include <signal.h>
#include <sys/time.h>

#include "E131.h"
#include "log.h"
#include "ogg123.h"

float RefreshRate = 20.00;
int   usTimerValue = 0;
int   RunOnce = 0;

void HandleLightTimerEvent(void);

/*
 * Schedule the next timer event
 */
void SetLightTimer(int us)
{
	struct itimerval tout_val;
	tout_val.it_interval.tv_sec = 0;
	tout_val.it_interval.tv_usec = 0;
	tout_val.it_value.tv_sec = 0; 
	tout_val.it_value.tv_usec = us;
	setitimer(ITIMER_REAL, &tout_val,0);
	signal(SIGALRM,(__sighandler_t)HandleLightTimerEvent);
}

/*
 * Handle a timer event
 */
void HandleLightTimerEvent(void)
{
	if ((E131status != E131_STATUS_IDLE) ||
		(RunOnce))
	{
		if (RunOnce)
			RunOnce = 0;
		SetLightTimer(usTimerValue);
		E131_Send();
	}
}

/*
 * Calculate the timer delay
 */
float InitTimerDelay(void)
{
	usTimerValue = (unsigned int)(((float)1/(float)RefreshRate) * ((float)985000));
//  usTimerValue = (unsigned int)(((float)1/(float)RefreshRate) * ((float)1200000));
}

/*
 * Set a flag to run once send blanking data (all 0's) and fire the timer
 */
void SendBlankingData(void)
{
	InitTimerDelay();
	RunOnce = 1;

	SetLightTimer(usTimerValue);
}

/*
 * Kickstart the timer
 */
void InitLightTimer(void)
{
	InitTimerDelay();

	SetLightTimer(usTimerValue);
}

