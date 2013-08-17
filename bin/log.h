#ifndef __LOG_H__
#define __LOG_H__

#define LogWrite(format, args...) _LogWrite(__FILE__, __LINE__, format, ## args)

void _LogWrite(char *file, int line, const char *format, ...);

#endif //__LOG_H__
