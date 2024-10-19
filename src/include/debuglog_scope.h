#pragma once

#include <functional>

/**
  Purpose: Helper to guard value
*/
class scope_exit
{
public:
    // Constructor that takes a handler
    scope_exit(std::function<void()> handler)
        : handler_(handler)
    {}

    // Destructor that calls the handler
    ~scope_exit()
    {
        if (handler_)
            handler_();
    }

private:
    std::function<void()> handler_;
};
