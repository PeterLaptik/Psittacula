#ifndef CLIENTS_FACTORY_INCLUDED_H
#define CLIENTS_FACTORY_INCLUDED_H

#include "ai_client.h"

class Model;

// Creates standard base AI API client
std::unique_ptr<AiClient> create_base_client(const Model &model);

#endif // CLIENTS_FACTORY_INCLUDED_H
