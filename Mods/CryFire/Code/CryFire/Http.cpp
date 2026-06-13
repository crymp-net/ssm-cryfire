//================================================================================
// File:    Code/CryFire/Http.h
//                 ____                        ____
// Project: SSM   /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\   _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/   \/_/  \/_/    \/____ /
//                                        /\___/
//                                        \/__/
// Created on:   20.2.2018
// Last edited:  13.6.2026
//--------------------------------------------------------------------------------
// Description: Asynchronous HTTP requests library using WinHTTP (SSL forced)
//================================================================================

#include "StdAfx.h"
#include "Http.h"
#include "CryFire/AsyncTasks.h"
#include "CryFire/Logging.h"

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

using namespace std;

enum HttpReqType { GET = 0, POST };
static const wchar_t* const HttpReqTypeStr[] = { L"GET", L"POST" };

struct HttpRequestParams {
    HttpReqType type;
    std::string hostName;
    uint16_t port;
    std::string urlPath;
    HTTP::Headers headers;
    std::string data;
    HTTP::ResultCallback userCallback;
    void* userArg;
};

struct HttpResultParams {
    std::string hostName;
    uint16_t port;
    std::string urlPath;
    int netStatus;
    unsigned int httpStatus;
    HTTP::Headers respHeaders;
    std::string respData;
    HTTP::ResultCallback userCallback;
    void* userArg;
};

// Helper function to convert UTF-8 to UTF-16 wide strings for WinHTTP
static std::wstring MultiByteToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Helper function to convert UTF-16 wide strings back to UTF-8
static std::string WideToMultiByte(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

#define returnErrorWinHTTP( msgFormat, ... ) {\
    resParams->netStatus = GetLastError();\
    CF_AsyncLog( 7, "$4HTTP WinHTTP: " msgFormat " (%d)", __VA_ARGS__, resParams->netStatus );\
    if (hRequest) WinHttpCloseHandle(hRequest);\
    if (hConnect) WinHttpCloseHandle(hConnect);\
    if (hSession) WinHttpCloseHandle(hSession);\
    delete reqParams;\
    return resParams;\
}

void* HTTP::SendRequest(void* arg)
{
    HttpRequestParams* reqParams = (HttpRequestParams*)arg;
    HttpResultParams* resParams = new HttpResultParams;

    resParams->hostName = reqParams->hostName;
    resParams->port = reqParams->port;
    resParams->urlPath = reqParams->urlPath;
    resParams->userCallback = reqParams->userCallback;
    resParams->userArg = reqParams->userArg;
    resParams->netStatus = 0;
    resParams->httpStatus = 0;

    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    // Force SSL/TLS port adjustments
    INTERNET_PORT targetPort = INTERNET_DEFAULT_HTTPS_PORT;
    if (reqParams->port != 80 && reqParams->port != 443) {
        targetPort = reqParams->port; // Keep custom port if it isn't an explicit override of standard web ports
    }

    std::wstring wHostName = MultiByteToWide(reqParams->hostName);
    std::wstring wUrlPath = MultiByteToWide(reqParams->urlPath);

    // 1. Initialize WinHTTP Session
    hSession = WinHttpOpen(L"SSM CryFire HTTP client",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        returnErrorWinHTTP("Failed to open WinHTTP session");

    // 2. Establish Connection Context
    hConnect = WinHttpConnect(hSession, wHostName.c_str(), targetPort, 0);
    if (!hConnect)
        returnErrorWinHTTP("Failed to connect to host %s", reqParams->hostName.c_str());

    // 3. Open Request (WINHTTP_FLAG_SECURE forces SSL/TLS processing)
    hRequest = WinHttpOpenRequest(hConnect,
        HttpReqTypeStr[reqParams->type],
        wUrlPath.c_str(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest)
        returnErrorWinHTTP("Failed to open HTTP request handle");

    // 4. Construct and Add Headers
    std::wstring extraHeaders;
    for (Headers::const_iterator iter = reqParams->headers.begin(); iter != reqParams->headers.end(); ++iter) {
        if (iter->first == "Host" || iter->first == "Connection" || iter->first == "User-Agent")
            continue; // Let WinHTTP handle these infrastructure tokens automatically

        extraHeaders += MultiByteToWide(iter->first) + L": " + MultiByteToWide(iter->second) + L"\r\n";
    }

    if (!extraHeaders.empty()) {
        WinHttpAddRequestHeaders(hRequest, extraHeaders.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    // 5. Transmit Request
    LPVOID requestData = reqParams->data.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)reqParams->data.c_str();
    DWORD requestDataLength = (DWORD)reqParams->data.length();

    CF_AsyncLog(7, "HTTP: sending SSL secured %s request to %s", HttpReqTypeStr[reqParams->type], reqParams->urlPath.c_str());

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, requestData, requestDataLength, requestDataLength, 0))
        returnErrorWinHTTP("Failed to send WinHTTP request");

    // 6. Wait and Receive Response
    if (!WinHttpReceiveResponse(hRequest, NULL))
        returnErrorWinHTTP("Failed to receive WinHTTP response");

    // 7. Extract Status Code
    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX)) {
        resParams->httpStatus = dwStatusCode;
    }

    // 8. Extract Headers
    dwSize = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &dwSize, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<wchar_t> headerBuffer(dwSize / sizeof(wchar_t));
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, &headerBuffer[0], &dwSize, WINHTTP_NO_HEADER_INDEX)) {
            std::wstring headersWStr(&headerBuffer[0]);
            std::wstringstream wss(headersWStr);
            std::wstring line;
            // Skip the first status description line
            std::getline(wss, line);
            while (std::getline(wss, line) && line != L"\r") {
                size_t colon = line.find(L':');
                if (colon != std::wstring::npos) {
                    std::wstring key = line.substr(0, colon);
                    std::wstring val = line.substr(colon + 1);
                    // Trim whitespaces
                    val.erase(0, val.find_first_not_of(L" \t"));
                    if (!val.empty() && val.back() == L'\r') val.pop_back();
                    resParams->respHeaders[WideToMultiByte(key)] = WideToMultiByte(val);
                }
            }
        }
    }

    // 9. Read Payload Body
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            returnErrorWinHTTP("Error mapping available body size");
        }
        if (dwSize == 0) break;

        std::vector<char> destBuf(dwSize);
        if (!WinHttpReadData(hRequest, (LPVOID)&destBuf[0], dwSize, &dwDownloaded)) {
            returnErrorWinHTTP("Error reading payload from buffer stream");
        }
        resParams->respData.append(&destBuf[0], dwDownloaded);
    } while (dwSize > 0);

    // Clean up WinHTTP instances safely
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    delete reqParams;
    return resParams;
}

void* HTTP::HandleResult(void* arg)
{
    HttpResultParams* resParams = (HttpResultParams*)arg;
    CF_Log(7, "HTTP: result is ready for request to %s:%hu%s", resParams->hostName.c_str(), resParams->port, resParams->urlPath.c_str());
    resParams->userCallback(resParams->netStatus, resParams->httpStatus, resParams->respHeaders, resParams->respData, resParams->userArg);
    delete resParams;
    return NULL;
}

//----------------------------------------------------------------------------------------------------
void HTTP::Get(const char* hostName, uint16_t port, const char* urlPath, const Headers& headers, ResultCallback callback, void* callbackArg)
{
    CF_Log(5, "HTTP: scheduling asynchronous secure GET request to %s:%hu%s", hostName, port, urlPath);

    HttpRequestParams* reqParams = new HttpRequestParams;
    reqParams->type = GET;
    reqParams->hostName = hostName;
    reqParams->port = port;
    reqParams->urlPath = urlPath;
    reqParams->headers = headers;
    reqParams->userCallback = callback;
    reqParams->userArg = callbackArg;

    AsyncTasks::addTask(SendRequest, reqParams, HandleResult);
}

//----------------------------------------------------------------------------------------------------
void HTTP::Post(const char* hostName, uint16_t port, const char* urlPath, const Headers& headers, const std::string& data, ResultCallback callback, void* callbackArg)
{
    CF_Log(5, "HTTP: scheduling asynchronous secure POST request to %s:%hu%s", hostName, port, urlPath);

    HttpRequestParams* reqParams = new HttpRequestParams;
    reqParams->type = POST;
    reqParams->hostName = hostName;
    reqParams->port = port;
    reqParams->urlPath = urlPath;
    reqParams->headers = headers;
    reqParams->data = data;
    reqParams->userCallback = callback;
    reqParams->userArg = callbackArg;

    AsyncTasks::addTask(SendRequest, reqParams, HandleResult);
}