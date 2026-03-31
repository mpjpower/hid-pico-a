#ifndef COMMAND_INTERFACE_H
#define COMMAND_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

void command_interface_process(const uint8_t *buffer, uint16_t bufsize, char *response, size_t response_size);

#endif // COMMAND_INTERFACE_H
