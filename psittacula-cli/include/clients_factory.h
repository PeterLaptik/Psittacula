#ifndef CLIENTS_FACTORY_INCLUDED_H
#define CLIENTS_FACTORY_INCLUDED_H

#include "ai_client.h"

AiClient *create_base_client(const std::string &host, int context_size);

#endif // CLIENTS_FACTORY_INCLUDED_H
