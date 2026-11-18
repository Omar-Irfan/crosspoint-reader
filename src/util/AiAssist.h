#pragma once

#include <cstdint>
#include <string>

// Optional "Ask AI" help for dictionary lookups: sends the selected word or
// phrase and its sentence (the matn) to the Claude or Gemini API and returns
// the short explanation that goes under the matn. Off unless
// /.crosspoint/ai.json holds an API key:
//   {"api_key": "...", "provider": "anthropic"|"gemini", "model": "...", "explain": "arabic"|"english"}
// Only api_key is required.
namespace AiAssist {

enum class Result : uint8_t {
  Ok,
  NotConfigured,
  NoClock,      // TLS certificate checks need a trusted date
  Network,      // connect, TLS or transport failure
  KeyRejected,  // HTTP 401/403
  Refused,      // stop_reason "refusal"
  BadResponse,  // other HTTP status, or a body without text
  LowMemory,
};

// True when the config file exists and names a key. Reads the SD card once.
bool isConfigured();

// Language of the explanation.
enum class Explain : uint8_t { Arabic, English };

// The configured first language ("explain" in ai.json; Arabic unless "english").
Explain defaultExplain();

// Blocking request; WiFi must already be connected. On Ok, answerOut holds
// plain text, one line per paragraph, for the definition viewer.
Result ask(const std::string& text, const std::string& context, Explain explain, std::string& answerOut);

}  // namespace AiAssist
