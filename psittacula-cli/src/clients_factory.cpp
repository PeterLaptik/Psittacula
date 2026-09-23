#include "clients_factory.h"
#include "ai_client_impl.h"
#include <memory>

std::unique_ptr<AiClient> create_base_client(const std::string &host, int context_size)
{
    return std::make_unique<AiClientImpl>(host, context_size);
}
