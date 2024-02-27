#pragma once

#include "connection_handler.hpp"


class SSHConnection : public Connection {
private:
    std::string hostname;
    int port;
    std::string username;
    ssh_session sshSession = nullptr;
    std::mutex sessionMutex; // Para garantir thread-safety nas operações

    void initializeSession() {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (!sshSession) {
            sshSession = ssh_new();
            if (!sshSession) {
                throw std::runtime_error("Failed to create SSH session");
            }
        }
    }

public:
    SSHConnection(const std::string& hostname, int port, const std::string& username)
        : hostname(hostname), port(port), username(username) {
        if (hostname.empty() || port <= 0 || username.empty()) {
            throw std::invalid_argument("Invalid arguments for SSHConnection");
        }
        initializeSession();
    }

    ~SSHConnection() override {
        disconnect(); // Garante que a sessão seja desconectada e liberada corretamente
    }

    void connect() override {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (!sshSession) {
            throw std::runtime_error("SSH session is not initialized");
        }
        ssh_options_set(sshSession, SSH_OPTIONS_HOST, hostname.c_str());
        ssh_options_set(sshSession, SSH_OPTIONS_PORT, &port);
        ssh_options_set(sshSession, SSH_OPTIONS_USER, username.c_str());

        if (ssh_connect(sshSession) != SSH_OK) {
            throw std::runtime_error("Failed to connect: " + std::string(ssh_get_error(sshSession)));
        }
    }

    void disconnect() override {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (sshSession) {
            ssh_disconnect(sshSession);
            ssh_free(sshSession);
            sshSession = nullptr; // Evita uso posterior de uma sessão liberada
        }
    }

    std::string getProtocolName() const override {
        return "SSH";
    }

    void authenticate(const std::string& password) override {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (!sshSession) {
            throw std::runtime_error("SSH session is not initialized");
        }

        if (ssh_userauth_password(sshSession, nullptr, password.c_str()) != SSH_AUTH_SUCCESS) {
            throw std::runtime_error("Authentication failed: " + std::string(ssh_get_error(sshSession)));
        }
    }
};
    // IMPLEMENTAR MÉTODO PARA EXECUTAR COMANDO REMOTO
//   void executeRemoteCommand(const std::string& hostname, int port, const std::string& username, const std::string& password, const std::string& command) {
//       auto sshConnection = std::make_shared<SSHConnection>(hostname, port, username);
//       sshConnection->connect();
//       sshConnection->authenticate(password);
//       auto result = sshConnection->executeCommand(command);
//       std::cout << "Resultado do comando: " << result << std::endl;
//       sshConnection->disconnect();
//   }

