#ifndef _PIXELNET_USB
#define _PIXELNET_USB

int  InitializeUSBDongle(void);
void SendUSBDongle(void);
int  IsUSBDongleActive(void);
void ShutdownUSBDongle(void);

#endif
