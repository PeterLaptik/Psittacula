#ifndef RESPONSE_INCLUDED_H
#define RESPONSE_INCLUDED_H

#include <string>

namespace tui {
    /// Screen response: result of processing one key (char)
    class Response
    {
        public:
            enum class Result
            {
                ok,         // do nothing
                enter,      // send message to LLM
                command,    // command to execute
                escape      // exit
            };

            Result result;
            std::string data;

            Response(Response::Result res = Response::Result::ok)
                : result(res)
            { }

            Response(Response::Result res, const std::string &txt)
                : result(res), data(txt)
            { }
    };

    using ResponseResult = Response::Result;
}

#endif // RESPONSE_INCLUDED_H
