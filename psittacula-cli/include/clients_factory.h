#ifndef CLIENTS_FACTORY_INCLUDED_H
#define CLIENTS_FACTORY_INCLUDED_H

#include "ai_client.h"

// Creates standard AI API client
AiClient *create_base_client(const std::string &host, int context_size);

#endif // CLIENTS_FACTORY_INCLUDED_H
