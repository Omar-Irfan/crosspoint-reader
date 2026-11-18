#pragma once

#include <Epub/Page.h>
#include <I18n.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ClipSelectionActivity.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
#include "util/Dictionary.h"

// Paged viewer for one dictionary definition. HTML definitions are laid out
// through the EPUB chapter parser into styled Pages; anything else (plain
// text, or HTML too damaged to parse) is word-wrapped once on entry and each
// page renders spans of the original string, so no per-line copies are held.
class DictionaryDefinitionActivity final : public Activity {
 public:
  explicit DictionaryDefinitionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string headword,
                                        std::string definition, bool htmlDefinition = false)
      : Activity("DictionaryDefinition", renderer, mappedInput),
        content{.headword = std::move(headword), .text = std::move(definition)},
        htmlDefinition(htmlDefinition) {}

  bool enableLookup(Dictionary& dictionary, std::string&& query);
  // Hold Confirm (or tap the header on touch boards) to ask the configured AI
  // about the looked-up text in this context (see AiAssist).
  void enableAi(std::string context) {
    aiMode = AiMode::Ask;
    aiContext = std::move(context);
    aiCacheRef = &aiCache;
  }

  // Looks word up; a phrase that is not an entry is glossed word by word
  // instead (see DictionaryGloss), as HTML when html is set, otherwise as
  // plain text with headword-marker lines. glossed reports which happened.
  static bool lookupOrGloss(Dictionary& dictionary, const char* word, bool html, std::string& text,
                            std::string& headword, Dictionary::LookupResult& result, bool* glossed = nullptr);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // One wrapped display line: a byte span of `definition`. Wrapping keeps
  // lines under the screen width, so uint16_t length is ample.
  struct PlainSelectionLine;

  struct Line {
    uint32_t start;
    uint16_t len;
    bool bold;
  };

  // Usable body-text area between the header and the button hints.
  struct BodyArea {
    int x;
    int y;
    int width;
    int height;
  };

  BodyArea bodyArea() const;

  struct Definition {
    std::string headword;
    std::string text;
    std::vector<std::unique_ptr<Page>> pages;
    std::vector<Line> lines;
    int currentPage = 0;
    int totalPages = 1;
    int linesPerPage = 1;
    int selectionPage = 0;
    ClipSelectionActivity::Selection selection;

    void releasePayload() {
      std::string{}.swap(text);
      std::vector<std::unique_ptr<Page>>{}.swap(pages);
      std::vector<Line>{}.swap(lines);
    }
  };

  // Replay the original query: the displayed headword may be a stem or synonym.
  struct HistoryEntry {
    std::string query;
    int page = 0;
    int selectionPage = 0;
    ClipSelectionActivity::Selection selection;
    bool styled = false;
  };

  struct LookupState {
    explicit LookupState(Dictionary& dictionary) : dictionary(dictionary) {}
    Dictionary& dictionary;  // Owned by the retained DictionaryWordSelectActivity parent.
    std::array<HistoryEntry, 16> history;
    size_t depth = 0;
    std::string query;
    std::string pendingWord;
    std::vector<std::unique_ptr<Page>> selectionPages;
  };

  bool prepareDefinition(Definition& next, bool forcePlain = false);
  bool layoutHtmlPages(Definition& next);
  bool wrapText(Definition& next);
  bool prepareFont(std::string& definition, int fontId);
  bool reserveLines(Definition& next, size_t count);
  int measureSpan(int fontId, const char* text, size_t len, EpdFontFamily::Style family = EpdFontFamily::REGULAR) const;
  void drawBody(int fontId, int x, int startY, int currentPage) const;
  void startSelection(int x = -1, int y = -1);
  bool buildPlainSelectionPages();
  void navigateDefinition(bool back);
  bool restoreCurrentDefinition(bool styled);
  // False when no word is found; result is then LowMemory or NotFound.
  static bool buildGloss(Dictionary& dictionary, const std::string& phrase, bool html, std::string& out,
                         Dictionary::LookupResult& result);
  void showMessage(StrId message);
  // Ask: a definition that can ask AI. Answer: an AI answer screen, whose
  // Confirm (or header tap) asks the same question in the other language.
  enum class AiMode : uint8_t { None, Ask, Answer };

  void askAi();
  void runAi(std::string context, bool arabic);
  void switchAnswerLanguage();
  bool fetchAnswer(const std::string& query, const std::string& context, bool arabic, std::string& answer);
  bool aiTapAvailable() const {
    return ((aiMode == AiMode::Ask && lookup) || aiMode == AiMode::Answer) && mappedInput.hasTouch();
  }
  // Answers already fetched while this definition is open, so asking again or
  // switching language back does not call the API. Owned by the top-level
  // definition and dropped when it closes (back to the book).
  struct AiCacheEntry {
    std::string query;
    std::string context;
    bool arabic;
    std::string answer;  // the model's text only; the matn is added for display
  };
#ifdef BOARD_HAS_PSRAM
  static constexpr size_t AI_CACHE_MAX = 6;
#else
  static constexpr size_t AI_CACHE_MAX = 2;  // entries live in internal RAM next to WiFi
#endif

  void enableAiAnswer(std::string context, bool arabic, std::vector<AiCacheEntry>* cache) {
    aiMode = AiMode::Answer;
    aiAnswerArabic = arabic;
    aiContext = std::move(context);
    aiCacheRef = cache;
  }
  const std::string* cachedAnswer(const std::string& query, const std::string& context, bool arabic) const;
  void cacheAnswer(const std::string& query, const std::string& context, bool arabic, const std::string& answer);
  void openAnswer(const std::string& answer, std::string context, bool arabic);
  bool showAnswerInPlace(const std::string& answer, bool arabic);

  Definition content;
  const bool htmlDefinition;
  std::unique_ptr<LookupState> lookup;
  StrId message = StrId::STR_DICT_NOT_FOUND;
  unsigned long messageTime = 0;
  bool messageVisible = false;
  bool initialLayoutFailed = false;
  AiMode aiMode = AiMode::None;
  bool networkUsed = false;  // Ask AI brought WiFi up: reboot into the book on exit
  bool aiAnswerArabic = true;
  std::vector<AiCacheEntry> aiCache;
  std::vector<AiCacheEntry>* aiCacheRef = nullptr;  // aiCache, or the parent's on an answer screen
  // Header tap target for Ask AI on touch boards; written by render, read by loop.
  Rect aiTapRect{};
  std::string aiContext;
  ButtonNavigator buttonNavigator;
};
