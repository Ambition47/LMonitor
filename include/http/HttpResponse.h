#ifndef LMONITOR_HTTP_RESPONSE_H
#define LMONITOR_HTTP_RESPONSE_H

#include <string>


class HttpResponse {
public:
    static std::string okJson(
        const std::string& body
    );


    static std::string okHtml(
        const std::string& body
    );


    static std::string notFound();


    static std::string badRequest();


    static std::string internalServerError();


private:
    static std::string build(
        int statusCode,
        const std::string& statusText,
        const std::string& contentType,
        const std::string& body
    );
};

#endif
