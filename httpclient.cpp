#include <cstdlib>
#include <cstring>
#include <iostream>
#include <openssl/crypto.h>
#include <sstream>

#include "httpclient.h"
/*
Yes but with some limitations; for example,
an SSL connection cannot be used concurrently by multiple threads.
This is true for most OpenSSL objects.

For version 1.1.0 and later, there is nothing further you need do.

For earlier versions than 1.1.0, it is necessary for your application
to set up the thread callback functions.
To do this, your application must call CRYPTO_set_locking_callback(3)
and one of the CRYPTO_THREADID_set... API's.
See the OpenSSL threads manpage
for details and "note on multi-threading" in the INSTALL file in the source distribution.
*/

using namespace std;
static pthread_mutex_t *lockarray;
class curl_initializer
{
public:
    curl_initializer()
    {
        curl_global_init(CURL_GLOBAL_ALL);
        init_locks();
    }
    ~curl_initializer()
    {
        kill_locks();
        curl_global_cleanup();
    }

private:
    static void locking_function(int mode, int type, const char *file, int line);
    static unsigned long thread_id(void);

    static void init_locks(void);
    static void kill_locks(void);

    static pthread_mutex_t *lockarray;
};

pthread_mutex_t *curl_initializer::lockarray = nullptr;

void curl_initializer::locking_function(int mode, int type, const char *file, int line)
{
    (void)file;
    (void)line;
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(&(lockarray[type]));
    } else {
        pthread_mutex_unlock(&(lockarray[type]));
    }
}

unsigned long curl_initializer::thread_id(void)
{
    unsigned long ret;

    ret = (unsigned long)pthread_self();
    return (ret);
}

void curl_initializer::init_locks(void)
{
    int i;

    lockarray = (pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
    for (i = 0; i < CRYPTO_num_locks(); i++) {
        pthread_mutex_init(&(lockarray[i]), NULL);
    }

    CRYPTO_set_id_callback(thread_id);
    CRYPTO_set_locking_callback(locking_function);
}

void curl_initializer::kill_locks(void)
{
    int i;

    CRYPTO_set_locking_callback(NULL);

    for (i = 0; i < CRYPTO_num_locks(); i++) {
        pthread_mutex_destroy(&(lockarray[i]));
    }
    OPENSSL_free(lockarray);
}

// See here: http://m_curl.haxx.se/libcurl/c/curl_global_init.html
static curl_initializer _curl_init = curl_initializer();
namespace HD
{
/**
 * taken from
 * http://stackoverflow.com/questions/2329571/c-libcurl-get-output-into-a-string
 */
struct my_string {
    char *ptr;
    size_t len;
};

static size_t writefunc(void *ptr, size_t size, size_t nmemb, struct my_string *s)
{
    size_t new_len = s->len + size * nmemb;
    s->ptr = (char *)realloc(s->ptr, new_len + 1);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

bool init_string(struct my_string *s)
{
    s->len = 0;
    s->ptr = static_cast<char *>(malloc(s->len + 1));
    if (!s->ptr) {
        return false;
    }
    s->ptr[0] = '\0';
    return true;
}

HttpClient::HttpClient()
{
    m_timeout = 10000;
    m_verbose = 0;
    m_curl = curl_easy_init();

    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    // curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1);
    // SetProxy("socks5h://192.168.2.111:1080");
}

HttpClient::~HttpClient() { curl_easy_cleanup(m_curl); }

void HttpClient::SetVerbose(const long &verbose) { m_verbose = !!verbose; }
void HttpClient::SetUrl(const std::string &url) { m_url = url; }

void HttpClient::SetProxy(const std::string &proxy) { curl_easy_setopt(m_curl, CURLOPT_PROXY, proxy.c_str()); }

void HttpClient::SetTimeout(const long &timeout) { m_timeout = timeout; }

void HttpClient::AddHeader(const std::string &attr, const std::string &val) { m_headers[attr] = val; }

void HttpClient::RemoveHeader(const std::string &attr) { m_headers.erase(attr); }

bool HttpClient::doHttpGetRequest(const std::string &url, Json::Value &root,
                                  const std::vector<std::string> &header_array)
{
    CURLcode res;
    std::string result;
    struct my_string s;
    if (!init_string(&s)) {
        cerr << "init_string: Out of Memery!" << endl;
        return false;
    }
    struct curl_slist *headers = NULL;
    SetCurlOpt(url, header_array, &headers);

    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &s);

    res = curl_easy_perform(m_curl);

    result = s.ptr;
    if (s.ptr) {
        free(s.ptr);
    }

    if (headers) {
        curl_slist_free_all(headers);
    }

    return parse_response(res, result, root);
}

bool HttpClient::doHttpPostRequest(const std::string &url, const std::string &params, Json::Value &root,
                                   const std::vector<std::string> &header_array)
{
    CURLcode res;
    std::string result;
    struct my_string s;
    if (!init_string(&s)) {
        cerr << "init_string: Out of Memery!" << endl;
        return false;
    }

    struct curl_slist *headers = NULL;
    SetCurlOpt(url, header_array, &headers);

    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, params.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &s);

    res = curl_easy_perform(m_curl);

    result = s.ptr;
    if (s.ptr) {
        free(s.ptr);
    }

    if (headers) {
        curl_slist_free_all(headers);
    }

    return parse_response(res, result, root);
}

bool HttpClient::doHttpDeleteRequest(const std::string &url, const std::string &params, Json::Value &root,
                                     const std::vector<std::string> &header_array)
{
    CURLcode res;
    std::string result;
    struct my_string s;

    if (!init_string(&s)) {
        cerr << "init_string: Out of Memery!" << endl;
        return false;
    }
    struct curl_slist *headers = NULL;
    SetCurlOpt(url, header_array, &headers);

    curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, params.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &s);

    res = curl_easy_perform(m_curl);

    result = s.ptr;

    if (s.ptr) {
        free(s.ptr);
    }
    if (headers) {
        curl_slist_free_all(headers);
    }

    return parse_response(res, result, root);
}

void HttpClient::SetCurlOpt(const std::string &url, const std::vector<std::string> &header_array,
                            struct curl_slist **headers)
{
    for (const auto &header : m_headers) {
        *headers = curl_slist_append(*headers, (header.first + ": " + header.second).c_str());
    }

    for (auto &header : header_array) {
        *headers = curl_slist_append(*headers, header.c_str());
    }

    if (url.find("https://") != std::string::npos) {
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 0);
    }

    if (m_verbose)
        curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1);

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, *headers);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, m_timeout);
}

bool HttpClient::parse_response(CURLcode res, const std::string &response, Json::Value &root)
{
    Json::Reader reader;
    if (res != CURLE_OK) {
        std::stringstream msg;
        msg << "libcurl error: " << curl_easy_strerror(res);
        cerr << msg.str() << endl;
        return false;
    }

    long http_status = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_status);

    if (http_status != 200) {
        cerr << "http_status: " << http_status << endl;
        cerr << response << endl;
        return false;
    }

    if (!reader.parse(response, root)) {
        cerr << "[" << response << "]" << endl;
        return false;
    }

    return true;
}
}
