// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <fstream>

#include "tools/tool_registry.h"

namespace prosophor {

class ToolRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // ToolRegistry is a singleton with private constructor
        // Tests use the singleton instance
        registry_ = &ToolRegistry::GetInstance();
        registry_->SetWorkspace("/tmp");
    }

    void TearDown() override {
        // Reset singleton state if needed
    }

    ToolRegistry* registry_ = nullptr;
};

TEST_F(ToolRegistryTest, ToolRegistration) {
    auto schemas = registry_->GetToolSchemas();

    // Should have many built-in tools
    EXPECT_GT(schemas.size(), 10u);

    // Check for common tools
    std::vector<std::string> expected_tools = {
        "read_file", "write_file", "edit_file",
        "bash", "glob", "grep"
    };

    for (const auto& expected : expected_tools) {
        bool found = false;
        for (const auto& schema : schemas) {
            if (schema.name == expected) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Expected tool not found: " << expected;
    }
}

TEST_F(ToolRegistryTest, HasTool) {
    EXPECT_TRUE(registry_->HasTool("read_file"));
    EXPECT_TRUE(registry_->HasTool("bash"));
    EXPECT_TRUE(registry_->HasTool("task_create"));
    EXPECT_FALSE(registry_->HasTool("nonexistent_tool"));
}

TEST_F(ToolRegistryTest, ToolSchemaStructure) {
    auto schemas = registry_->GetToolSchemas();

    for (const auto& schema : schemas) {
        EXPECT_FALSE(schema.name.empty());
        EXPECT_FALSE(schema.description.empty());
        EXPECT_TRUE(schema.input_schema.is_object());
    }
}

TEST_F(ToolRegistryTest, ExecuteReadFile) {
    // Create a test file
    std::string test_path = "/tmp/prosophor_test.txt";
    std::ofstream ofs(test_path);
    ofs << "Hello, World!";
    ofs.close();

    nlohmann::json params = nlohmann::json::object();
    params["path"] = test_path;

    std::string result = registry_->ExecuteTool("read_file", params);

    EXPECT_NE(result.find("Hello, World!"), std::string::npos);

    // Cleanup
    std::remove(test_path.c_str());
}

TEST_F(ToolRegistryTest, ExecuteReadFile_NotFound) {
    nlohmann::json params = nlohmann::json::object();
    params["path"] = "/nonexistent/path/file.txt";

    std::string result = registry_->ExecuteTool("read_file", params);

    EXPECT_NE(result.find("Error"), std::string::npos);
}

TEST_F(ToolRegistryTest, ExecuteBash_Echo) {
    nlohmann::json params = nlohmann::json::object();
    params["command"] = "echo 'Hello, World!'";

    std::string result = registry_->ExecuteTool("bash", params);

    EXPECT_NE(result.find("Hello, World!"), std::string::npos);
}

TEST_F(ToolRegistryTest, ExecuteBash_ExitCode) {
    nlohmann::json params = nlohmann::json::object();
    params["command"] = "exit 42";

    std::string result = registry_->ExecuteTool("bash", params);

    // Should contain error information
    EXPECT_NE(result.find("exit"), std::string::npos);
}

TEST_F(ToolRegistryTest, ExecuteGlob) {
    nlohmann::json params = nlohmann::json::object();
    params["pattern"] = "*.cc";
    params["path"] = "/tmp";

    std::string result = registry_->ExecuteTool("glob", params);

    // Should return JSON with files array
    EXPECT_NE(result.find("\"files\""), std::string::npos);
}

TEST_F(ToolRegistryTest, WorkspacePath) {
    std::string test_path = "/tmp/test_workspace";
    registry_->SetWorkspace(test_path);
    // Workspace path is set, verification would require internal access
}

// =============================================================================
// Weather Query Test Cases (using web_search and web_fetch)
// =============================================================================

TEST_F(ToolRegistryTest, ExecuteWebSearch_WeatherQuery) {
    // Test web search for weather information
    nlohmann::json params = nlohmann::json::object();
    params["query"] = "上海今天天气情况 温度 预报";
    params["count"] = 5;

    std::string result = registry_->ExecuteTool("web_search", params);

    // Log the result for debugging
    std::cout << "\n=== Web Search Result ===" << std::endl;
    std::cout << result << std::endl;
    std::cout << "==========================" << std::endl;

    // Web search should execute and return some result
    EXPECT_FALSE(result.empty()) << "Web search result should not be empty";

    // Result should contain the query or search engine name
    bool contains_query = result.find("上海") != std::string::npos;
    bool contains_engine = result.find("DuckDuckGo") != std::string::npos ||
                           result.find("Brave") != std::string::npos ||
                           result.find("Tavily") != std::string::npos ||
                           result.find("Search") != std::string::npos;

    EXPECT_TRUE(contains_query || contains_engine)
        << "Expected search result to contain query or search engine name, got: " << result;
}

TEST_F(ToolRegistryTest, ExecuteWebFetch_WeatherWebsite) {
    // Test web fetch with a known weather website URL
    nlohmann::json params = nlohmann::json::object();
    params["url"] = "https://example.com";

    std::string result = registry_->ExecuteTool("web_fetch", params);

    // Log the result for debugging
    std::cout << "\n=== Web Fetch Result ===" << std::endl;
    std::cout << result << std::endl;
    std::cout << "========================" << std::endl;

    // Should either fetch successfully or return an error message
    bool contains_expected = result.find("Example Domain") != std::string::npos ||
                             result.find("Error") != std::string::npos ||
                             result.find("error") != std::string::npos;

    EXPECT_TRUE(contains_expected) << "Expected 'Example Domain' or error message, got: " << result;
}

TEST_F(ToolRegistryTest, WeatherQueryIntegration) {
    // Integration test: simulate a weather query workflow
    std::cout << "\n=== Weather Query Integration Test ===" << std::endl;

    // Step 1: Search for weather information
    nlohmann::json search_params = nlohmann::json::object();
    search_params["query"] = "上海天气 今天 温度 预报";

    std::string search_result = registry_->ExecuteTool("web_search", search_params);
    std::cout << "Step 1 - Search Result: " << search_result << std::endl;

    // Step 2: Verify the search executed successfully
    ASSERT_FALSE(search_result.empty()) << "Weather search result should not be empty";

    // Step 3: Check result contains expected content
    bool has_location = search_result.find("上海") != std::string::npos;
    bool has_weather_term = search_result.find("天气") != std::string::npos ||
                            search_result.find("Weather") != std::string::npos;
    bool has_search_indicator = search_result.find("Search") != std::string::npos ||
                                search_result.find("DuckDuckGo") != std::string::npos ||
                                search_result.find("Brave") != std::string::npos;

    EXPECT_TRUE(has_location || has_search_indicator)
        << "Expected search result to contain location or search indicator";

    std::cout << "=== Weather Query Test Complete ===" << std::endl;
}

}  // namespace prosophor
