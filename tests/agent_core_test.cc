// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "core/agent_core.h"
#include "core/agent_session.h"
#include "core/agent_role.h"
#include "tools/tool_registry.h"
#include "providers/provider_router.h"

namespace prosophor {

class AgentCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize ToolRegistry singleton
        auto& tool_registry = ToolRegistry::GetInstance();
        tool_registry.RegisterBuiltinTools();

        // Create tool executor callback
        tool_executor_ =
            [&tool_registry](const std::string& name, const nlohmann::json& args) -> std::string {
                return tool_registry.ExecuteTool(name, args);
            };

        // Create a test session
        session_ = std::make_unique<AgentSession>();
        session_->session_id = "test-session-001";
        session_->role_id = "coder";
        session_->task_description = "Test task";
        session_->use_tools = true;
        session_->is_active = true;

        // Create a mock role
        mock_role_ = std::make_unique<AgentRole>();
        mock_role_->id = "coder";
        mock_role_->name = "Coder";
        mock_role_->model = "claude-sonnet-4-6";
        mock_role_->temperature = 0.7;
        mock_role_->max_tokens = 8192;
        mock_role_->max_iterations = 5;  // Low for testing

        session_->role = mock_role_.get();
    }

    void TearDown() override {
        session_.reset();
        mock_role_.reset();
    }

    ToolExecutorCallback tool_executor_;
    std::unique_ptr<AgentSession> session_;
    std::unique_ptr<AgentRole> mock_role_;
};

// Test static method availability
TEST_F(AgentCoreTest, StaticMethodsAvailable) {
    // Verify that Loop is callable (compile-time check)
    // We don't actually run it here to avoid LLM calls
    EXPECT_TRUE(true);  // Placeholder - actual test is in integration tests
}

// Test session initialization
TEST_F(AgentCoreTest, SessionInitialization) {
    EXPECT_EQ(session_->session_id, "test-session-001");
    EXPECT_EQ(session_->role_id, "coder");
    EXPECT_EQ(session_->task_description, "Test task");
    EXPECT_TRUE(session_->use_tools);
    EXPECT_TRUE(session_->is_active);
    EXPECT_NE(session_->role, nullptr);
}

// Test message addition
TEST_F(AgentCoreTest, AddUserMessage) {
    session_->messages.emplace_back("user", "Hello, Agent!");

    EXPECT_EQ(session_->messages.size(), 1u);
    EXPECT_EQ(session_->messages[0].role, "user");
    EXPECT_EQ(session_->messages[0].text(), "Hello, Agent!");
}

// Test message content building
TEST_F(AgentCoreTest, MessageContentBuilding) {
    MessageSchema msg;
    msg.role = "assistant";

    msg.AddTextContent("Hello!");
    msg.AddToolUseContent("tool_1", "read_file", {{"path", "/test.txt"}});
    msg.AddToolResultContent("tool_1", "File content here");

    EXPECT_EQ(msg.content.size(), 3u);
    EXPECT_EQ(msg.content[0].type, "text");
    EXPECT_EQ(msg.content[1].type, "tool_use");
    EXPECT_EQ(msg.content[2].type, "tool_result");
}

// Test text extraction from message
TEST_F(AgentCoreTest, MessageTextExtraction) {
    MessageSchema msg;
    msg.role = "assistant";

    msg.AddTextContent("First message. ");
    msg.AddTextContent("Second message.");

    std::string text = msg.text();
    EXPECT_EQ(text, "First message. Second message.");
}

// Test empty session messages
TEST_F(AgentCoreTest, EmptySessionMessages) {
    EXPECT_TRUE(session_->messages.empty());
    EXPECT_EQ(session_->messages.size(), 0u);
}

// Test system prompt building
TEST_F(AgentCoreTest, SystemPromptBuilding) {
    session_->system_prompt.push_back({"text", "You are a helpful assistant.", false});
    session_->system_prompt.push_back({"text", "You are an AI coding assistant.", true});

    EXPECT_EQ(session_->system_prompt.size(), 2u);
    EXPECT_EQ(session_->system_prompt[0].text, "You are a helpful assistant.");
    EXPECT_TRUE(session_->system_prompt[0].cache_control == false);
    EXPECT_TRUE(session_->system_prompt[1].cache_control == true);
}

// Test role configuration propagation
TEST_F(AgentCoreTest, RoleConfigPropagation) {
    EXPECT_EQ(session_->role->model, "claude-sonnet-4-6");
    EXPECT_EQ(session_->role->temperature, 0.7);
    EXPECT_EQ(session_->role->max_tokens, 8192);
    EXPECT_EQ(session_->role->max_iterations, 5);
}

// Test working directory setting
TEST_F(AgentCoreTest, WorkingDirectorySetting) {
    std::string test_dir = "/tmp/test_workdir";
    session_->working_directory = test_dir;
    EXPECT_EQ(session_->working_directory, test_dir);
}

// Test related files
TEST_F(AgentCoreTest, RelatedFiles) {
    session_->related_files.push_back("/path/to/file1.cpp");
    session_->related_files.push_back("/path/to/file2.h");

    EXPECT_EQ(session_->related_files.size(), 2u);
    EXPECT_EQ(session_->related_files[0], "/path/to/file1.cpp");
    EXPECT_EQ(session_->related_files[1], "/path/to/file2.h");
}

// Test message with special characters
TEST_F(AgentCoreTest, MessageWithSpecialCharacters) {
    std::string special_text = "Hello with special chars: <>&\"'\n\t";
    session_->messages.emplace_back("user", special_text);

    EXPECT_EQ(session_->messages[0].text(), special_text);
}

// Test multiline message
TEST_F(AgentCoreTest, MultilineMessage) {
    std::string multiline = "Line 1\nLine 2\nLine 3";
    session_->messages.emplace_back("user", multiline);

    EXPECT_EQ(session_->messages[0].text(), multiline);
}

// Test tool use in message
TEST_F(AgentCoreTest, ToolUseMessage) {
    MessageSchema msg;
    msg.role = "assistant";
    msg.AddTextContent("I'll read the file for you.");
    msg.AddToolUseContent("call_123", "read_file", {{"path", "/test.txt"}});

    EXPECT_EQ(msg.content.size(), 2u);
    EXPECT_EQ(msg.content[1].tool_use_id, "call_123");
    EXPECT_EQ(msg.content[1].name, "read_file");
    EXPECT_EQ(msg.content[1].input["path"], "/test.txt");
}

// Test tool result in message
TEST_F(AgentCoreTest, ToolResultMessage) {
    MessageSchema msg;
    msg.role = "user";
    msg.AddToolResultContent("call_123", "File contents here", false);

    EXPECT_EQ(msg.content.size(), 1u);
    EXPECT_EQ(msg.content[0].type, "tool_result");
    EXPECT_EQ(msg.content[0].tool_use_id, "call_123");
    EXPECT_EQ(msg.content[0].content, "File contents here");
    EXPECT_FALSE(msg.content[0].is_error);
}

// Test tool error result
TEST_F(AgentCoreTest, ToolErrorMessage) {
    MessageSchema msg;
    msg.role = "user";
    msg.AddToolResultContent("call_456", "Error: File not found", true);

    EXPECT_TRUE(msg.content[0].is_error);
}

// Test session state
TEST_F(AgentCoreTest, SessionState) {
    EXPECT_TRUE(session_->is_active);
    session_->is_active = false;
    EXPECT_FALSE(session_->is_active);
}

// Test provider assignment (mock)
TEST_F(AgentCoreTest, ProviderAssignment) {
    // Provider would be set by ProviderRouter in real usage
    // This test verifies the field exists and can be set
    EXPECT_TRUE(session_->provider == nullptr);  // Initially null

    // In real code, provider would be set like:
    // auto& router = ProviderRouter::GetInstance();
    // session_->provider = router.GetProviderByName("anthropic");
}

// Test use_tools flag
TEST_F(AgentCoreTest, UseToolsFlag) {
    EXPECT_TRUE(session_->use_tools);
    session_->use_tools = false;
    EXPECT_FALSE(session_->use_tools);
}

// Test thinking content
TEST_F(AgentCoreTest, ThinkingContent) {
    MessageSchema msg;
    msg.role = "assistant";
    msg.AddThinkingContent("Let me think about this step by step...");

    EXPECT_EQ(msg.content.size(), 1u);
    EXPECT_EQ(msg.content[0].type, "thinking");
    EXPECT_EQ(msg.content[0].text, "Let me think about this step by step...");
}

// Test mixed content types
TEST_F(AgentCoreTest, MixedContentTypes) {
    MessageSchema msg;
    msg.role = "assistant";

    msg.AddThinkingContent("Thinking...");
    msg.AddTextContent("Response text");
    msg.AddToolUseContent("tool_1", "bash", {{"command", "ls -la"}});

    auto text = msg.text();
    // text() should include thinking and text content
    EXPECT_NE(text.find("Thinking..."), std::string::npos);
    EXPECT_NE(text.find("Response text"), std::string::npos);
}

// Test empty message creation
TEST_F(AgentCoreTest, EmptyMessageCreation) {
    MessageSchema empty_msg;
    EXPECT_TRUE(empty_msg.content.empty());
    EXPECT_TRUE(empty_msg.role.empty());
}

// Test message with role constructor
TEST_F(AgentCoreTest, MessageWithRoleConstructor) {
    MessageSchema msg("user", "Test content");
    EXPECT_EQ(msg.role, "user");
    EXPECT_EQ(msg.text(), "Test content");
    EXPECT_EQ(msg.content.size(), 1u);
}

// Test empty text message
TEST_F(AgentCoreTest, EmptyTextMessage) {
    MessageSchema msg("assistant", "");
    EXPECT_EQ(msg.role, "assistant");
    EXPECT_TRUE(msg.text().empty());
    EXPECT_TRUE(msg.content.empty());  // Empty string should not add content
}

}  // namespace prosophor
