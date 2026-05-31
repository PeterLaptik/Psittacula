#include "health_rsp.h"
#include <rapidjson/document.h>
#include <string>
#include <variant>
#include <stdexcept>

ServerHealthResponse health_response_from_json(const std::string &str)
{
    rapidjson::Document doc;
    doc.Parse(str.c_str());

    if (doc.HasParseError()) 
    {
        throw std::runtime_error("Could not parse response: Invalid JSON");
    }

    // Case 1: {"status": "ok"}
    if (doc.HasMember("status") && doc["status"].IsString()) 
    {
        return HealthOkResponse{ doc["status"].GetString() };
    }

    ErrorInfo err_info;
    // Case 2: {"error": {...}}
    if (doc.HasMember("error") && doc["error"].IsObject()) 
    {
        const auto &err = doc["error"];

        ErrorInfo err_info{
            err["code"].GetInt(),
            err["message"].GetString(),
            err["type"].GetString()
        };
    }

    return HealthErrorResponse{ err_info };
}
