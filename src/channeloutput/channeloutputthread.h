#ifndef _CHANNELOUTPUTTHREAD_H
#define _CHANNELOUTPUTTHREAD_H

int  ChannelOutputThreadIsRunning(void);
int  StartChannelOutputThread(void);
void CalculateNewChannelOutputDelay(float mediaPosition);

#endif
