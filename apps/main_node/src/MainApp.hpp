#pragma once

#include <deos/deos.h>

class MainApp {
public:
    static MainApp& getInstance();
    
    int init();
    void run();

private:
    MainApp() = default;
    ~MainApp() = default;

    // Non-copyable
    MainApp(const MainApp&) = delete;
    MainApp& operator=(const MainApp&) = delete;

    static deos_transport_t routing_policy(deos_node_id_t destination);
};
