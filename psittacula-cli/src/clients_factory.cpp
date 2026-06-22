#include "clients_factory.h"
#include "ai_client_impl.h"

AiClient* create_base_client(const std::string &host, int context_size)
{
    return new AiClientImpl(host, context_size);
}
