#include "http/HttpResponse.h"


#include <sstream>


std::string HttpResponse::ok(
    const std::string& body
)
{

    return build(
        200,
        "OK",
        body
    );

}



std::string HttpResponse::notFound()
{

    return build(
        404,
        "Not Found",
        "{\"error\":\"not found\"}"
    );

}



std::string HttpResponse::badRequest()
{

    return build(
        400,
        "Bad Request",
        "{\"error\":\"bad request\"}"
    );

}



std::string HttpResponse::build(
    int statusCode,
    const std::string& statusText,
    const std::string& body
)
{

    std::ostringstream response;


    response
        << "HTTP/1.1 "
        << statusCode
        << " "
        << statusText
        << "\r\n";


    response
        << "Content-Type: application/json\r\n";


    response
        << "Content-Length: "
        << body.size()
        << "\r\n";


    response
        << "Connection: close\r\n";


    response
        << "\r\n";


    response
        << body;


    return response.str();
}
