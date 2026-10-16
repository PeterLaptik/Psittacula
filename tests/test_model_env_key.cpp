// Unit tests: api_key_env= support in Model::FromFile.
// The API key is never read from the model file; api_key_env=<VAR> is the
// only key source. A missing environment variable is not an error (the key
// stays empty and the connectivity probe later reports it as 401/403,
// "API key rejected").
#include "test_framework.h"
#include "model.h"
#include "clients_factory.h"
#include "ai_client.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

// The env-key tests never call Model::GetClient(), but the linker still
// needs the symbol because model.cpp defines it out-of-line. Provide a
// stub instead of dragging AiClientImpl/libcurl into the test binary.
std::unique_ptr<AiClient> create_base_client(const Model &)
{
    return nullptr;
}

namespace
{
    // Cross-platform _putenv_s / setenv wrapper
    void put_env(const char *name, const char *value)
    {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }

    // Writes a throwaway model file and reads it back through Model::FromFile
    Model model_from_text(const std::string &text)
    {
        const std::string path = "test_model.txt";
        std::ofstream f(path, std::ios::trunc | std::ios::binary);
        f << text;
        f.close();

        Model m = Model::FromFile(path);
        std::remove(path.c_str());
        return m;
    }
}

// api_key absent + api_key_env set -> key comes from the environment
PS_TEST(model_env_key_fallback)
{
    put_env("PS_ENVKEY_A", "sk-test-123");

    const Model m = model_from_text(
        "name=m1\n"
        "host=http://localhost:8080\n"
        "api_key_env=PS_ENVKEY_A\n");

    PS_CHECK(m.GetApiKey() == "sk-test-123");
    PS_CHECK(m.GetName() == "m1");
    PS_CHECK(m.GetHost() == "http://localhost:8080");
}

// Variable is not set -> key stays empty, no exception
PS_TEST(model_env_key_missing_var_falls_back_to_empty)
{
    put_env("PS_ENVKEY_ABSENT", "");

    const Model m = model_from_text(
        "name=m3\n"
        "host=http://localhost:8080\n"
        "api_key_env=PS_ENVKEY_ABSENT\n");

    PS_CHECK(m.GetApiKey().empty());
    PS_CHECK(m.GetName() == "m3");
}

// No key lines at all -> keyless (llama.cpp) server file
PS_TEST(model_env_key_keyless_file)
{
    const Model m = model_from_text(
        "name=m4\n"
        "host=http://localhost:8080\n");

    PS_CHECK(m.GetApiKey().empty());
    PS_CHECK(m.GetChatEndpoint() == "/v1/chat/completions");
}

// Variable is set to empty string -> treated as "no key"
PS_TEST(model_env_key_env_set_to_empty)
{
    put_env("PS_ENVKEY_EMPTY", "");

    const Model m = model_from_text(
        "name=m5\n"
        "host=http://localhost:8080\n"
        "api_key_env=PS_ENVKEY_EMPTY\n");

    PS_CHECK(m.GetApiKey().empty());
    PS_CHECK(m.GetName() == "m5");
}
