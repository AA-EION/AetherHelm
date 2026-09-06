#include "aetherhelm_mcp_server.h"
#include <iostream>

int main(int argc, char* argv[]) {
  // Initialize JUCE message manager / core runtime for headless operation
  juce::ScopedJuceInitialiser_GUI juceInit;

  if (argc > 1) {
    std::string arg = argv[1];
    if (arg == "--install" || arg == "-i") {
      std::cout << "Installing AetherHelm MCP Server into desktop AI configurations...\n";
      bool ok = AetherHelmMcpServer::installDesktopConfigs(true, true, true, true);
      return ok ? 0 : 1;
    } else if (arg == "--install-claude") {
      std::cout << "Installing into Claude Desktop...\n";
      return AetherHelmMcpServer::installDesktopConfigs(true, false, false, false) ? 0 : 1;
    } else if (arg == "--install-cursor") {
      std::cout << "Installing into Cursor...\n";
      return AetherHelmMcpServer::installDesktopConfigs(false, true, false, false) ? 0 : 1;
    } else if (arg == "--install-windsurf") {
      std::cout << "Installing into Windsurf...\n";
      return AetherHelmMcpServer::installDesktopConfigs(false, false, true, false) ? 0 : 1;
    } else if (arg == "--install-cline") {
      std::cout << "Installing into Cline / Roo-Code...\n";
      return AetherHelmMcpServer::installDesktopConfigs(false, false, false, true) ? 0 : 1;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "AetherHelm MCP Server v1.0.0\n"
                << "Usage: aetherhelm-mcp [OPTIONS]\n\n"
                << "Options:\n"
                << "  (no arguments)     Run MCP server in stdio mode (JSON-RPC 2.0)\n"
                << "  --install, -i      Auto-configure all supported desktop AI tools\n"
                << "  --install-claude   Configure Claude Desktop config\n"
                << "  --install-cursor   Configure Cursor mcp.json\n"
                << "  --install-windsurf Configure Windsurf mcp_config.json\n"
                << "  --install-cline    Configure Cline / Roo-Code config\n"
                << "  --help, -h         Display this help message\n";
      return 0;
    }
  }

  AetherHelmMcpServer server;
  server.runStdioLoop();
  return 0;
}
