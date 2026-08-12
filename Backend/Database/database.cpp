// Project headers
#include "Database/database.h"
#include "SSH/SSHClient.h"
#include "Utils/Exceptions.h"

// System control flow headers
#include <unistd.h>
#include <sys/wait.h>
#include <system_error>
#include <stdexcept>

// Container headers
#include <queue>
#include <vector>
#include <string>

// File headers
#include <fstream>

// xDevAPI header
#include <mysqlx/xdevapi.h>

// cstdlib header
#include <cstdlib>

// Mutex
#include <mutex>

// Number headers
#include <algorithm>
#include <charconv>
#include <climits>

// I/O
#include <iostream>



namespace Database
{

    void uploadSchema
    (
        // SSH
        const bool& ssh_active,
        const std::string& user,
        const std::string& host,
        const int& port,

        // DB
        const std::string& dbURL,

        // Default
        const std::string& directory,
        const std::string& dbName,

        // SSH password
        const std::string& password
    )
    {
        if (!ssh_active)
        {
            // Create database
            try
            {
                auto session = Database::MySql(dbURL);

                session.getSession().sql("CREATE DATABASE IF NOT EXISTS " + dbName + ";").execute();
            }
            catch (std::exception& err)
            {
                std::cout << "Error: "
                          << err.what()
                          << '\n';
				std::cout << "--------\n" << std::flush;
            }

            // Subprocess
            pid_t pid = fork();

            if (pid == 0)
            {
                std::string query =
                    "sudo mysql " + dbName + " < " + directory;
                
                execlp
                (
                    "sh",
                    "sh",
                    "-c",
                    query.c_str(),
                    nullptr
                );

                perror("Schema exec failed");
                _exit(1);
            }

            int status;
            waitpid(pid, &status, 0);
        }
        else
        {
            SSH::remoteConnectTools SSHRemote;

            int connect = SSHRemote.connectClient(user, host, port, password);
            
            if (connect == SSH_OK)
            {
                std::cout << "Remote SSH | Connected client to session.\n";

                std::string cmd =
                    "sudo mysql -e \"CREATE DATABASE " + dbName + ";\"";

                try
                {
                    int err1 = SSHRemote.execSingleChannel(cmd);
                    int err2 = SSHRemote.writeFileSQL(directory, dbName);

                    if (err1 != SSH_OK)
                        throw std::runtime_error("Remote SSH | Create database failed.\n");
                    else
                        std::cout << "Remote SSH | Created database successfully.\n";
                    
                    if (err2 != SSH_OK)
                        throw std::runtime_error("Remote SSH | Cmd query failed.\n");
                    else
                        std::cout << "Remote SSH | Cmd query successful.\n";
                }
                catch (std::exception& err)
                {
                    std::cout << err.what();
                }

                SSHRemote.disconnectClient();
            }
            else
            {
                std::cout << "Remote SSH | Failed to connect to client.\n";
                _exit(-1);
            }

            std::cout << "Remote SSH | Schema successfully uploaded.\n";

            return;
        }
    }

    // MySql class functions

	// API functions
	std::string MySql::get_post(int Id, std::string& db_name, std::string& table_name)
	{
		try
		{
			std::string query = 
				"SELECT * FROM " + db_name + '.' + table_name + " WHERE Id = " + std::to_string(Id) + ';';
			auto result = sess.sql(query).execute();
			auto row = result.fetchOne();

			std::string finished_row;
			for (unsigned i = 0; i < row.colCount(); ++i)
			{
				auto value = row[i];

				switch (value.getType())
				{
					case mysqlx::Value::Type::STRING:
					{
						finished_row += " " + value.get<std::string>();
						break;
					}
					case mysqlx::Value::Type::INT64:
					{
						finished_row += " " + std::to_string(value.get<int64_t>());
						break;
					}
					case mysqlx::Value::Type::VNULL:
					{
						finished_row += " NULL";
						break;
					}
				}
			}

			return finished_row;
		}
		catch(std::exception& err)
		{
			return "Id not found in database";
		}
	}

	// Parser
	int MySql::getColumnCount
	(
		mysqlx::Session& session,
		const std::string& dbName,
		const std::string& tableName
	)
	{
		std::string query =
			"SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = '" + dbName + "' AND TABLE_NAME = '" + tableName + "';";
		
		auto count = session.sql(query).execute();
		auto row = count.fetchOne();
		int num = row[0];

		return num;	
	}

	std::vector<std::string> MySql::get_row_types
	(
		mysqlx::Session& sess,
		const std::string row_name,
		const std::string db_name
	)
	{
		sess.sql("USE " + db_name + ";").execute();

		std::vector<std::string> type_vec;

		auto result = sess.sql("DESCRIBE " + row_name + ";").execute();
		mysqlx::Row row;

		while ((row = result.fetchOne()))
		{
			type_vec.emplace_back(row[1].get<std::string>());
		}

		return type_vec;
	}

    int MySql::bulkLoad
    (
        mysqlx::Session& sess,
        const std::string& db_Name,
        const std::string& tableName,
        std::queue<std::string>& chunk,
		std::vector<std::string>& row_types,
		std::mutex& db_mutex,
		std::ofstream& err_log,
		ErrorData& errC
    )
    {
		std::lock_guard<std::mutex> lock(db_mutex);

        sess.startTransaction();

		std::string test;

        try
        {
            std::string enter_db_query =
                "USE " + db_Name + ";";

            sess.sql(enter_db_query).execute();

			std::string stmt =
				"INSERT INTO " + tableName + " VALUES ";

			std::stringstream query;

			query << stmt;

			bool query_first = true;

            while (!chunk.empty())
            {
                std::stringstream chunkLine(chunk.front());

                std::string cell;
				
				std::stringstream entry;

				entry << "(";
				
				bool bad_row = false;

				bool insert_first = true;

				int index = 0;

                while (std::getline(chunkLine, cell, '\t'))
                {
					if (!insert_first)
						entry << ", ";
					
					if (cell == "NULL")
					{
						entry << "NULL";
						insert_first = false;
						continue;
					}

					long long val;

					auto result = std::from_chars
					(
						cell.data(),
						cell.data() + cell.size(),
						val
					);

					if (result.ec == std::errc::result_out_of_range)
					{
						errC.bad_interger++;
						bad_row = true;
					}
					else if (result.ec != std::errc() || result.ptr != cell.data() + cell.size())
					{
						if (cell.find(":") != std::string::npos)
							entry << "'" << cell << "'";
						else
						{
							if ((val > INT_MAX || val < INT_MIN) && row_types[index] == "int")
							{
								errC.bad_interger++;
								bad_row = true;
							}
							else
								entry << "'" << val << "'";
						}
					}
					else
					{
						if (val > INT_MAX || val < INT_MIN)
						{
							errC.bad_interger++;
							bad_row = true;
						}
						else
							entry << val;
					}

					index++;

					insert_first = false;
                }

				entry << ")";
				
				if (!bad_row)
				{
					if (!query_first)
						query << ", ";
					query << entry.str();

					query_first = false;
				} else
				{
					errC.discarded_rows++;

					err_log << "Failed to parse line(DATABASE): " << entry.str() << '\n';
				}
				if (!chunk.empty())
				{
                	chunk.pop();
				}
            }

			query << ";";

			test = query.str();

			sess.sql(query.str()).execute();

            sess.commit();

            return 1;
        }
        catch (std::exception& e)
        {
            sess.rollback();
            
			err_log << '\n' << test << '\n';
			std::cout << '\n' << "Failed to parse chunk | " << e.what() << '\n';
            
            return -1;
        }
    }

}
