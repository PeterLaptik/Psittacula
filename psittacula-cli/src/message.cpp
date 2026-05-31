#include "message.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

Message::Message(Message::Role role, const std::string &msg)
    : m_message(msg)
{
    switch (role)
    {
        case Message::Role::assistant:
            m_role = "assistant";
    }
}

std::string Message::ToPlainString() const
{
    return m_role + ": " + m_message;
}

std::string Message::ToJsonString() const
{
    rapidjson::Document doc;
    doc.SetObject();

    auto &alloc = doc.GetAllocator();

    doc.AddMember("role",
        rapidjson::Value(m_role.c_str(), alloc),
        alloc);

    doc.AddMember("message",
        rapidjson::Value(m_message.c_str(), alloc),
        alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}
