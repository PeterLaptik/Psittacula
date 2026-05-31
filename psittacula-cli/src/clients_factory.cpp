#include "clients_factory.h"
#include "ai_client_impl.h"

AiClient* create_base_client(const std::string &host)
{
    return new AiClientImpl(host);
}
