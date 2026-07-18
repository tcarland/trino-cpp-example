#include "trino-query.h"

#include <curl/curl.h>

#include <cctype>
#include <chrono>
#include <thread>

namespace trino {

// ── Internal helpers ──────────────────────────────────────────────────────────

namespace {

/// libcurl write callback: appends received bytes to a std::string.
size_t
writeCallback ( char* ptr, size_t size, size_t nmemb, std::string* out ) 
{
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

/// RAII wrapper around a CURL easy handle.
struct CurlHandle {
    CURL* h;
    
    CurlHandle() : h(curl_easy_init()) {
        if ( ! h ) 
            throw TrinoException("Failed to initialise libcurl handle");
    }
    ~CurlHandle() { curl_easy_cleanup(h); }

    operator CURL*() const { return h; }

    CurlHandle ( const CurlHandle& )            = delete;
    CurlHandle& operator= ( const CurlHandle& ) = delete;
};

/// RAII wrapper around a curl_slist header list.
struct CurlHeaders {
    curl_slist* list = nullptr;

    CurlHeaders()                                 = default;
    CurlHeaders ( const CurlHeaders& )            = delete;
    CurlHeaders& operator= ( const CurlHeaders& ) = delete;

    void add ( const std::string& header ) {
        list = curl_slist_append(list, header.c_str());
    }
    ~CurlHeaders() { curl_slist_free_all(list); }
};


/**
  * Locate a JSON string-valued field by key inside a raw JSON body.
  *
  * Searches for  "key": "value"  and returns the unescaped value string.
  * Returns an empty string when the key is absent or its value is not a
  * JSON string (e.g. null).  This is intentionally narrow — it is only
  * used to extract well-known fields like "nextUri" from Trino responses
  * whose structure we already understand.
 **/
std::string
extractStringField ( const std::string & body, const std::string & key ) 
{
    const std::string needle = "\"" + key + "\"";
    const auto kpos = body.find(needle);
    if ( kpos == std::string::npos )
        return {};

    auto pos = kpos + needle.size();

    // Skip whitespace, then expect ':'
    while ( pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos])) ) 
        ++pos;
    if ( pos >= body.size() || body[pos] != ':' ) 
        return {};
    ++pos;

    // Skip whitespace, then expect opening '"'
    while ( pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos])) )
        ++pos;
    if ( pos >= body.size() || body[pos] != '"' )
        return {};
    ++pos;

    // Read until the closing '"', honouring backslash escapes.
    std::string value;
    while ( pos < body.size() )
    {
        const char c = body[pos++];
        if ( c == '"' )
            break;
        if ( c == '\\' && pos < body.size() ) {
            const char esc = body[pos++];
            switch ( esc ) {
                case '"':  value += '"';  break;
                case '\\': value += '\\'; break;
                case '/':  value += '/';  break;
                case 'n':  value += '\n'; break;
                case 'r':  value += '\r'; break;
                case 't':  value += '\t'; break;
                default:   value += esc;  break;
            }
        } else {
            value += c;
        }
    }
    return value;
}

} // anonymous namespace



// ── Client ────────────────────────────────────────────────────────────────────

const std::string Client::Version = "0.1.5";


Client::Client ( std::string uri,    std::string catalog,
                 std::string schema, std::string user, 
                 std::string password )
  : uri_(std::move(uri)),
    catalog_(std::move(catalog)),
    schema_(std::move(schema)),
    user_(std::move(user)),
    password_(std::move(password))
{
    // Normalise: strip trailing slashes from the base URI.
    while ( ! uri_.empty() && uri_.back() == '/' )
        uri_.pop_back();
}

Client::~Client() = default;

// ── Private transport helpers ─────────────────────────────────────────────────

std::string
Client::submitQuery ( const std::string & sql ) const
{
    CurlHandle  curl;
    CurlHeaders hdrs;

    hdrs.add("X-Trino-User: "    + user_);
    hdrs.add("X-Trino-Catalog: " + catalog_);
    hdrs.add("X-Trino-Schema: "  + schema_);
    hdrs.add("Content-Type: text/plain");
    hdrs.add("Accept: application/json");

    const std::string url = uri_ + "/v1/statement";
    std::string responseBody;

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST,          1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    sql.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(sql.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    hdrs.list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &responseBody);

    // Enable basic authentication if password is provided
    if ( ! password_.empty() ) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERNAME, user_.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
    }

    const CURLcode rc = curl_easy_perform(curl);
    if ( rc != CURLE_OK ) {
        throw TrinoException(std::string("libcurl error: ") + curl_easy_strerror(rc));
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if ( httpCode < 200 || httpCode >= 300 ) {
        throw TrinoException("HTTP " + std::to_string(httpCode) +
                             " submitting query: " + responseBody);
    }

    return responseBody;
}
 

std::string
Client::fetchNext ( const std::string & nextUri ) const
{
    // Trino returns HTTP 503 when the coordinator is under back-pressure.
    // Retry the same URI after a short fixed delay.
    constexpr int  kMaxAttempts = 60;
    constexpr auto kRetryDelay  = std::chrono::milliseconds(100);

    for ( int attempt = 0; attempt < kMaxAttempts; ++attempt ) 
    {
        CurlHandle  curl;
        CurlHeaders hdrs;

        hdrs.add("X-Trino-User: " + user_);
        hdrs.add("Accept: application/json");

        std::string responseBody;

        curl_easy_setopt(curl, CURLOPT_URL,           nextUri.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET,       1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    hdrs.list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &responseBody);

        // Enable basic authentication if password is provided
        if ( ! password_.empty() ) {
            curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
            curl_easy_setopt(curl, CURLOPT_USERNAME, user_.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
        }

        const CURLcode rc = curl_easy_perform(curl);
        if ( rc != CURLE_OK )
            throw TrinoException(std::string("libcurl error: ") + curl_easy_strerror(rc));

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if ( httpCode == 200 )
            return responseBody;
        if ( httpCode == 503 ) {
            std::this_thread::sleep_for(kRetryDelay);
            continue;
        }

        throw TrinoException("HTTP " + std::to_string(httpCode) +
                             " fetching results: " + responseBody);
    }

    throw TrinoException("Exceeded maximum retry attempts polling: " + nextUri);
}


// ── Public API ────────────────────────────────────────────────────────────────

std::string
Client::selectAll ( const std::string & table, std::optional<int> limit )
{
    std::string sql = "SELECT * FROM " + table;
    
    if ( limit.has_value() )
        sql += " LIMIT " + std::to_string(*limit);

    return Select(sql);
}

std::string
Client::Select ( const std::string & sql )
{
    // Collect every raw response page into a JSON array so the caller receives
    // a single, valid JSON document containing the full result set.
    std::string output = "[";
    bool firstPage = true;

    std::string body = submitQuery(sql);

    while ( true ) {
        if ( ! firstPage )
            output += ',';
        output += body;
        firstPage = false;

        // extractStringField returns "" when "nextUri" is absent or null —
        // either means the coordinator has no more pages.
        const std::string nextUri = extractStringField(body, "nextUri");
        if ( nextUri.empty() )
            break;

        body = fetchNext(nextUri);
    }

    output += ']';
    return output;
}

} // namespace trino
