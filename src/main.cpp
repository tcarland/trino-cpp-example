#include "trino-query.h"

extern "C" {
#include <getopt.h>
#include <string.h>
#include <unistd.h>
}

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <curl/curl.h>


static void
usage ( const char * prog )
{
    std::cerr
        << "Usage: " << prog << " [-u:U:P:q:] <catalog> <schema> <table> [limit]\n"
        << "       " << prog << " [-u:U:P:] -q <query> <catalog> <schema>\n"
        << "  -h | --help           :  Show usage info and exit. \n"
        << "  -u | --uri    <uri>   :  Trino coordinator base URI  (e.g. http://localhost:8080)\n"
        << "  -U | --user   <name>  :  Username for basic authentication. Default: trino\n"
        << "  -P | --pwfile <file>  :  Path to file containing the password. Default: .trino_password\n"
        << "  -q | --query  <stmt>  :  Execute custom SQL query instead of SELECT * FROM <table>\n"
        << "   catalog              :  Trino catalog name\n"
        << "   schema               :  Trino schema / database name\n"
        << "   table                :  Table to query (SELECT * FROM <table>)\n"
        << "   limit                :  Optional maximum number of rows to return\n";
}

static int maxstrlen = 1024;


int 
main ( int argc, char * argv[] ) 
{
    char   optChar;
    char  *uristr   = nullptr;
    char  *userstr  = nullptr;
    char  *pwfile   = nullptr;
    char  *querystr = nullptr;
    
    std::string uri;
    std::string user = "trino";
    std::string pwf  = ".trino_password";
    std::optional<std::string> customQuery;

    int optindx = 0;
    static struct option l_opts[] = { {"help",   no_argument, 0, 'h'},
                                      {"uri",    required_argument, 0, 'u'},
                                      {"user",   required_argument, 0, 'U'},
                                      {"pwfile", required_argument, 0, 'P'},
                                      {"query",  required_argument, 0, 'q'},
                                      {0,0,0,0}
                                    };

    while ( (optChar = ::getopt_long(argc, argv, "hu:U:P:q:", l_opts, &optindx)) != EOF ) {
        switch ( optChar ) {
            case 'u':
                uristr  = ::strdup(optarg);
                break;
            case 'U':
                userstr = ::strdup(optarg);
                break;
            case 'P':
                pwfile  = ::strdup(optarg);
                break;
            case 'q':
                querystr = ::strdup(optarg);
                break;
            default:
                break;
        }
    }

    // Parse custom query if provided
    if ( querystr != nullptr && ::strnlen(querystr, maxstrlen) > 0 ) {
        customQuery = querystr;
        ::free(querystr);
    }

    // Check that we have the required positional arguments
    // In query mode: catalog, schema (2 args)
    // In table mode: catalog, schema, table (3 args)
    const int requiredArgs = customQuery.has_value() ? 2 : 3;
    if ( argc - optind < requiredArgs ) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string catalog  = argv[optind];
    const std::string schema   = argv[optind + 1];
    std::string table;
    if ( !customQuery.has_value() ) {
        table = argv[optind + 2];
    }

    if ( uristr != nullptr && ::strnlen(uristr, maxstrlen) > 0 ) {
        uri = uristr;
        ::free(uristr);
    } else {
        std::cerr << "Trino URI is a required parameter" << std::endl;
        return EXIT_FAILURE;
    }

    if ( userstr != nullptr && ::strnlen(userstr, maxstrlen) > 0 ) {
        user = userstr;
        ::free(userstr);
    }

    if ( pwfile != nullptr && ::strnlen(pwfile, maxstrlen) > 0 ) {
        pwf = pwfile;
        ::free(pwfile);
    }

    // Read password from file
    std::string password;
    {
        std::ifstream ifs(pwf);
        if ( ! ifs ) {
            std::cerr << "Error: cannot open password file '" << pwf << "'\n";
            return EXIT_FAILURE;
        }
        std::getline(ifs, password);
        if ( password.empty() ) {
            std::cerr << "Error: password file is empty\n";
            return EXIT_FAILURE;
        }
        // Trim trailing whitespace/newline
        while ( ! password.empty() && std::isspace(static_cast<unsigned char>(password.back())) )
            password.pop_back();
    }

    std::optional<int> limit;
    if ( !customQuery.has_value() && argc - optind >= 4 ) {
        try {
            const int n = std::stoi(argv[optind + 3]);
            if (n <= 0) throw std::invalid_argument("must be a positive integer");
            limit = n;
        } catch ( const std::exception & ex ) {
            std::cerr << "Error: invalid limit value '" << argv[optind + 3]
                      << "': " << ex.what() << '\n';
            return EXIT_FAILURE;
        }
    }

    // curl_global_init must be called once before any curl handle is used.
    curl_global_init(CURL_GLOBAL_DEFAULT);

    int exitCode = EXIT_SUCCESS;
    try {
        trino::Client client(uri, catalog, schema, user, password);
        if ( customQuery.has_value() ) {
            std::cout << client.Select(*customQuery) << '\n';
        } else {
            std::cout << client.selectAll(table, limit) << '\n';
        }
    } catch ( const trino::TrinoException & ex ) {
        std::cerr << "Trino error: " << ex.what() << '\n';
        exitCode = EXIT_FAILURE;
    } catch ( const std::exception & ex ) {
        std::cerr << "Error: " << ex.what() << '\n';
        exitCode = EXIT_FAILURE;
    }

    curl_global_cleanup();
    return exitCode;
}
