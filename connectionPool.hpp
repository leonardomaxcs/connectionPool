
#include "threadpool/threadpool.hpp"
#include "protocols/connection_handler.hpp"


class ConTaskInfo {
public:
    std::optional<std::string> errorMessage = std::nullopt;
    std::atomic<bool> isCompleted = false;

    ConTaskInfo() = default;
    ConTaskInfo(const ConTaskInfo& other)
        : errorMessage(other.errorMessage), isCompleted(other.isCompleted.load()) {}
    ConTaskInfo& operator=(const ConTaskInfo& other) {
        errorMessage = other.errorMessage;
        isCompleted.store(other.isCompleted.load());
        return *this;
    }
    ConTaskInfo(ConTaskInfo&&) = default;
    ConTaskInfo& operator=(ConTaskInfo&&) = default;
};

//class ConTaskInfo {
//public:
//    std::optional<std::string> errorMessage = std::nullopt;
//    std::atomic<bool> isCompleted = false;
//
//    void markAsCompleted() {
//        isCompleted.store(true, std::memory_order_release);
//    }
//};

class ConnectionInfoTask {
public:
    std::string protocolName;
    std::size_t taskId;
    std::atomic<bool> isCompleted = false;
    ConTaskInfo taskInfo;
};

class ConnectionPool {
private:
    ThreadPool& thread_pool;
    std::atomic<std::size_t> connectionIdCounter = 0;
    mutable std::shared_mutex registryMutex;
    std::unordered_map<std::size_t, ConnectionInfoTask> connectionRegistry;

    // Construtor privado para impedir instâncias fora do getInstance()
    ConnectionPool() : thread_pool(ThreadPool::getInstance()) {}

public:
    static ConnectionPool& getInstance() {
        static ConnectionPool instance; // Singleton pattern
        return instance;
    }

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ~ConnectionPool() = default;

    template<typename F, typename... Args>
    auto enqueue(std::shared_ptr<Connection> connection, F&& f, Args&&... args) {
        static_assert(std::is_base_of<Connection, typename std::remove_reference<decltype(*connection)>::type>::value,
            "Connection must be a base of the passed object");

        using ReturnType = std::invoke_result_t<F, Args...>;
        auto taskWrapper = [this, connection, f = std::forward<F>(f), capturedArgs = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            auto currentTaskId = connectionIdCounter.fetch_add(1, std::memory_order_relaxed);
            recordConnection(currentTaskId, connection); // Registra a conexão antes de executar

            try {
                if constexpr (!std::is_same_v<ReturnType, void>) {
                    ReturnType result = std::apply(f, capturedArgs);
                    markTaskAsCompleted(currentTaskId);
                    return result;
                }
                else {
                    std::apply(f, capturedArgs);
                    markTaskAsCompleted(currentTaskId);
                }
            }
            catch (const std::exception& e) {
                recordTaskError(currentTaskId, e.what());
                if constexpr (!std::is_same_v<ReturnType, void>) {
                    throw; // Re-throw para o chamador tratar
                }
            }
            };

        // Enfileira a tarefa no ThreadPool
        return thread_pool.enqueue(std::move(taskWrapper));
    }

    bool isTaskCompleted(std::size_t taskId) {
        std::shared_lock<std::shared_mutex> lock(registryMutex);
        auto it = connectionRegistry.find(taskId);
        if (it == connectionRegistry.end()) {
            throw std::runtime_error("Task not found");
        }
        return it->second.isCompleted.load(std::memory_order_acquire);
    }

    std::optional<std::string> getTaskErrorMessage(std::size_t taskId) {
        std::shared_lock<std::shared_mutex> lock(registryMutex);
        auto it = connectionRegistry.find(taskId);
        if (it == connectionRegistry.end()) {
            throw std::runtime_error("Task not found");
        }
        return it->second.taskInfo.errorMessage;
    }

    // Método para recuperar detalhadamente o estado de uma tarefa
    std::optional<ConTaskInfo> getDetailedTaskInfo(std::size_t taskId) {
        std::shared_lock<std::shared_mutex> lock(registryMutex);
        auto it = connectionRegistry.find(taskId);
        if (it != connectionRegistry.end()) {
            return it->second.taskInfo;
        }
        return std::nullopt; // Retorna nullopt se a tarefa não for encontrada
    }

private:
    // Método aprimorado para registrar uma nova tarefa e sua conexão associada
    void recordConnection(std::size_t currentTaskId, std::shared_ptr<Connection> connection) {
        std::unique_lock<std::shared_mutex> lock(registryMutex);
        // Emplace garante que um novo ConnectionInfoTask seja construído diretamente no mapa
      
      // QUEBRANDO O CODIGO
      //   auto& task = connectionRegistry.emplace(currentTaskId, ConnectionInfoTask{ connection->getProtocolName(), currentTaskId, false, {} }).first->second;
      // task.protocolName = connection->getProtocolName();
        
      // Aquela gambiarra salvadora
        std::unordered_map<std::size_t, ConnectionInfoTask> connectionRegistry;
        ConnectionInfoTask newConnectionInfo{ connection->getProtocolName(), currentTaskId, false, {} };
        newConnectionInfo.protocolName = connection->getProtocolName();
        //TODO: Inicializações adicionais, se necessário
    }

        void markTaskAsCompleted(std::size_t taskId) {
        std::unique_lock<std::shared_mutex> lock(registryMutex);
        auto it = connectionRegistry.find(taskId);
        if (it != connectionRegistry.end()) {
            it->second.isCompleted.store(true, std::memory_order_release);
        }
    }

    void recordTaskError(std::size_t taskId, const std::string& errorMessage) {
        std::unique_lock<std::shared_mutex> lock(registryMutex);
        auto it = connectionRegistry.find(taskId);
        if (it != connectionRegistry.end()) {
            it->second.taskInfo.errorMessage = errorMessage;
        }
    }

};
