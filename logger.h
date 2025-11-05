#ifndef LOGGER_H
#define LOGGER_H

void log_init();
void log_message(const char *format, ...);
void log_close();

#endif // LOGGER_H