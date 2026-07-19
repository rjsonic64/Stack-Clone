#pragma once

// External libraries
#include <libssh/libssh.h>
#include <stdlib.h>

// Containers
#include <optional>

// I/O
#include <iostream>



namespace SSH
{
    class remoteConnectTools
    {
    private:

        // Session
        ssh_session client = nullptr;

        // Channels
        ssh_channel TCP_tunnel = nullptr;
        ssh_channel shell = nullptr;
    
    public:
    
        // Open connections
        int connectClient(const std::string& user,
                          const std::string& host,
                          const std::optional<int>& port,
                          const std::string&);
        int open_interactive_terminal();

        // Authentication
        int authenticateSession();
        int authenticateUser(const std::string&);

        // Disconnect
        int disconnectClient();

        // Run Commands
        int execSingleChannel(const std::string&);
        int writeChannel(const std::string&);

        int writeFileSQL(const std::string&, const std::string&);

        // Port Forwarding
        int open_TCP_Tunnel
        (
            const char*,
            const int&,
            const char*,
            const int&,
            const std::string& user,
            const std::string& host,
            const int& port,
            const std::string&
        );

        int close_TCP_Tunnel();

    };
}