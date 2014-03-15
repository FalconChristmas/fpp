#ifndef _MEMORYMAP_H
#define _MEMORYMAP_H

int InitializeChannelDataMemoryMap(void);
void CloseChannelDataMemoryMap(void);
int UsingMemoryMapInput(void);
void OverlayMemoryMap(char *channelData);

#endif /* _MEMORYMAP_H */
