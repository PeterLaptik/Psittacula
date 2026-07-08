#ifndef LOGO_INCLUDED_H
#define LOGO_INCLUDED_H

#include <string>

#ifndef PSITTACULA_APP_VERSION
#define PSITTACULA_APP_VERSION "unknown"
#endif

std::string get_logo()
{
    std::string logo =
        R"(      ____________________________________________
      / __\ ___\//_ _/_ _// | /__\//  ///   / |
     / /_// /  // //  // //||//  //  ///   //||
    / ___/__ \// //  // //_||/  //  ///   //_||
   / /  ____\// //  // /___ |\_//__///__ /___ |
 __\/__/____//_//__//_//___||_/\___/____\/___||_
    )";

    logo += "Version: ";
    logo += PSITTACULA_APP_VERSION;
    logo += "\n    Written by Peter Laptik";
    logo += "\n\033[36m    Input /help or /h for information about commands\033[0m";
    return logo;
}

#endif // LOGO_INCLUDED_H
