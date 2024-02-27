#include "threadpool/threadpool.hpp"
#include "connection_handler.hpp"
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <stdexcept>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <future>
#include <vector>


class SFTPConnection : public Connection {
private:
    std::string hostname;
    int port;
    std::string username;
    std::string password;
    ssh_session sshSession = nullptr;
    sftp_session sftpSession = nullptr;
    mutable std::shared_mutex sessionMutex; // Ensure thread-safe access to SSH and SFTP sessions

    // Initializes SSH session
    void initializeSSHSession();

    // Initializes SFTP session
    void initializeSFTPSession();

public:
    SFTPConnection(const std::string& hostname, int port, const std::string& username, const std::string& password);
    virtual ~SFTPConnection();

    void connect() override;
    void disconnect() override;
    void authenticate(const std::string& password) override;
    std::string getProtocolName() const override;

    // Asynchronous file operations using ThreadPool
    std::future<void> uploadFileAsync(const std::string& localPath, const std::string& remotePath);
    std::future<void> downloadFileAsync(const std::string& remotePath, const std::string& localPath);
};

// Constructor
SFTPConnection::SFTPConnection(const std::string& hostname, int port, const std::string& username, const std::string& password)
    : hostname(hostname), port(port), username(username), password(password) {
    if (hostname.empty() || port <= 0 || username.empty() || password.empty()) {
        throw std::invalid_argument("Invalid arguments for SFTPConnection constructor");
    }
    initializeSSHSession(); // Ensure SSH session is ready upon construction
}

// Destructor
SFTPConnection::~SFTPConnection() {
    disconnect(); // Ensure resources are cleaned up properly
}

void SFTPConnection::initializeSSHSession() {
    std::lock_guard<std::shared_mutex> lock(sessionMutex);
    if (!sshSession) {
        sshSession = ssh_new();
        if (!sshSession) {
            throw std::runtime_error("Failed to create SSH session");
        }
        ssh_options_set(sshSession, SSH_OPTIONS_HOST, hostname.c_str());
        ssh_options_set(sshSession, SSH_OPTIONS_PORT, &port);
        ssh_options_set(sshSession, SSH_OPTIONS_USER, username.c_str());
    }
}

void SFTPConnection::initializeSFTPSession() {
    std::lock_guard<std::shared_mutex> lock(sessionMutex);
    if (!sftpSession) {
        sftpSession = sftp_new(sshSession);
        if (!sftpSession) {
            throw std::runtime_error("Failed to create SFTP session");
        }
        if (sftp_init(sftpSession) != SSH_OK) {
            throw std::runtime_error("Failed to initialize SFTP session: " + std::string(sftp_get_error(sftpSession)));
        }
    }
}

void SFTPConnection::connect() {
    std::lock_guard<std::shared_mutex> lock(sessionMutex);
    if (!sshSession) {
        throw std::runtime_error("SSH session is not initialized");
    }
    if (ssh_connect(sshSession) != SSH_OK) {
        throw std::runtime_error("Failed to connect: " + std::string(ssh_get_error(sshSession)));
    }
    authenticate(password);
    initializeSFTPSession();
}

void SFTPConnection::disconnect() {
    std::lock_guard<std::shared_mutex> lock(sessionMutex);
    if (sftpSession) {
        sftp_free(sftpSession);
        sftpSession = nullptr;
    }
    if (sshSession) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        sshSession = nullptr;
    }
}

void SFTPConnection::authenticate(const std::string& password) {
    if (ssh_userauth_password(sshSession, nullptr, password.c_str()) != SSH_AUTH_SUCCESS) {
        throw std::runtime_error("Authentication failed: " + std::string(ssh_get_error(sshSession)));
    }
}

std::string SFTPConnection::getProtocolName() const {
    return "SFTP";
}
#include <fcntl.h> // For O_WRONLY, O_CREAT, etc.
#include <sys/stat.h> // For S_IRWXU

// Asynchronous file upload
std::future<void> SFTPConnection::uploadFileAsync(const std::string& localPath, const std::string& remotePath) {
    return ThreadPool::submit([this, localPath, remotePath] {
        std::lock_guard<std::shared_mutex> lock(sessionMutex);
        if (!sftpSession) {
            throw std::runtime_error("SFTP session is not initialized for upload");
        }

        sftp_file file = sftp_open(sftpSession, remotePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (!file) {
            throw std::runtime_error("Unable to open remote file for upload: " + std::string(sftp_get_error(sftpSession)));
        }

        std::ifstream localFile(localPath, std::ios::binary);
        if (!localFile.is_open()) {
            sftp_close(file);
            throw std::runtime_error("Failed to open local file for upload");
        }

        char buffer[1024];
        while (localFile.read(buffer, sizeof(buffer)) || localFile.gcount() > 0) {
            if (sftp_write(file, buffer, localFile.gcount()) != localFile.gcount()) {
                sftp_close(file);
                throw std::runtime_error("Failed to write to remote file during upload: " + std::string(sftp_get_error(sftpSession)));
            }
        }

        sftp_close(file);
        });
}

// Asynchronous file download
std::future<void> SFTPConnection::downloadFileAsync(const std::string& remotePath, const std::string& localPath) {
    return ThreadPool::enqueue([this, remotePath, localPath] {
        std::lock_guard<std::shared_mutex> lock(sessionMutex);
        if (!sftpSession) {
            throw std::runtime_error("SFTP session is not initialized for download");
        }

        sftp_file file = sftp_open(sftpSession, remotePath.c_str(), O_RDONLY, 0);
        if (!file) {
            throw std::runtime_error("Unable to open remote file for download: " + std::string(sftp_get_error(sftpSession)));
        }

        std::ofstream localFile(localPath, std::ios::binary);
        if (!localFile.is_open()) {
            sftp_close(file);
            throw std::runtime_error("Failed to open local file for download");
        }

        char buffer[1024];
        int nbytes;
        while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0) {
            localFile.write(buffer, nbytes);
        }
        if (!localFile.good()) {
            sftp_close(file);
            throw std::runtime_error("Failed to write to local file during download");
        }

        sftp_close(file);
        });
}
