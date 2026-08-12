// Project headers
#include "Database/database.h"
#include "SSH/SSHClient.h"
#include "Utils/Exceptions.h"

// File handling
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

// JSON handling
#include "json.hpp"

// Containers and strings
#include <unordered_set>
#include <queue>
#include <string>
#include <vector>

// Mutex
#include <mutex>

// Error handling
#include <stdexcept>

// I/O
#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>



namespace TSVCleaner
{

    class Cleaner
    {
    private:

        std::unordered_set<char> garbage =
        {
            '\0', '\r', '\x01', '\x02', '\x03', '\x04',
            '\x05', '\x06', '\x07', '\x08',
            '\x0B', '\x0C',
            '\x0E', '\x0F',
            '\x10', '\x11', '\x12', '\x13', '\x14',
            '\x15', '\x16', '\x17', '\x18', '\x19',
            '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F',
            '\x7F'
        };

    public:

        int CleanLine
        (
            std::stringstream& lineRef,
            const char& delimeter,
			int column_count,
			std::vector<std::string>& row_types,
			bool& bad_row,
			ErrorData& errC
        )
        {
            int err = 0;

            // Line reconstruction stream
            std::stringstream newLine;

            std::string cell;
			
			int columns_counted = 0;

			int index = 0;

            while (std::getline(lineRef, cell, delimeter))
            {
                std::string line;
		
				if (cell.empty())
				{
					newLine << "NULL" << '\t';
					err++;
					index++;
				}
				else
				{
					// Clear garbage values and check for type
					bool contains_dash = false;
					bool contains_colon = false;
					bool contains_dot = false;
					bool is_all_nums = true;
					bool is_all_spaces = true;

					for (char& c : cell)
					{
						if (garbage.find(c) != garbage.end())
						{
							err++;
							continue;
						}
						else
							line += c;

						if (c == '-') contains_dash = true;
						else if (c == ':') contains_colon = true;
						else if (c == '.') contains_dot = true;
						else if (c != '-' && c != ':' && c != ' ' && !std::isdigit(static_cast<unsigned char>(c))) is_all_nums = false;
						else if (c != ' ') is_all_spaces = false;
					}
					
					std::string escaped_line;
					escaped_line.reserve(line.size());

					for (const char& c : line)
					{
						if (c == '\'')
						{
							err++;
							escaped_line += "''";
						}
						else if (c == '\\')
						{
							err++;
							escaped_line += "\\\\";
						}
						else
							escaped_line += c;
					}

					auto check_literal_int = [&](const std::string& s) -> bool
					{
						bool is_all_spaces = true;

						size_t i = 0;

						while (i < s.size() && s[i] == ' ') i++;
						if (i < s.size() && (s[i] == '-' || s[i] == '+'))
						{
							is_all_spaces = false;
							i++;
						}

						if (s[i] == ' ')
						{
							bad_row = true;
							return false;
						}

						bool is_digit = false;
						bool seen_space = false;

						for (; i < s.size(); i++)
						{
							if (std::isdigit(static_cast<unsigned char>(s[i])))
							{
								if (seen_space == true)
									bad_row = true;
								is_all_spaces = false;
								is_digit = true;
							}
							else if (s[i] == ' ')
							{
								seen_space = true;
								continue;
							}
							else
							{
								is_all_spaces = false;
								return false;
							}
						}

						if (is_all_spaces)
						{
							bad_row = true;
							return false;
						}

						return is_digit;
					};

					// Check conditions
					if (contains_dash == true && contains_colon == true && is_all_nums == true && contains_dot == true)
					{
						if (row_types[index] == "datetime")
						{
							newLine << escaped_line << '\t';
						}
						else
						{
							errC.bad_date++;
							bad_row = true;
							return err;
						}
					}
					else if (is_all_nums == true)
					{
						if (row_types[index] == "int" && !is_all_spaces)
						{
							if (check_literal_int(cell) == true)
							{
								newLine << escaped_line << '\t';
							}
							else
							{
								errC.bad_interger++;
								bad_row = true;
								return err;
							}
						}
						else
						{
							errC.malformed_row++;
							bad_row = true;
							return err;
						}
					}
					else
					{
						if (row_types[index].find("varchar") != std::string::npos || row_types[index] == "mediumtext")
						{
							newLine << escaped_line << '\t';
						}
						else
						{
							errC.malformed_row++;
							bad_row = true;
							return err;
						}
					}

					index++;
				}

				columns_counted++;
            }

			if (columns_counted < column_count)
			{
				while ((column_count - columns_counted) > 0)
				{
					newLine << "NULL" << '\t';
                    err++;
					column_count--;
				}
			}
            
            std::string reconstLine = newLine.str();

            if (err > 0)
            {
                lineRef.clear();
                lineRef.str(reconstLine);
            }

            lineRef.seekg(0, std::ios::beg);
			
            return err;
        }

    };

}



namespace TSVEngine
{

    using RowData = std::queue<std::string>;

    class Parser
    {
    private:

        char delimiter;

    public:

        Parser(char delim = '\t') : delimiter(delim) {}

        ErrorData ParseFile
        (
            TSVCleaner::Cleaner FileCleaner,
            const std::string& TsvFile,
            const std::string& dbName,
            const std::string& dbURL,
			std::ofstream& err_log
        )
        {
		
			// Error container
			ErrorData errC;

			// Open new timer thread
			std::atomic<bool> timer_loop(true);
			std::thread timer([&]()
			{
				while (timer_loop.load())
				{
					errC.elapsed_time += 0.1;
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			});

            // Table name
            std::filesystem::path headerFile(TsvFile);
            std::string headerName = headerFile.filename();
            size_t pos = headerName.find_last_of('.');

            std::string tableName = headerName;

            if (pos != std::string::npos)
            {
                tableName = tableName.substr(0, pos);
            }

            // Mysql Session
            auto session = Database::MySql(dbURL);
			
			// Get column count
			int col_count = session.getColumnCount(session.getSession(), dbName, tableName);

            // Open file
            std::ifstream file(TsvFile, std::ios::binary);

            // Get file size and set bytes read variable for percentage complete
            uint64_t fsize = std::filesystem::file_size(TsvFile);

            uint64_t bytesRead = 0;

            if (!file.is_open())
            {
                std::cerr << TsvFile
                          << " | Failed to open."
                          << std::endl;
				errC.total_rows = 0;
				errC.discarded_rows = 0;
				errC.bad_interger = 0;
				errC.bad_date = 0;
				errC.malformed_row = 0;
                return errC;
            }

            // Parse file
            const size_t CHUNK_SIZE = 5000;

            RowData chunk;

            std::string line;
			
			// Read lock
			std::mutex db_mutex;
			
			// Progress bar
			int bar_width = 25;

			auto row_types = session.get_row_types(session.getSession(), tableName, dbName);

            while (std::getline(file, line))
            {
				errC.total_rows++;

				// Error check file
				if (!file.good()) std::cout << '\n' << "FILE STREAM BROKE\n" << std::flush;
				if (file.eof()) std::cout << '\n' << "EOF REACHED\n" << std::flush;
				if (file.fail()) std::cout << '\n' << "FAIL STATE\n" << std::flush;
				if (file.bad()) std::cout << '\n' << "BAD STATE\n" << std::flush;

                std::stringstream ParseLine(line);
                
                // Clean each line
				bool bad_row = false;
                FileCleaner.CleanLine(ParseLine, delimiter, col_count, row_types, bad_row, errC);

				if (bad_row == true)
				{
					errC.discarded_rows++;
					
					err_log << "#---------------------------------------#" << '\n';
					err_log << "Failed to parse line(RAW): " << line << '\n';
					err_log << "Failed to parse line(CLEANER): " << ParseLine.str() << '\n';	
					err_log << "#---------------------------------------#" << '\n';
					continue;
				}

                // Update bytes read
                uint64_t line_size = line.size();
                bytesRead += line_size;

                if (bytesRead > fsize)
                    bytesRead = fsize;

                // percentage formula
                double percentage = 
                    (static_cast<double>(bytesRead) / fsize) * 100;

				// Bar formula
				int bar_percentage =
					static_cast<int>((percentage / 100) * bar_width) ;

				// Bar-remaining
				int bar_remaining = 
					bar_width - bar_percentage;

                std::cout << "\r\033[KProgress: [";

				for (int i = 0; i <= bar_percentage; i++)
					std::cout << "█";
				for (int i = 0; i < bar_remaining; i++)
					std::cout << "-";

				std::cout << "] " << percentage << "%";

				// Convert total bytes remaining to seconds
				if (errC.elapsed_time > 0)
				{
					double rate = bytesRead / errC.elapsed_time;
					uint64_t remaining = (fsize - bytesRead) / rate;

					uint64_t hours = remaining / 3600;
					uint64_t minutes = (remaining % 3600) / 60;
					uint64_t seconds = remaining % 60;
					
					std::cout << " | " << "Estimated time remaining: ";
					if (hours > 0) std::cout << hours << "hrs ";
					if (minutes > 0) std::cout << minutes << "mins ";
					if (seconds > 0) std::cout << seconds << "sec ";
				}
				
				std::cout << std::flush;

                chunk.push(ParseLine.str());

                if (chunk.size() >= CHUNK_SIZE)
                {
                    try
                    {
                        if (session.bulkLoad
                        (
                            session.getSession(),
                            dbName,
                            tableName,
                            chunk,
							row_types,
							db_mutex,
							err_log,
							errC
                        ) < 0) throw std::runtime_error("Line failed to parse.");
                    }
                    catch (std::exception& err)
                    {
                        std::cout << "Parser terminated on exit code: "
                                  << err.what()
                				  << " | For file: "
								  << tableName
								  << " | Line: "
								  << line
                                  << '\n';
                        _exit(-1);
                    }

                    while (!chunk.empty())
                    {
                        chunk.pop();
                    }
                }
            }

            if (!chunk.empty())
            {
                try
                {
                    session.bulkLoad
                    (
                        session.getSession(),
                        dbName,
                        tableName,
                        chunk,
						row_types,
						db_mutex,
						err_log,
						errC
                    );
                }
                catch (std::exception& err)
                {
                    std::cout << "Parser terminated on exit code: "
                                << err.what()
                                << '\n';
                    _exit(-1);
                }

                while (!chunk.empty())
                {
					if (!chunk.empty())
					{
                    	chunk.pop();
					}
                }
            }

            std::cout << "\r\033[KProgress: [";

			for (int i = 0; i <= bar_width; i++)
				std::cout << "█";
				
			std::cout << "] " << 100 << "%\n" << std::flush;

            file.close();
			
			timer_loop = false;
			if (timer.joinable())
				timer.join();

            return errC;
        }

    };

}



int main()
{

    // TSVEngine
    TSVEngine::Parser TSVParser;

    // TSVCleaner
    TSVCleaner::Cleaner FileCleaner;

    // SSH remote
    SSH::remoteConnectTools SSHRemote;

    // ---- Program configs ----
    std::ifstream Config
    (
        "Backend/Parser/Config/ParserConfig.json"
    );
    nlohmann::json configurations;

    bool opened = Config.is_open();

    if (!opened)
    {
        std::cerr << "Config file failed to open\n";
        return -1;
    }

    // Load Config into nlohmann structure
    try
    {
        Config >> configurations;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load config file\n"
                  << e.what()
                  << '\n';
        return -1;
    }

    // Error handling
    if (!configurations.contains("ssh") || !configurations["ssh"].is_object())
    {
        std::cerr << "Missing ssh config\n";
        return -1;
    }
    if (!configurations.contains("paths") || !configurations["paths"].is_object())
    {
        std::cerr << "Missing paths config\n";
        return -1;
    }
    if (!configurations.contains("schema") || !configurations["schema"].is_object())
    {
        std::cerr << "Missing schema config\n";
        return -1;
    }
    if (!configurations.contains("db") || !configurations["db"].is_object())
    {
        std::cerr << "Missing db config\n";
        return -1;
    }
    if (!configurations.contains("local_machine") || !configurations["local_machine"].is_object())
    {
        std::cerr << "Missing localhost config\n";
        return -1;
    }

    // Configs
    const auto& schema = configurations.at("schema");
    const auto& db = configurations.at("db");
    const auto& ssh = configurations.at("ssh");
    const auto& paths = configurations.at("paths");
    const auto& local = configurations.at("local_machine");

    // Error handling
    try
    {
        // SCHEMA
        schema.at("upload_schema");

        // DATABASE
        db.at("database_name");

        // SSH
        ssh.at("active");
        ssh.at("user");
        ssh.at("host");
        ssh.at("port");

        // PATHS
        paths.at("default_directory");
        paths.at("ssh_directory");
        paths.at("schema");
    }
    catch (std::exception& e)
    {
        std::cout << "Error has occured: "
                  << e.what()
                  << '\n';
        return -1;
    }

    // Config objects
    struct SSHC_Obj
    {
        bool active;
        std::string user;
        std::string host;
        int port;
    };

    struct PATHC_Obj
    {
        std::string default_directory;
        std::string ssh_directory;
        std::string schema;
    };

    struct DB_Obj
    {
        std::string db_Name;
        std::string db_URL;
        std::string remote_host;
        int remote_port;
    };

    const SSHC_Obj ssh_config
    {
        ssh.at("active"),
        ssh.at("user"),
        ssh.at("host"),
        ssh.at("port")
    };

    const PATHC_Obj paths_config
    {
        paths.at("default_directory"),
        paths.at("ssh_directory"),
        paths.at("schema")
    };

    const DB_Obj db_config
    {
        db.at("database_name"),
        db.at("url"),
        db.at("remote_host"),
        db.at("remote_port")
    };

    // Other config objects/variables

    // SCHEMA
    const bool upload_schema = schema.at("upload_schema");

    // LOCAL
    const std::string source_host = local.at("source_host");
    const int local_port = local.at("local_port");

    // --------

    // ---- Config setup ----
    std::string directory;

    std::thread tunnel_thread;
    std::string password;

    std::cout << "Enter password: \n";
    std::getline(std::cin, password);

    if (ssh_config.active == true)
    {
        // Set directory for file path
        directory = paths_config.ssh_directory;

        // Open TCP socket
        const char* localhost = source_host.c_str();
        const char* rm_host = db_config.remote_host.c_str();

        tunnel_thread = std::thread
        (
            &SSH::remoteConnectTools::open_TCP_Tunnel,
            &SSHRemote,
            rm_host,
            db_config.remote_port,
            localhost,
            local_port,
            ssh_config.user,
            ssh_config.host,
            ssh_config.port,
            password
        );
    }
    else
    {
        // Set directory for file path
        directory = paths_config.default_directory;
    }
    
    // --------

    // Upload Schema
    if (upload_schema == true)
    {
        // Functions with SSH if turned ON
        Database::uploadSchema
        (
            ssh_config.active,
            ssh_config.user,
            ssh_config.host,
            ssh_config.port,
            db_config.db_URL,
            paths_config.schema,
            db_config.db_Name,
            password
        );
    }

    // Parse files
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
		// Error Log
		std::string err_file_name = entry.path().filename().string();
		size_t pos = entry.path().filename().string().find_last_of('.');
		
		if (pos != std::string::npos)
			err_file_name = err_file_name.substr(0, pos);

		std::string err_file_path =
			"Backend/Parser/Error_Logs/" + err_file_name + ".txt";

		std::filesystem::path err_path = err_file_path;

		if (std::filesystem::exists(err_path))
		{
			std::ofstream wipe_file(err_file_path, std::ios::trunc);
			wipe_file.close();
		}

		std::ofstream err_log(err_file_path, std::ios::app);

		if (!err_log.is_open())
		{
			std::cout << "Error log for file " + err_file_name + " failed to open.\n";
			_exit(-1);
		}

        std::cout << "# ----------------------\n";
        std::cout << "Starting parser on: "
                  << entry.path().filename().string()
                  << std::endl;
        std::cout << "# ----------------------\n";

        ErrorData errors = TSVParser.ParseFile
        (
            FileCleaner,
            entry.path(),
            db_config.db_Name,
            db_config.db_URL,
			err_log
        );

		unsigned int hours = (static_cast<int>(errors.elapsed_time) / 60) / 60;
		unsigned int minutes = (static_cast<int>(errors.elapsed_time) % 3600) / 60;
		unsigned int real_seconds = (static_cast<int>(errors.elapsed_time) % 3600) % 60;

		// Write to error log
		err_log << "Error log for file: " + entry.path().filename().string() << '\n'
			    << "# -----------------------------#\n"
				<< "Total Rows: " << errors.total_rows << '\n'
			    << "Rows Discarded: " << errors.discarded_rows << '\n'
				<< '\n'
				<< "Bad Intergers: " << errors.bad_interger << '\n'
				<< "Bad Dates: " << errors.bad_date << '\n'
				<< "Malformed Rows: " << errors.malformed_row << '\n'
				<< '\n'
				<< "Time finished: ";
				if (hours > 0) err_log << hours << "hrs ";
				if (minutes > 0) err_log << minutes << "mins ";
				err_log << real_seconds << "sec";

        std::cout << "# ----------------------\n";
        std::cout << "Finished parsing for: "
                  << entry.path().filename().string()
                  << std::endl;
        std::cout << "# ----------------------\n";
    }

    // Close TCP socket if SSH ON
    if (ssh_config.active == true)
    {
        SSHRemote.close_TCP_Tunnel();

        if (tunnel_thread.joinable())
            tunnel_thread.detach();
    }
    
    return 0;

}
