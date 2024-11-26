#pragma once

#include "Common.h"

namespace Mach1 {

class ServiceLocator {
public:
    static ServiceLocator& getInstance() {
        static ServiceLocator instance;
        return instance;
    }

    template<typename T>
    void registerService(std::shared_ptr<T> service) {
        services[typeid(T).name()] = service;
    }

    template<typename T>
    std::shared_ptr<T> getService() {
        auto it = services.find(typeid(T).name());
        return it != services.end() ? std::static_pointer_cast<T>(it->second) : nullptr;
    }

private:
    ServiceLocator() = default;
    std::unordered_map<std::string, std::shared_ptr<void>> services;
    
    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;
};

} // namespace Mach1