#include "AiAssist.h"

#include <ArduinoJson.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <SecureHttpClient.h>
#include <TrustedTime.h>

#include <cstring>

#include "util/TaskWatchdog.h"

namespace AiAssist {
namespace {

constexpr const char* CONFIG_PATH = "/.crosspoint/ai.json";
constexpr size_t MAX_CONFIG_BYTES = 1024;
constexpr const char* ANTHROPIC_URL = "https://api.anthropic.com/v1/messages";
constexpr const char* ANTHROPIC_DEFAULT_MODEL = "claude-sonnet-5-5";
constexpr const char* GEMINI_URL_PREFIX = "https://generativelanguage.googleapis.com/v1beta/models/";
constexpr const char* GEMINI_DEFAULT_MODEL = "gemini-3.8-flash";
constexpr uint32_t REQUEST_TIMEOUT_MS = 20000;
// Room for a short reply plus the little thinking low effort uses.
constexpr int MAX_TOKENS = 900;
// The response body is streamed into one buffer of at most this size; a
// larger body is abandoned rather than grown.
constexpr size_t MAX_RESPONSE_BYTES = 8 * 1024;
constexpr size_t MAX_ANSWER_BYTES = 4096;
// TLS handshake needs (as KOReaderSyncClient), plus the response buffer and
// its parsed copy; the request body is added per request. Checked against
// internal RAM, which is conservative where wolfSSL may also use PSRAM.
constexpr size_t MIN_FREE_FOR_REQUEST = 35000 + 2 * MAX_RESPONSE_BYTES;
// The response buffer is reserved before the handshake, so the largest block
// must hold both.
constexpr size_t MIN_BLOCK_FOR_REQUEST = 20000 + MAX_RESPONSE_BYTES;

// Roots for api.anthropic.com and generativelanguage.googleapis.com (both
// Google Trust Services): GTS Root R4, and
// GlobalSign Root CA, which cross-signs it.
constexpr const char* ROOT_CAS =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
    "VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
    "A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
    "WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
    "IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
    "AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
    "QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
    "HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
    "BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
    "9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
    "p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
    "-----END CERTIFICATE-----\n"
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\n"
    "A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\n"
    "b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\n"
    "MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\n"
    "YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\n"
    "aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\n"
    "jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\n"
    "xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\n"
    "1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\n"
    "snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\n"
    "U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\n"
    "9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\n"
    "BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\n"
    "AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\n"
    "yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\n"
    "38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\n"
    "AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\n"
    "DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\n"
    "HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\n"
    "-----END CERTIFICATE-----\n";

constexpr const char* PROMPT_ENGLISH =
    "You help a Muslim learner of Arabic reading a classical or Islamic book on a small e-ink screen. "
    "They know the religion's basics, common Islamic terms and everyday Arabic; what they lack is the "
    "harder vocabulary and how the sentence fits together. You get a word, phrase or sentence they selected and, "
    "as background only, the text around it (the matn): use it to understand the selection, but "
    "explain only the selection. The selection is already shown above your reply. Write what goes under "
    "it, in plain text, no Markdown, two short lines of about 50 words in all, each starting with its "
    "label:\n"
    "Translation: a close, literal-leaning English translation of the whole selection (a word, phrase or "
    "sentence), as it is used in this passage.\n"
    "Explanation: the point of the selection and any nuance a learner could miss, without repeating the "
    "translation. Never explain what such a reader already knows (who the Prophet "
    "is, what salah is) or restate the translation.\n"
    "Rely on the text provided and what is reliably known; never attribute a view to the author that the "
    "text does not show (it may quote someone else), and say \"I am not sure\" rather than guess. "
    "Do not repeat the passage. No grammatical analysis (i'rab), no notes. Write honorifics as their "
    "symbols (ﷺ, ﷻ).";

constexpr const char* PROMPT_ARABIC =
    "أنت تساعد متعلمًا مسلمًا للعربية يقرأ كتابًا تراثيًا أو شرعيًا على شاشة حبر إلكتروني صغيرة. هو يعرف "
    "أساسيات الدين والمصطلحات الشائعة والعربية المألوفة، وينقصه الغريب من الألفاظ. تصلك كلمة أو عبارة أو جملة "
    "اختارها، ومعها ما حولها من المتن للاستئناس به في فهم المختار فقط، فلا تشرح غير المختار. والمختار "
    "معروض فوق جوابك. اكتب ما تحته فقط، بالعربية الفصحى، بنص عادي "
    "دون تنسيق، في سطرين، كل سطر نحو ثلاثين كلمة:\n"
    "المختار: معنى المختار كاملًا، كلمةً كان أو عبارةً أو جملةً، في هذا السياق تحديدًا، مع جذر الغريب منه إن أفاد.\n"
    "المقصود: مؤدى المختار وما قد يفوت المتعلم من دقائقه، دون تكرار السطر الأول.\n"
    "اعتمد على النص المقدَّم وما هو معلوم يقينًا من اللغة؛ ولا تنسب إلى المؤلف قولًا أو رأيًا لا يدل "
    "عليه النص، فقد يكون المتن حكايةً لقول غيره. وإن لم تتيقن من معنى فقل «لا أجزم» ولا تخمّن. لا تشرح ما "
    "يعرفه مثله (من هو رسول الله ﷺ، ما الصلاة)، ولا تعِد المتن. لا إعراب ولا ملاحظات نحوية. اكتب صيغ "
    "التعظيم برموزها (ﷺ، ﷻ).";

// Anthropic models that accept server-side refusal fallbacks; others (e.g.
// Haiku) reject the parameter, so it is only sent to these.
bool supportsFallbacks(const std::string& model) {
  return model == "claude-opus-5-5" || model == "claude-sonnet-5-5" || model == "claude-opus-5" ||
         model.rfind("claude-fable-", 0) == 0;
}

enum class Provider : uint8_t { Anthropic, Gemini };

struct Config {
  Provider provider = Provider::Anthropic;
  std::string key;
  std::string model;
  bool explainInArabic = false;
};

int8_t configured = -1;  // -1 unknown, 0 no, 1 yes

// {"api_key": "...", "provider": "anthropic"|"gemini", "model": "...", "explain": "english"|"arabic"}
bool loadConfig(Config& config) {
  std::string raw;
  if (!Storage.exists(CONFIG_PATH) || !Storage.readFileToString("AI", CONFIG_PATH, MAX_CONFIG_BYTES, raw)) {
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, raw) != DeserializationError::Ok) {
    LOG_ERR("AI", "Bad %s", CONFIG_PATH);
    return false;
  }
  config.key = doc["api_key"] | "";
  const std::string provider = doc["provider"] | "anthropic";
  config.provider = provider == "gemini" ? Provider::Gemini : Provider::Anthropic;
  config.model = doc["model"] | (config.provider == Provider::Gemini ? GEMINI_DEFAULT_MODEL : ANTHROPIC_DEFAULT_MODEL);
  config.explainInArabic = std::string(doc["explain"] | "arabic") != "english";
  return !config.key.empty();
}

std::string userMessage(const std::string& text, const std::string& context) {
  std::string user = "Selection: " + text;
  if (!context.empty() && context != text) {
    user += "\nSurrounding text (background only; explain the selection, not this): " + context;
  }
  return user;
}

std::string buildAnthropicBody(const Config& config, const std::string& user) {
  JsonDocument doc;
  doc["model"] = config.model;
  doc["max_tokens"] = MAX_TOKENS;
  doc["output_config"]["effort"] = "low";
  if (supportsFallbacks(config.model)) doc["fallbacks"] = "default";  // retry on another model if declined
  doc["system"] = config.explainInArabic ? PROMPT_ARABIC : PROMPT_ENGLISH;
  JsonObject message = doc["messages"].add<JsonObject>();
  message["role"] = "user";
  message["content"] = user;
  std::string body;
  serializeJson(doc, body);
  return body;
}

std::string buildGeminiBody(const Config& config, const std::string& user) {
  JsonDocument doc;
  doc["system_instruction"]["parts"].add<JsonObject>()["text"] =
      config.explainInArabic ? PROMPT_ARABIC : PROMPT_ENGLISH;
  JsonObject content = doc["contents"].add<JsonObject>();
  content["role"] = "user";
  content["parts"].add<JsonObject>()["text"] = user;
  doc["generationConfig"]["maxOutputTokens"] = MAX_TOKENS;
  // thinkingLevel exists from Gemini 3; 2.x models reject it.
  if (config.model.rfind("gemini-2", 0) != 0) doc["generationConfig"]["thinkingConfig"]["thinkingLevel"] = "low";
  std::string body;
  serializeJson(doc, body);
  return body;
}

void appendAnswer(std::string& answerOut, const char* text) {
  if (!answerOut.empty()) answerOut += "\n";
  answerOut += text;
  if (answerOut.size() > MAX_ANSWER_BYTES) {
    size_t n = MAX_ANSWER_BYTES;  // cut on a UTF-8 boundary
    while (n > 0 && (static_cast<unsigned char>(answerOut[n]) & 0xC0) == 0x80) --n;
    answerOut.resize(n);
  }
}

Result parseAnthropic(const std::string& body, std::string& answerOut) {
  JsonDocument filter;
  filter["stop_reason"] = true;
  filter["content"][0]["type"] = true;
  filter["content"][0]["text"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter)) != DeserializationError::Ok) {
    return Result::BadResponse;
  }
  if (doc["stop_reason"] == "refusal") return Result::Refused;
  answerOut.clear();
  for (JsonVariantConst block : doc["content"].as<JsonArrayConst>()) {
    if (block["type"] != "text") continue;  // thinking blocks carry no text by default
    appendAnswer(answerOut, block["text"] | "");
  }
  return answerOut.empty() ? Result::BadResponse : Result::Ok;
}

Result parseGemini(const std::string& body, std::string& answerOut) {
  JsonDocument filter;
  filter["promptFeedback"]["blockReason"] = true;
  filter["candidates"][0]["finishReason"] = true;
  filter["candidates"][0]["content"]["parts"][0]["text"] = true;
  filter["candidates"][0]["content"]["parts"][0]["thought"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter)) != DeserializationError::Ok) {
    return Result::BadResponse;
  }
  if (!doc["promptFeedback"]["blockReason"].isNull()) return Result::Refused;
  JsonVariantConst candidate = doc["candidates"][0];
  const char* finish = candidate["finishReason"] | "STOP";
  answerOut.clear();
  for (JsonVariantConst part : candidate["content"]["parts"].as<JsonArrayConst>()) {
    if (part["thought"] | false) continue;  // thought summaries, if any
    appendAnswer(answerOut, part["text"] | "");
  }
  if (answerOut.empty() && strcmp(finish, "STOP") != 0 && strcmp(finish, "MAX_TOKENS") != 0) {
    return Result::Refused;  // SAFETY, RECITATION, PROHIBITED_CONTENT, ...
  }
  return answerOut.empty() ? Result::BadResponse : Result::Ok;
}

}  // namespace

bool isConfigured() {
  if (configured < 0) {
    Config config;
    configured = loadConfig(config) ? 1 : 0;
  }
  return configured == 1;
}

Explain defaultExplain() {
  Config config;
  return loadConfig(config) && !config.explainInArabic ? Explain::English : Explain::Arabic;
}

Result ask(const std::string& text, const std::string& context, const Explain explain, std::string& answerOut) {
  Config config;
  const bool loaded = loadConfig(config);
  configured = loaded ? 1 : 0;  // the file may have changed since isConfigured()
  if (!loaded) return Result::NotConfigured;
  config.explainInArabic = explain == Explain::Arabic;
  const bool gemini = config.provider == Provider::Gemini;
  std::string body;
  {
    const std::string user = userMessage(text, context);
    body = gemini ? buildGeminiBody(config, user) : buildAnthropicBody(config, user);
  }
  const auto heap = HalMemory::getInternalHeap();
  if (heap.freeBytes < MIN_FREE_FOR_REQUEST + body.size() || heap.largestBlockBytes < MIN_BLOCK_FOR_REQUEST) {
    LOG_ERR("AI", "Low heap for request: %u free, %u largest", static_cast<unsigned>(heap.freeBytes),
            static_cast<unsigned>(heap.largestBlockBytes));
    return Result::LowMemory;
  }
  // Certificate validity is checked against the clock; sync it if unknown.
  if (trustedtime::trustedNow() == 0) {
    resetTaskWatchdogIfSubscribed();  // the sync blocks; keep it under the watchdog's 5 s
    if (!trustedtime::syncNow(4000)) return Result::NoClock;
  }

  LOG_DBG("AI", "Asking %s (%s) about \"%.80s\" (context %u bytes, body %u bytes)", config.model.c_str(),
          config.explainInArabic ? "arabic" : "english", text.c_str(), static_cast<unsigned>(context.size()),
          static_cast<unsigned>(body.size()));
  freeink::SecureHttpClient http;
  http.setCACert(ROOT_CAS);
  http.setTimeout(REQUEST_TIMEOUT_MS);
  const std::string url = gemini ? GEMINI_URL_PREFIX + config.model + ":generateContent" : ANTHROPIC_URL;
  if (!http.begin(url)) return Result::Network;
  http.addHeader("content-type", "application/json");
  if (gemini) {
    http.addHeader("x-goog-api-key", config.key);
  } else {
    http.addHeader("x-api-key", config.key);
    http.addHeader("anthropic-version", "2026-10-21");
    if (supportsFallbacks(config.model)) http.addHeader("anthropic-beta", "server-side-fallback-2026-07-01");
  }

  std::string response;
  response.reserve(MAX_RESPONSE_BYTES);
  bool tooLarge = false;
  const int code = http.sendRequest(
      "POST", reinterpret_cast<const uint8_t*>(body.data()), body.size(),
      [&response, &tooLarge](const uint8_t* data, const size_t len) {
        if (response.size() + len > MAX_RESPONSE_BYTES) {
          tooLarge = true;
          return false;  // stop the transfer
        }
        response.append(reinterpret_cast<const char*>(data), len);
        return true;
      },
      [] {
        // A reply takes seconds; keep the loop task's watchdog fed.
        resetTaskWatchdogIfSubscribed();
        return false;
      });
  http.end();
  LOG_DBG("AI", "HTTP %d, %u bytes", code, static_cast<unsigned>(response.size()));
  // Gemini reports a bad key as 400 API_KEY_INVALID rather than 401.
  if (code == 401 || code == 403 || (code == 400 && response.find("API_KEY_INVALID") != std::string::npos)) {
    return Result::KeyRejected;
  }
  if (code <= 0 && !tooLarge) return Result::Network;
  if (code != 200 || tooLarge) {
    LOG_ERR("AI", "HTTP %d%s: %.200s", code, tooLarge ? " (body too large)" : "", response.c_str());
    return Result::BadResponse;
  }
  return gemini ? parseGemini(response, answerOut) : parseAnthropic(response, answerOut);
}

}  // namespace AiAssist
