#pragma once

// Projects headers
#include "Utils/Exceptions.h"

// External libraries
#include <unistd.h>

// File handling
#include <fstream>
#include <sstream>

// Containers
#include <queue>
#include <vector>
#include <string>

// xDevAPI header
#include <mysqlx/xdevapi.h>

// Mutex
#include <mutex>

// I/O
#include <iostream>



namespace Database
{

    class MySql
    {
    private:
        mysqlx::Session sess;

    public:
        MySql(const std::string url) : sess(url)
        {
            try
            {
                mysqlx::RowResult res = sess.sql("show variables like 'version'").execute();
                std::stringstream version;

                version << res.fetchOne().get(1).get<std::string>();
                int major_version;
                version >> major_version;
                
                if (major_version < 8)
                {
                    std::cout << "Can only work with MySQL version 8 or later!\n";
                    std::cout << "Done!\n";

                    throw std::runtime_error
                    (
                        " MySql session denied, MySql version is outdated!(version 8 or later)"
                    );
                }

                std::cout << "MySql session accepted, creating collection\n";
                std::cout << "# ----------------------\n";
            }
            catch (std::exception& e)
            {
                std::cerr << e.what();
                _exit(1);
            }
        }

        mysqlx::Session& getSession()
        {
            return sess;
        }

        int bulkLoad
        (
            mysqlx::Session&,
            const std::string&,
            const std::string&,
            std::queue<std::string>&,
			std::vector<std::string>&,
			std::mutex&,
			std::ofstream&,
			ErrorData&
        );

		
		int getColumnCount
		(
			mysqlx::Session&,
			const std::string&,
			const std::string&
		);

		std::vector<std::string> get_row_types
		(
			mysqlx::Session&,
			const std::string,
			const std::string
		);

    };

    void uploadSchema
    (
        // SSH
        const bool&,
        const std::string&,
        const std::string&,
        const int&,

        // DB
        const std::string&,

        // Default
        const std::string&,
        const std::string&,

        // SSH password
        const std::string&
    );
	
}
