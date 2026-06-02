#ifndef DEFAULT_RULE_PROVIDER_INCLUDED_H
#define DEFAULT_RULE_PROVIDER_INCLUDED_H

#include <string>

class DefaultRuleProvider
{
    public:
        DefaultRuleProvider() = default;

        ~DefaultRuleProvider() = default;

        std::string GetDefaultSystemPrompt() const;

    private:
        std::string GetDefaultRules() const;

};

#endif // ! DEFAULT_RULE_PROVIDER_INCLUDED_H
