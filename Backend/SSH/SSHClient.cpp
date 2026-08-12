// Project headers
#include "SSH/SSHClient.h"

// External libraries
#include <libssh/libssh.h>
#include <stdlib.h>
#include <unistd.h>

// Containers
#include <atomic>
#include <optional>

// Socket handling
#include <sys/socket.h>
#include <netinet/in.h>

// File handling
#include <fstream>
#include <sstream>

// Mutex headers
#include <mutex>

// I/O
#include <thread>
#include <iostream>



namespace SSH
{
    // General SSH

    // Open connections
    int SSH::remoteConnectTools::connectClient
    (
        const std::string& user,
        const std::string& host,
        const std::optional<int>& port,
        const std::string& password
    )
    {
        client = ssh_new();

        if (client == nullptr)
        {
            return SSH_ERROR;
        }

        ssh_options_set(client, SSH_OPTIONS_USER, user.c_str());
        ssh_options_set(client, SSH_OPTIONS_HOST, host.c_str());

        int portNum = port.value_or(22);
        ssh_options_set(client, SSH_OPTIONS_PORT, &portNum);

        int rc;
        rc = ssh_connect(client);

        if (rc != SSH_OK)
        {
            std::cout << "\nRemote SSH | Error has ocurred | "
                      << ssh_get_error(client)
                      << '\n';

            ssh_disconnect(client);
            ssh_free(client);
            return SSH_ERROR;
        }

        // Authenticate session
        if (authenticateSession() != SSH_OK)
            return SSH_ERROR;
        // Authenticate user
        if (authenticateUser(password) != SSH_OK)
            return SSH_ERROR;

        return SSH_OK;
    }

    int SSH::remoteConnectTools::open_interactive_terminal()
    {
        int rc;

        shell = ssh_channel_new(client);
        if (shell == nullptr)
            return SSH_ERROR;

        rc = ssh_channel_open_session(shell);
        if (rc != SSH_OK)
        {
            ssh_channel_free(shell);
            return rc;
        }
        
        rc = ssh_channel_request_shell(shell);
        if (rc != SSH_OK)
            return rc;

        return rc;
    }

    // Authentication
    int SSH::remoteConnectTools::authenticateSession()
    {
        unsigned char *hash = nullptr;
        size_t hlen;
        ssh_key srv_pubkey = nullptr;
        int rc;

        rc = ssh_get_server_publickey(client, &srv_pubkey);
        if (rc < 0)
            return SSH_ERROR;

        rc = ssh_get_publickey_hash
        (
            srv_pubkey,
            SSH_PUBLICKEY_HASH_SHA256,
            &hash,
            &hlen
        );

        ssh_key_free(srv_pubkey);
        if (rc < 0)
            return SSH_ERROR;
        
        auto state = ssh_session_is_known_server(client);
        switch (state)
        {
            case SSH_KNOWN_HOSTS_OK :
            {
                std::cout << "\nRemote SSH | Session authenticated successfully.";
                break;
            }
            case SSH_KNOWN_HOSTS_CHANGED :
            {
                std::cout << "Remote SSH | Host key for server has changed: new key: \n";
                ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
                std::cout << "For Security reasons, the connection will be terminated.\n";
                ssh_clean_pubkey_hash(&hash);

                return SSH_ERROR;
            }
            case SSH_KNOWN_HOSTS_OTHER :
            {
                std::cout << "Remote SSH | The host key for this server was not found but another key type exists.\n"
                          << "An attacker might change the default server key to confuse your client into thinking the key doesn't exist!\n";
                ssh_clean_pubkey_hash(&hash);

                return SSH_ERROR;
            }
            case SSH_KNOWN_HOSTS_NOT_FOUND :
            {
                std::cout << "Remote SSH | Could not find known host file.\n"
                          << "If you accept the host key here, the file will be automatically created.\n";
                
                std::string input;

                std::cout << "Enter 'yes' to create new key file: \n";
                std::getline(std::cin, input);
                if (input != "yes")
                    return SSH_ERROR;
                
                rc = ssh_session_update_known_hosts(client);
                if (rc < 0)
                    return SSH_ERROR;
                
                break;
            }
            case SSH_KNOWN_HOSTS_UNKNOWN :
            {
                std::cout << "The server is unknown. Do you trust the host key?\n"
                          << "Enter 'yes' to create public key hash: \n";
                ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
                ssh_clean_pubkey_hash(&hash);

                std::string input;

                std::getline(std::cin, input);
                if (input != "yes")
                    return SSH_ERROR;
                
                rc = ssh_session_update_known_hosts(client);
                if (rc < 0)
                    return SSH_ERROR;
                
                break;
            }
            case SSH_KNOWN_HOSTS_ERROR :
            {
                std::cout << "Remote SSH | Error has occured: \n"
                          << ssh_get_error(client);
                
                ssh_clean_pubkey_hash(&hash);
                return SSH_ERROR;
            }
        }

        ssh_clean_pubkey_hash(&hash);
        return SSH_OK;
    }

    int SSH::remoteConnectTools::authenticateUser(const std::string& password)
    {
        int rc;

        rc = ssh_userauth_password(client, nullptr, password.c_str());
        if (rc != SSH_AUTH_SUCCESS)
        {
            std::cout << "Remote SSH | Password authentication failed: \n"
                      << ssh_get_error(client);

            ssh_disconnect(client);
            ssh_free(client);
            return SSH_ERROR;
        }

        std::cout << "Remote SSH | User authenticated successfully.\n";
        return SSH_OK;
    }

    // Disconnect
    int SSH::remoteConnectTools::disconnectClient()
    {
        if (client == nullptr)
        {
            return -1;
        }

        ssh_disconnect(client);
        ssh_free(client);

        client = nullptr;

        return 1;
    }

    // Run Commands
    int SSH::remoteConnectTools::execSingleChannel(const std::string& cmd)
    {
        ssh_channel channel = nullptr;
        char buffer[256];
        int rc;
        int nbytes;

        // Start channel
        channel = ssh_channel_new(client);
        if (channel == nullptr)
        {
            return SSH_ERROR;
        }

        rc = ssh_channel_open_session(channel);
        if (rc != SSH_OK)
        {
            ssh_channel_free(channel);
            return rc;
        }

        // Command
        rc = ssh_channel_request_exec(channel, cmd.c_str());
        if (rc != SSH_OK)
        {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return rc;
        }

        // Process output
        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0)
        {
            write(1, buffer, nbytes);
        }

        if (nbytes < 0)
        {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return SSH_ERROR;
        }

        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 1)) > 0)
        {
            write(2, buffer, nbytes);
        }

        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);

        return SSH_OK;
    }

    int SSH::remoteConnectTools::writeChannel(const std::string& cmd)
    {
        if (shell == nullptr)
            return SSH_ERROR;
        
        ssize_t nbytes = cmd.size();
        ssize_t nwritten;

        nwritten = ssh_channel_write(shell, cmd.c_str(), nbytes);
        if (nwritten < 0)
            return SSH_ERROR;
        
        unsigned char buffer[256];

        nbytes = ssh_channel_read_nonblocking(shell, buffer, sizeof(buffer), 0);
        if (nbytes < 0) return SSH_ERROR;
        while (nbytes > 0)
        {
            if (write(1, buffer, nbytes) != nbytes)
                return SSH_ERROR;

            nbytes = ssh_channel_read_nonblocking(shell, buffer, sizeof(buffer), 0);
            if (nbytes < 0)
                return SSH_ERROR;
        }

        return SSH_OK;
    }

    int SSH::remoteConnectTools::writeFileSQL
    (
        const std::string& file_dir,
        const std::string& dbName
    )
    {
        int rc;
        char buffer[256];
        int nbytes;

        std::ifstream file(file_dir);

        ssh_channel channel = ssh_channel_new(client);
        if (channel == nullptr)
            return SSH_ERROR;
        
        if (ssh_channel_open_session(channel) != SSH_OK)
        {
            ssh_channel_free(channel);
            return SSH_ERROR;
        }

        std::string query = "sudo mysql " + dbName;

        if (ssh_channel_request_exec(channel, query.c_str()) != SSH_OK)
        {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return SSH_ERROR;
        }
    
        std::string line;
        while (std::getline(file, line))
        {
            line += '\n';

            if (ssh_channel_write(channel, line.c_str(), line.size()) == SSH_ERROR)
            {
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                return SSH_ERROR;
            }
        }

        ssh_channel_send_eof(channel);

        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0)
        {
            write(1, buffer, nbytes);
        }

        if (nbytes < 0)
        {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return SSH_ERROR;
        }

        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 1)) > 0)
        {
            write(2, buffer, nbytes);
        }

        if (nbytes < 0)
        {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return SSH_ERROR;
        }

        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return SSH_OK;
    }

    // Port forwarding
    int SSH::remoteConnectTools::open_TCP_Tunnel
    (
        const char* remote_host,
        const int& remote_port,
        const char* source_host,
        const int& local_port,
        const std::string& user,
        const std::string& host,
        const int& port,
        const std::string& password
    )
    {
        try
        {   
            // system("ssh -fNL 33060:127.0.0.1:33060 rjsonic64@192.168.1.181 > /dev/null 2>&1");

            // Manually create MySqlx TCP socket
            int laptop_socket = -1;
            int mysqlx_socket = -1;

            if (connectClient(user, host, port, password) != SSH_OK) 
                throw std::runtime_error("Remote SSH | Session failed to connect.\n");

            laptop_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (laptop_socket < 0)
                throw std::runtime_error("Remote SSH | Socket failed to initialize.\n");

            int opt = 1;
            setsockopt(laptop_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(remote_port);

            if (bind(laptop_socket, (sockaddr*)&addr, sizeof(addr)) < 0)
                throw std::runtime_error("Remote SSH | Socket failed to bind.\n");
            
            if (listen(laptop_socket, 5) < 0)
            {
                close(laptop_socket);
                throw std::runtime_error("Remote SSH | Couldn't find any ports.\n");
            }
            
            std::cout << "Server listening on port: " << remote_port << '\n';

            mysqlx_socket = accept(laptop_socket, nullptr, nullptr);
            if (mysqlx_socket < 0)
            {
                close(laptop_socket);
                throw std::runtime_error("Remote SSH | Couldn't accept incoming connection.\n");
            }

            TCP_tunnel = ssh_channel_new(client);
            if (TCP_tunnel == nullptr)
                throw std::runtime_error("Remote SSH | Channel failed to open.\n");
            
            if (ssh_channel_open_forward(TCP_tunnel, remote_host, remote_port, source_host, local_port) < 0)
            {
                ssh_channel_free(TCP_tunnel);
                throw std::runtime_error("Remote SSH | Failed to start session.\n");
            }

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            setsockopt(mysqlx_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(mysqlx_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            std::atomic<bool> alive = true;
            
            auto ClitS = [&]()
            { 
                unsigned char buffer[16384];
                int nbytes;

                while (alive.load())
                {
                    if (!alive) break;

                    nbytes = recv(mysqlx_socket, buffer, sizeof(buffer), 0);
                    if (nbytes < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(5));
                            continue;
                        }

                        alive = false;
                        shutdown(mysqlx_socket, SHUT_RDWR);
                        ssh_channel_close(TCP_tunnel);
                        break;
                    }
                    if (nbytes == 0)
                    {
                        alive = false;
                        shutdown(mysqlx_socket, SHUT_RDWR);
                        ssh_channel_close(TCP_tunnel);
                        break;
                    }

                    int total = 0;

                    while (total < nbytes)
                    {
                        int sent = ssh_channel_write(TCP_tunnel, buffer + total, nbytes - total);
                        if (sent <= 0) return;
                        total += sent;
                    }

                    if (!alive) break;
                }
            };

            auto StCli = [&]()
            {
                unsigned char buffer[16384];
                int nbytes;

                while (alive.load())
                {
                    if (!alive) break;

                    nbytes = ssh_channel_read_nonblocking(TCP_tunnel, buffer, sizeof(buffer), 0);
                    if (nbytes == SSH_ERROR) break;

                    if (nbytes == 0)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        continue;
                    }

                    int total = 0;

                    while (total < nbytes)
                    {
                        int sent = send(mysqlx_socket, buffer + total, nbytes - total, 0);
                        if (sent <= 0) return;
                        total += sent;
                    }

                    if (!alive) break;
                }
            };

            std::thread t1(ClitS);
            std::thread t2(StCli);

            t1.join();
            t2.join();

            alive = false;

            close(laptop_socket);

            shutdown(mysqlx_socket, SHUT_RDWR);
            close(mysqlx_socket);

            ssh_channel_close(TCP_tunnel);
            ssh_channel_free(TCP_tunnel);
            
            return SSH_OK;
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return SSH_ERROR;
        }
    }

    int SSH::remoteConnectTools::close_TCP_Tunnel()
    {
        if (TCP_tunnel == nullptr)
            return SSH_ERROR;

        ssh_channel_close(TCP_tunnel);
        ssh_channel_free(TCP_tunnel);
        TCP_tunnel = nullptr;
        return SSH_OK;
    }

}
