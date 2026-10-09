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

    // A read_file tool result as the real tool emits it (parsed later by
    // PurgePreviousFileContents)
    ToolResponse MakeRead(const std::string &id, const std::string &path, const std::string &bytes)
    {
        ToolResponse r;
        r.id = id;
        r.name = "read_file";
        r.input_content = std::string("{\"path\":\"") + path + "\"}";
        r.output_content = std::string("{\"file\":{\"path\":\"") + path + "\",\"content\":\"" + bytes + "\"}}";
        return r;
    }

    std::size_t MessageCount(const rapidjson::Document &doc)
    {
        return doc.HasMember("messages") && doc["messages"].IsArray()
            ? doc["messages"].Size() : 0;
    }

} // namespace

PS_TEST(purge_replaces_only_stale_reread_file)
{
    DialogueBody body;
    body.AddUserMessage("read a.txt and b.txt, then re-read a.txt");

    // Round 1: read both files
    std::vector<ToolResponse> round1;
    round1.push_back(MakeRead("c1", "C:/p/a.txt", "OLD-A"));
    round1.push_back(MakeRead("c2", "C:/p/b.txt", "OLD-B"));
    body.AddToolResponses(round1);

    // Round 2: re-read a.txt only -> earlier OLD-A payload is obsolete
    std::vector<ToolResponse> round2;
    round2.push_back(MakeRead("c3", "C:/p/a.txt", "NEW-A"));
    body.AddToolResponses(round2);

    const std::string json = body.ToJsonString();

    PS_CHECK(json.find("NEW-A") != std::string::npos);         // the fresh read
    PS_CHECK(json.find("OLD-B") != std::string::npos);         // b.txt untouched
    PS_CHECK(json.find("File was successfully read") != std::string::npos); // the note
    PS_CHECK(json.find("OLD-A") == std::string::npos);         // stale content is gone
}

PS_TEST(purge_keeps_binary_reads)
{
    DialogueBody body;
    body.AddUserMessage("q");

    // A binary read carries 'content_base64' instead of 'content':
    // there is nothing to purge, and the message must survive as-is
    std::vector<ToolResponse> round1;
    ToolResponse bin;
    bin.id = "c1";
    bin.name = "read_file";
    bin.input_content = "{\"path\":\"C:/p/img.png\",\"binary\":true}";
    bin.output_content = "{\"file\":{\"path\":\"C:/p/img.png\",\"content_base64\":\"QUJD\"}}";
    round1.push_back(bin);

    body.AddToolResponses(round1);

    std::vector<ToolResponse> round2;
    round2.push_back(MakeRead("c2", "C:/p/img.png", "texty"));
    body.AddToolResponses(round2);

    const std::string json = body.ToJsonString();
    PS_CHECK(json.find("QUJD") != std::string::npos); // base64 payload survived
}

PS_TEST(compress_keeps_system_summary_and_last_messages)
{
    DialogueBody body;
    body.AddSystemMessage("sys-rules");
    body.AddUserMessage("q1");
    body.AddResponse("a1");
    body.AddUserMessage("q2");
    body.AddResponse("a2");
    body.AddUserMessage("q3");  // last two messages: q3 stays verbatim
    body.AddResponse("a3");

    body.Compress("SUMMARY-TXT", 2);

    const rapidjson::Document doc = Parse(body.ToJsonString());
    const auto &msgs = doc["messages"];

    PS_CHECK(MessageCount(doc) == 4); // sys + summary + q3 + a3
    PS_CHECK(std::string(msgs[0]["role"].GetString()) == "system");
    PS_CHECK(std::string(msgs[0]["content"].GetString()) == "sys-rules");
    PS_CHECK(std::string(msgs[1]["role"].GetString()) == "assistant");
    PS_CHECK(std::string(msgs[1]["content"].GetString()) == "SUMMARY-TXT");
    PS_CHECK(std::string(msgs[2]["content"].GetString()) == "q3");
    PS_CHECK(std::string(msgs[3]["content"].GetString()) == "a3");
}

PS_TEST(compress_drops_orphaned_tool_result_in_kept_window)
{
    DialogueBody body;
    body.AddSystemMessage("sys");
    body.AddUserMessage("q");
    body.AddResponse("a");

    // A tool exchange as the kept window would start with: the tool result
    // without its tool_calls parent is rejected by strict servers, so
    // Compress must skip such leading tool results
    std::vector<ToolResponse> responses;
    ToolResponse rsp;
    rsp.id = "call_1";
    rsp.name = "list_files";
    rsp.input_content = "{}";
    rsp.output_content = "[]";
    responses.push_back(rsp);
    body.AddToolResponses(responses);   // adds tool_calls carrier + tool result
    body.AddResponse("final answer");   // the newest message

    PS_CHECK(MessageCount(Parse(body.ToJsonString())) == 6); // sys + q + a + carrier + tool + final

    // msg_left = 2 cuts BETWEEN carrier and tool: the window would start
    // with a bare tool result -> the guard must drop it
    body.Compress("SUMMARY", 2);

    const rapidjson::Document doc = Parse(body.ToJsonString());
    const auto &msgs = doc["messages"];

    PS_CHECK(MessageCount(doc) == 3); // sys + summary + final
    PS_CHECK(std::string(msgs[0]["role"].GetString()) == "system");
    PS_CHECK(std::string(msgs[1]["content"].GetString()) == "SUMMARY");
    PS_CHECK(std::string(msgs[2]["role"].GetString()) == "assistant");
    PS_CHECK(std::string(msgs[2]["content"].GetString()) == "final answer");
}

PS_TEST(compress_keeps_full_tool_exchange_window)
{
    DialogueBody body;
    body.AddSystemMessage("sys");
    body.AddUserMessage("q");
    body.AddResponse("a");

    std::vector<ToolResponse> responses;
    ToolResponse rsp;
    rsp.id = "call_1";
    rsp.name = "list_files";
    rsp.input_content = "{}";
    rsp.output_content = "[]";
    responses.push_back(rsp);
    body.AddToolResponses(responses);   // carrier + tool result
    body.AddResponse("final answer");

    // msg_left = 3 keeps [carrier, tool, final] as-is: the window starts
    // with the carrier (assistant), nothing may be skipped
    body.Compress("SUMMARY", 3);

    const rapidjson::Document doc = Parse(body.ToJsonString());
    const auto &msgs = doc["messages"];

    PS_CHECK(MessageCount(doc) == 5); // sys + summary + carrier + tool + final
    PS_CHECK(std::string(msgs[2]["role"].GetString()) == "assistant");
    PS_CHECK(msgs[2].HasMember("tool_calls"));  // the carrier
    PS_CHECK(std::string(msgs[3]["role"].GetString()) == "tool");
    PS_CHECK(std::string(msgs[4]["content"].GetString()) == "final answer");
}

PS_TEST(summarize_request_carries_model_and_trims_tail)
{
    DialogueBody body;
    body.SetModel("test-model");
    body.AddSystemMessage("sys");
    body.AddUserMessage("q1");
    body.AddResponse("a1");
    body.AddUserMessage("q2");
    body.AddResponse("a2"); // last 2 stay out of the summarize body

    const std::string summary_body = body.GetBodyForSummarizing(2);
    const rapidjson::Document doc = Parse(summary_body);

    PS_CHECK(!doc.HasParseError());
    PS_CHECK(doc.HasMember("model") && doc["model"].IsString());
    PS_CHECK(std::string(doc["model"].GetString()) == "test-model"); // (was the "any" bug)
    PS_CHECK(MessageCount(doc) == 4); // sys + q1 + a1 + the instruction message itself

    // The summarize instruction is the latest (appended) user message
    const auto &msgs = doc["messages"];
    PS_CHECK(std::string(msgs[0]["role"].GetString()) == "system");
    PS_CHECK(std::string(msgs[1]["content"].GetString()) == "q1");
    PS_CHECK(std::string(msgs[2]["role"].GetString()) == "assistant");
    PS_CHECK(std::string(msgs[3]["role"].GetString()) == "user");
    PS_CHECK(std::string(msgs[3]["content"].GetString()).find("summarize") != std::string::npos);
}

PS_TEST(summarize_request_on_short_dialogue_returns_full_body)
{
    DialogueBody body;
    body.AddUserMessage("only one");

    // Not enough context: the body is reused as-is rather than built
    const std::string summary_body = body.GetBodyForSummarizing(2);
    const rapidjson::Document doc = Parse(summary_body);
    PS_CHECK(!doc.HasParseError());
    PS_CHECK(MessageCount(doc) == 1);
}
