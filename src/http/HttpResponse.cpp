#include "http/HttpResponse.h"

#include <sstream>


// ============================================================
// 200 JSON
// ============================================================

std::string HttpResponse::okJson(
    const std::string& body
) {
    return build(
        200,
        "OK",
        "application/json; charset=utf-8",
        body
    );
}


// ============================================================
// 200 HTML
// ============================================================

std::string HttpResponse::okHtml(
    const std::string& body
) {
    return build(
        200,
        "OK",
        "text/html; charset=utf-8",
        body
    );
}


// ============================================================
// 404
// ============================================================

std::string HttpResponse::notFound() {
    return build(
        404,
        "Not Found",
        "application/json; charset=utf-8",
        "{\"error\":\"not found\"}"
    );
}


// ============================================================
// 400
// ============================================================

std::string HttpResponse::badRequest() {
    return build(
        400,
        "Bad Request",
        "application/json; charset=utf-8",
        "{\"error\":\"bad request\"}"
    );
}


// ============================================================
// 500
// ============================================================

std::string HttpResponse::internalServerError() {
    return build(
        500,
        "Internal Server Error",
        "application/json; charset=utf-8",
        "{\"error\":\"internal server error\"}"
    );
}


// ============================================================
// Build complete HTTP response
// ============================================================

std::string HttpResponse::build(
    int statusCode,
    const std::string& statusText,
    const std::string& contentType,
    const std::string& body
) {
    std::ostringstream response;


    response
        << "HTTP/1.1 "
        << statusCode
        << ' '
        << statusText
        << "\r\n";


    response
        << "Content-Type: "
        << contentType
        << "\r\n";


    response
        << "Content-Length: "
        << body.size()
        << "\r\n";


    response
        << "Connection: close\r\n";


    response
        << "Cache-Control: no-cache\r\n";


    response
        << "\r\n";


    response
        << body;


    return response.str();
}
