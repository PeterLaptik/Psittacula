#ifndef RSP_HEALTH_INCLUDED_H
#define RSP_HEALTH_INCLUDED_H

#include <string>
#include <variant>

struct HealthOkResponse 
{
    std::string status;
};

struct ErrorInfo 
{
    int code = 0;
    std::string message;
    std::string type;
};

struct HealthErrorResponse 
{
    ErrorInfo error;
};

using ServerHealthResponse = std::variant<HealthOkResponse, HealthErrorResponse>;

ServerHealthResponse health_response_from_json(const std::string &str);

#endif // RSP_HEALTH_INCLUDED_H