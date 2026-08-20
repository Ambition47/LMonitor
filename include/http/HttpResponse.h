#ifndef LMONITOR_HTTP_RESPONSE_H
#define LMONITOR_HTTP_RESPONSE_H


#include <string>


class HttpResponse {

public:

    static std::string ok(
        const std::string& body
    );


    static std::string notFound();


    static std::string badRequest();


private:

    static std::string build(
        int statusCode,
        const std::string& statusText,
        const std::string& body
    );

};


#endif
