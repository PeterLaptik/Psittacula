#ifndef MESSAGE_INCLUDED_H
#define MESSAGE_INCLUDED_H

// TODO REMOVE

#include<string>

class Message
{
    public:
        enum class Role {
            user,
            assistant
        };

        Message(Message::Role role, const std::string &msg);

        ~Message() = default;

        std::string ToJsonString() const;

        std::string ToPlainString() const;

    private:
        std::string m_role = "user";
        std::string m_message;
};

#endif // MESSAGE_INCLUDED_H
