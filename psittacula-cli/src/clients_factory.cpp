#include "clients_factory.h"
#include "ai_client_impl.h"
#include "model.h"
#include <memory>

std::unique_ptr<AiClient> create_base_client(const Model &model)
{
    return std::make_unique<AiClientImpl>(model);
}
