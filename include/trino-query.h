#pragma once

#include <optional>
#include <stdexcept>
#include <string>

namespace trino {

/// Thrown on HTTP or network failure.
/// Trino query-level errors are visible inside the returned JSON.
class TrinoException : public std::runtime_error {
public:
    explicit TrinoException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * Lightweight Trino REST client backed by libcurl.
 *
 * Typical usage:
 *   trino::Client client("http://localhost:8080", "tpch", "sf1");
 *   std::string json = client.selectAll("orders", 100);
 *
 * The client is not thread-safe; create one instance per thread.
 */
class Client {
public:
    /**
     * @param uri      Base URI of the Trino coordinator, e.g. "http://host:8080".
     *                 A trailing slash is ignored.
     * @param catalog  Trino catalog name (X-Trino-Catalog header).
     * @param schema   Trino schema name  (X-Trino-Schema  header).
     * @param user     Trino user identity (X-Trino-User   header). Defaults to "trino".
     * @param password Password for basic authentication (optional).
     */
    Client(std::string uri,
           std::string catalog,
           std::string schema,
           std::string user = "trino",
           std::string password = "");

    ~Client();

    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&)                 = default;
    Client& operator=(Client&&)      = default;

    /**
     * Execute  SELECT * FROM <table> [LIMIT <limit>].
     *
     * Pages through every nextUri returned by the coordinator and retries
     * automatically on HTTP 503 back-pressure responses.
     *
     * @param table  Fully- or partially-qualified table name.
     * @param limit  Optional LIMIT clause value.
     * @return       Raw JSON array where each element is one Trino response page.
     * @throws TrinoException on transport or HTTP failure.
     */
    std::string selectAll(const std::string& table,
                          std::optional<int> limit = std::nullopt);

private:
    std::string uri_;
    std::string catalog_;
    std::string schema_;
    std::string user_;
    std::string password_;

    // POST the SQL to /v1/statement; returns the raw JSON response body.
    std::string submitQuery(const std::string& sql) const;

    // GET a nextUri, retrying automatically on HTTP 503. Returns JSON body.
    std::string fetchNext(const std::string& nextUri) const;
};

} // namespace trino
