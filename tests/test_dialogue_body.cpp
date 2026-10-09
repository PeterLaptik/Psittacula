#include "test_framework.h"
#include "dialogue_body.h"
#include <rapidjson/document.h>
#include <string>
#include <vector>

namespace {

    rapidjson::Document Parse(const std::string &json)
    {
        rapidjson::Document doc;
        doc.Parse(json.c_str());
        return doc;
    }

    bool HasRole(const rapidjson::Document &doc, const char *role)
    {
        if (doc.HasParseError() || !doc.HasMember("messages"))
            return false;

        for (const auto &m : doc["messages"].GetArray())
        {
            if (m.IsObject() && m.HasMember("role") && m["role"].IsString() &&
                std::string(m["role"].GetString()) == role)
                return true;
        }
        return false;
    }

} // namespace

PS_TEST(body_starts_with_system_and_empty_arrays)
{
    DialogueBody body;

    // A default system prompt is added in the AiClientImpl ctor, not here:
    // a fresh body has zero messages but the arrays exist
    const std::string json = body.ToJsonString();
    const rapidjson::Document doc = Parse(json);

    PS_CHECK(!doc.HasParseError());
    PS_CHECK(doc.HasMember("messages") && doc["messages"].IsArray());
    PS_CHECK(doc["messages"].Empty());
    PS_CHECK(doc.HasMember("tools") && doc["tools"].IsArray());
    PS_CHECK(doc.HasMember("model") && doc["model"].IsString());
    PS_CHECK(std::string(doc["model"].GetString()) == "any");
}

PS_TEST(add_user_and_response_roundtrip)
{
    DialogueBody body;
    body.AddUserMessage("hello");
    PS_CHECK(body.AddResponse("hi there"));

    const rapidjson::Document doc = Parse(body.ToJsonString());
    PS_CHECK(!doc.HasParseError());

    const auto &msgs = doc["messages"];
    PS_CHECK(msgs.Size() == 2);
    PS_CHECK(std::string(msgs[0]["role"].GetString()) == "user");
    PS_CHECK(std::string(msgs[0]["content"].GetString()) == "hello");
    PS_CHECK(std::string(msgs[1]["role"].GetString()) == "assistant");
    PS_CHECK(std::string(msgs[1]["content"].GetString()) == "hi there");
}

PS_TEST(add_response_rejects_whitespace_only)
{
    DialogueBody body;
    body.AddUserMessage("q");

    PS_CHECK(!body.AddResponse(""));
    PS_CHECK(!body.AddResponse("   \n\t  "));

    const rapidjson::Document doc = Parse(body.ToJsonString());
    PS_CHECK(doc["messages"].Size() == 1); // no empty assistant bubble
}

PS_TEST(response_with_embedded_nul_survives)
{
    DialogueBody body;
    body.AddUserMessage("q");

    const std::string with_nul = std::string("abc\0def", 7);
    PS_CHECK(body.AddResponse(with_nul));

    // Round-trip through the serialized body: NUL is escaped as \u0000,
    // the tail after it MUST survive (the old c_str() overload truncated)
    const std::string json = body.ToJsonString();
    PS_CHECK(json.find("\\u0000") != std::string::npos);
    PS_CHECK(json.find("def") != std::string::npos);

    const rapidjson::Document doc = Parse(json);
    PS_CHECK(!doc.HasParseError());
    // Measure with the length-aware accessor: a plain `const char* ->
    // std::string` construction would stop at the decoded NUL - the very
    // truncation this test guards against, now in the test itself
    const rapidjson::Value &content_val = doc["messages"][1]["content"];
    const std::string stored(content_val.GetString(), content_val.GetStringLength());
    PS_CHECK_MSG(stored.size() == 7, "embedded NUL truncation: content lost its tail");
    PS_CHECK(stored[3] == '\0');
    PS_CHECK(stored.substr(4) == "def");
    PS_CHECK(std::string(stored.c_str()) == "abc"); // c_str view stops at NUL, as expected
}

PS_TEST(system_message_replaces_not_duplicates)
{
    DialogueBody body;
    body.AddSystemMessage("rules v1");
    body.AddSystemMessage("rules v2");

    const rapidjson::Document doc = Parse(body.ToJsonString());
    const auto &msgs = doc["messages"];

    PS_CHECK(msgs.Size() == 1); // replaced, not stacked
    PS_CHECK(std::string(msgs[0]["role"].GetString()) == "system");
    PS_CHECK(std::string(msgs[0]["content"].GetString()) == "rules v2");
}

PS_TEST(remove_last_exchange_keeps_requested_exchange)
{
    DialogueBody body;
    body.AddSystemMessage("sys");
    body.AddUserMessage("q1");
    body.AddResponse("a1");
    body.AddUserMessage("q2"); // the exchange to be rolled back
    body.AddResponse("partial");

    body.RemoveLastExchange();

    const rapidjson::Document doc = Parse(body.ToJsonString());
    const auto &msgs = doc["messages"];

    PS_CHECK(msgs.Size() == 3); // sys + q1 + a1
    PS_CHECK(std::string(msgs[2]["content"].GetString()) == "a1");
    PS_CHECK(!HasRole(doc, "x-unused")); // silence unused helper warnings on some compilers
}

PS_TEST(tool_responses_shape)
{
    DialogueBody body;
    body.AddUserMessage("list files");

    std::vector<ToolResponse> responses;
    ToolResponse rsp;
    rsp.id = "call_1";
    rsp.name = "list_files";
    rsp.input_content = "{\"path\":\"C:/x\"}";
    rsp.output_content = "{\"entries\":[]}";
    responses.push_back(rsp);

    body.AddToolResponses(responses);

    const rapidjson::Document doc = Parse(body.ToJsonString());
    const auto &msgs = doc["messages"];

    PS_CHECK(msgs.Size() == 3);
    // assistant tool_calls carrier message
    PS_CHECK(std::string(msgs[1]["role"].GetString()) == "assistant");
    PS_CHECK(msgs[1].HasMember("tool_calls"));
    // tool result message
    PS_CHECK(std::string(msgs[2]["role"].GetString()) == "tool");
    PS_CHECK(std::string(msgs[2]["tool_call_id"].GetString()) == "call_1");
    PS_CHECK(std::string(msgs[2]["content"].GetString()) == "{\"entries\":[]}");
}
