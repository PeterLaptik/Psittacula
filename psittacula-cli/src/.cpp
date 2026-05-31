#include "response_reader_stream_or.h"
#include "tool_base.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

// Outputs reasoning text or progress animation
void reasoning_output(const char *text);

// Tokens counter
static int completion_tokens = 0;
static int prompt_tokens = 0;
static int total_tokens = 0;
static double tokens_cost = 0;

static bool show_reasoning = false;
static const char *kReasoningOutputColour = "\033[90m";

// Resoning progress
// Shows rotating line in a console
static size_t progress_cursor = 0;
static char progress[4] = { '/', '|', '\\', '-' };
static bool reasoning_in_process = true;

static std::string stream_text_accumulator;


// Pieces of tool calls chunks
static std::vector<std::string> tool_calls_sequense; // ids in responses order
static int tool_auto_id = 0; // autoincrenting id for lost id numbers


size_t responses_fn::open_router::stream_callback_stream_completion(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t total = size * nmemb;
    std::string chunk(ptr, total);

    // SSE sends lines like:
    // data: {"id":"...","choices":[{"delta":{"content":"Text"}}]}
    // data: [DONE]

    std::istringstream stream(chunk);
    std::string line;

    while (std::getline(stream, line)) {
        std::cout << line << std::endl;
        if (line.rfind("data:", 0) == 0)
        {
            std::string json = line.substr(5);
            if (json == " [DONE]" || json == "[DONE]")
            {
                std::cout << "\n<stream finished>\n";

                std::cout << "FINAL:" << stream_text_accumulator << std::endl;

                std::string assembled_tool_name = "";
                std::string assembled_tool_args = "";

                std::cout << "\n  Full name: " << assembled_tool_name << "\n";
                std::cout << "\n  Full args: " << assembled_tool_args << "\n";
                std::cout << "\n  Tokens: " << total_tokens << "\n";
                return total;
            }

            rapidjson::Document d;
            if (!d.Parse(json.c_str()).HasParseError())
            {
                if (d.HasMember("choices"))
                {
                    // ========== Content output ==========================
                    const auto &delta = d["choices"][0]["delta"];

                    if (delta.HasMember("content") && delta["content"].IsString())
                    {
                        // Empty content can exist, so the first non-empty value is considered as a finished reasoning, see the check below
                        size_t c_len = std::strlen(delta["content"].GetString());

                        // Reset reasoning mode output after first entering
                        if (reasoning_in_process && c_len > 0)
                        {
                            reasoning_in_process = false;
                            std::cout << std::endl << std::flush;
                        }

                        std::cout << "\033[32m" << delta["content"].GetString() << std::flush;
                        stream_text_accumulator += delta["content"].GetString();
                    }

                    // ========== Tokens counter ==========================
                    if (d.HasMember("usage"))
                    {
                        const auto &usage = d["usage"];
                        if (usage.HasMember("total_tokens") && usage["total_tokens"].IsInt())
                        {
                            total_tokens = usage["total_tokens"].GetInt();
                        }
                        if (usage.HasMember("completion_tokens") && usage["completion_tokens"].IsInt())
                        {
                            completion_tokens = usage["completion_tokens"].GetInt();
                        }
                        if (usage.HasMember("prompt_tokens") && usage["prompt_tokens"].IsInt())
                        {
                            prompt_tokens = usage["prompt_tokens"].GetInt();
                        }
                        if (usage.HasMember("cost") && usage["cost"].IsDouble())
                        {
                            tokens_cost = usage["cost"].GetInt();
                        }
                    }


                    // ========== Output reasoning data ====================
                    // Reasoning text or progress
                    // Ignored after the first non-empty contet (see the code above)
                    // Checking fields: reasoning_content or reasoning
                    else if (reasoning_in_process && delta.HasMember("reasoning") && delta["reasoning"].IsString())
                    {
                        reasoning_output(delta["reasoning"].GetString());
                        reasoning_in_process = true;
                    }
                }
            }
            else
            {
                std::cout << "No data to parse: " << json << std::endl;
            }
        }
    }

    return total;
}

void responses_fn::open_router::set_reasoning_visible(bool is_visible)
{
    show_reasoning = is_visible;
}

void responses_fn::open_router::reset_stream_receiver()
{
    stream_text_accumulator = "";
    tool_calls_sequense.clear();
    tool_auto_id = 0;
    total_tokens = 0;
    completion_tokens = 0;
    prompt_tokens = 0;
    tokens_cost = 0;
}

void responses_fn::open_router::get_stream_response(CompletionResponse &rsp)
{
    //std::cout << "\n<stream finished>\n";

    //std::cout << "FINAL:" << stream_text_accumulator << std::endl;

    std::string assembled_tool_name = "";
    std::string assembled_tool_args = "";

    rsp.message = stream_text_accumulator;
    //rsp.tool_name = assembled_tool_name;
    rsp.total_tokens = total_tokens;
    rsp.completion_tokens = completion_tokens;
    rsp.completion_tokens = total_tokens;
    rsp.prompt_tokens = prompt_tokens;
    rsp.tokens_cost = tokens_cost;
   /* std::cout << "\n  Full name: " << assembled_tool_name << "\n";*/
    std::cout << "\n  Full args: " << assembled_tool_args << "\n";
    /*std::cout << "\n  Tokens: " << total_tokens << "\n"; */
}

static void reasoning_output(const char *text)
{
    //if (show_reasoning)
    //{
    //    std::cout << kReasoningOutputColour << text << std::flush;
    //}
    //else
    //{
    //    progress_cursor = (progress_cursor >= 3 ? 0 : progress_cursor + 1);
    //    std::cout << kReasoningOutputColour << "\rReasoning " << progress[progress_cursor];
    //    if(total_tokens > 0)
    //        std::cout << "\ttokens: " << total_tokens;
    //}
}