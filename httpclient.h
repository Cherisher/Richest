#pragma once

#include <curl/curl.h>
#include <jsoncpp/json/json.h>
#include <map>
#include <vector>
namespace HD
{
class HttpClient
{
public:
    HttpClient();
    ~HttpClient();

    bool doHttpGetRequest(const std::string &url, Json::Value &root,
                          const std::vector<std::string> &header_array = std::vector<std::string>());
    bool doHttpPostRequest(const std::string &url, const std::string &params, Json::Value &root,
                           const std::vector<std::string> &header_array = std::vector<std::string>());
    bool doHttpDeleteRequest(const std::string &url, const std::string &params, Json::Value &root,
                             const std::vector<std::string> &header_array = std::vector<std::string>());

    void SetUrl(const std::string &url);
    void SetProxy(const std::string &proxy);
    void SetVerbose(const long &verbose);
    void SetTimeout(const long &timeout);

    void AddHeader(const std::string &attr, const std::string &val);
    void RemoveHeader(const std::string &attr);

private:
    void SetCurlOpt(const std::string &url, const std::vector<std::string> &header_array, struct curl_slist **headers);
    bool parse_response(CURLcode res, const std::string &response, Json::Value &root);

private:
    std::map<std::string, std::string> m_headers;
    std::string m_url;

    /**
     * @brief timeout for http request in milliseconds
     */
    long m_timeout;
    long m_verbose;
    CURL *m_curl;
};
}
