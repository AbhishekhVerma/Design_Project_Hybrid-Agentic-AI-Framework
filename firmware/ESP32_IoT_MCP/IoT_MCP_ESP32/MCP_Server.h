#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>

class MCPServer {
public:
    MCPServer();
    
    // Parses incoming AES-encrypted JSON-RPC payload
    String process_rpc_call(uint8_t* payload, size_t length);

    // Prints current Heap and PSRAM memory to Serial for benchmarking
    void print_memory_usage();

    // Runs the Layer 3 TinyML C++ Decision Tree if EdgeShard disconnects
    void run_tinyml_fallback();

private:
    // Core tool execution logic
    String execute_tool(String tool_name, JsonObject parameters);
    
    // Tools
    String tool_inject_medication(JsonObject parameters);
    String tool_toggle_led(JsonObject parameters);
    String tool_read_sensor(JsonObject parameters);
};

#endif // MCP_SERVER_H
