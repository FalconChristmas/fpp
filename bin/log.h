#ifndef __LOG_H__
#define __LOG_H__

#define LogWrite(format, args...) _LogWrite("%s:%d:" format, __FILE__, __LINE__, ## args)

void _LogWrite(const char *format, char *file, int line, ...);

#endif //__LOG_H__
