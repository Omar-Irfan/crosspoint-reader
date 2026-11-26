#include <gtest/gtest.h>

#include "ReaderSession.h"

namespace {

TEST(ReaderSession, UsesOnlyBoundedScalarState) { EXPECT_LE(sizeof(ReaderSession), 48u); }

void anchor(ReaderSession& session, uint32_t ms, int64_t epoch, int progress) {
  session.onRenderComplete(ms, epoch, progress);
}

void turnAndRender(ReaderSession& session, bool forward, bool succeeded, uint32_t ms, int64_t epoch, int progress) {
  session.noteTurn(forward, succeeded);
  session.onRenderComplete(ms, epoch, progress);
}

TEST(ReaderSession, IgnoresDwellsUnderTwoSeconds) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, true, true, 2999, 102, 1100);

  EXPECT_EQ(session.durationSeconds(), 0u);
  EXPECT_FALSE(session.isEmitWorthy());
}

TEST(ReaderSession, AccumulatesSuccessfulForwardDwellsAndCompactsEndTime) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, true, true, 6000, 105, 1100);
  turnAndRender(session, true, true, 13000, 112, 1200);

  EXPECT_EQ(session.startTime(), 100);
  EXPECT_EQ(session.durationSeconds(), 12u);
  EXPECT_EQ(session.endTime(), 112);
  EXPECT_EQ(session.endTime(), session.startTime() + session.durationSeconds());
  EXPECT_EQ(session.startProgressBp(), 1000);
  EXPECT_EQ(session.endProgressBp(), 1200);
  EXPECT_TRUE(session.isEmitWorthy());
}

TEST(ReaderSession, ExcludesBackwardSkipAndFailedTurns) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, false, true, 11000, 110, 900);
  turnAndRender(session, true, false, 21000, 120, 1100);
  session.noteTurn(false, false);  // owner uses this signal for skips
  session.onRenderComplete(31000, 130, 2000);

  EXPECT_EQ(session.durationSeconds(), 0u);
}

TEST(ReaderSession, CapsOneDwellAtThirtyMinutes) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, true, true, 3601000, 3700, 2000);

  EXPECT_EQ(session.durationSeconds(), 1800u);
  EXPECT_EQ(session.endTime(), 1900);
}

TEST(ReaderSession, KeepsFirstEpochAndProgressAcrossContributions) {
  ReaderSession session;
  anchor(session, 1000, 1000, 3000);
  turnAndRender(session, true, true, 5000, 1004, 3100);
  turnAndRender(session, true, true, 11000, 1010, 3300);

  EXPECT_EQ(session.startTime(), 1000);
  EXPECT_EQ(session.startProgressBp(), 3000);
  EXPECT_EQ(session.endProgressBp(), 3300);
  EXPECT_EQ(session.durationSeconds(), 10u);
}

TEST(ReaderSession, ClampsProgressRangeAndMonotonicEnd) {
  ReaderSession session;
  anchor(session, 1000, 100, -50);
  turnAndRender(session, true, true, 7000, 106, 12000);
  turnAndRender(session, true, true, 13000, 112, 5000);

  EXPECT_EQ(session.startProgressBp(), 0);
  EXPECT_EQ(session.endProgressBp(), 10000);
}

TEST(ReaderSession, RequiresTenSecondsAndTrustedStart) {
  ReaderSession session;
  anchor(session, 1000, 0, 1000);
  turnAndRender(session, true, true, 11000, 10, 1100);
  EXPECT_EQ(session.durationSeconds(), 0u);

  turnAndRender(session, true, true, 20000, 19, 1200);
  EXPECT_EQ(session.durationSeconds(), 9u);
  EXPECT_FALSE(session.isEmitWorthy());

  turnAndRender(session, true, true, 22000, 21, 1300);
  EXPECT_EQ(session.durationSeconds(), 11u);
  EXPECT_TRUE(session.isEmitWorthy());
  EXPECT_GT(session.endTime(), session.startTime());
}

TEST(ReaderSession, ResetClearsSummaryAnchorAndPendingTurn) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  session.noteTurn(true, true);
  session.reset();
  session.onRenderComplete(12000, 111, 2000);

  EXPECT_EQ(session.startTime(), 0);
  EXPECT_EQ(session.durationSeconds(), 0u);
  EXPECT_EQ(session.startProgressBp(), 0);
  EXPECT_EQ(session.endProgressBp(), 0);
  EXPECT_FALSE(session.isEmitWorthy());
}

TEST(ReaderSession, MonotonicElapsedHandlesMillisWrap) {
  ReaderSession session;
  anchor(session, 0xfffffc18u, 100, 1000);
  turnAndRender(session, true, true, 4000u, 105, 1100);

  EXPECT_EQ(session.durationSeconds(), 5u);
}

TEST(ReaderSession, AnIdlePageCountsAsOneTypicalPage) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, true, true, 11000, 110, 1100);     // 10 s; pace 60 -> 48
  turnAndRender(session, true, true, 3611000, 3710, 1200);  // an hour away: one typical page

  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(3611000, epoch), 58u);
  EXPECT_EQ(epoch, 3710);
  EXPECT_EQ(session.takeTrackedSeconds(3611000, epoch), 0u);  // taking clears it
}

TEST(ReaderSession, TenIdleMinutesAddOnlyTheReadersPace) {
  ReaderSession session;
  uint32_t ms = 1000;
  anchor(session, ms, 100, 1000);
  for (int i = 0; i < 4; ++i) {
    ms += 40000;
    turnAndRender(session, true, true, ms, 100 + ms / 1000, 1100 + i);
  }
  EXPECT_EQ(session.paceSeconds(), 46u);
  ms += 600000;  // left open for ten minutes, then turned
  turnAndRender(session, true, true, ms, 100 + ms / 1000, 1200);

  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(ms, epoch), 160u + 46u);
}

TEST(ReaderSession, SlowPagesStillCountInFull) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, true, true, 151000, 250, 1100);  // 150 s
  turnAndRender(session, true, true, 301000, 400, 1200);  // 150 s
  turnAndRender(session, true, true, 541000, 640, 1300);  // 240 s: the pace allows it now

  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(541000, epoch), 540u);
}

TEST(ReaderSession, ASlowReaderLosesOnlyTheFirstLongPage) {
  ReaderSession session;  // no saved pace yet
  uint32_t ms = 1000;
  anchor(session, ms, 100, 1000);
  for (int i = 0; i < 4; ++i) {
    ms += 240000;  // four minutes a page from the start
    turnAndRender(session, true, true, ms, 100 + ms / 1000, 1100 + i);
  }
  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(ms, epoch), 60u + 3u * 240u);
  EXPECT_GE(session.paceSeconds(), 170u);
}

TEST(ReaderSession, ARestoredPaceCountsSlowPagesFromTheFirst) {
  ReaderSession session;
  session.restorePace(200);
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, true, true, 281000, 380, 1100);  // 280 s
  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(281000, epoch), 280u);
  EXPECT_EQ(session.takeTrackedSeconds(281000 + 600000, epoch), 220u);  // asleep on the next page: one typical page
}

TEST(ReaderSession, NoPageCountsMoreThanFiveMinutes) {
  ReaderSession session;
  uint32_t ms = 1000;
  anchor(session, ms, 100, 1000);
  for (int i = 0; i < 12; ++i) {
    ms += 290000;
    turnAndRender(session, true, true, ms, 100 + ms / 1000, 1100 + i);
  }
  ms += 400000;
  turnAndRender(session, true, true, ms, 100 + ms / 1000, 1200);
  int64_t epoch = 0;
  EXPECT_LE(session.takeTrackedSeconds(ms, epoch), 12u * 290u + 290u);
}

TEST(ReaderSession, TrackedTimeIgnoresBackwardTurnsAndResets) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  turnAndRender(session, false, true, 11000, 110, 900);
  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(11000, epoch), 0u);

  turnAndRender(session, true, true, 21000, 120, 1000);
  session.reset();
  EXPECT_EQ(session.takeTrackedSeconds(21000, epoch), 0u);
}

TEST(ReaderSession, TakeCountsThePageBeingReadOnceUpToThePace) {
  ReaderSession session;
  anchor(session, 1000, 100, 1000);
  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(61000, epoch), 60u);  // a minute on the last page
  EXPECT_EQ(session.takeTrackedSeconds(61500, epoch), 0u);   // a second flush adds nothing

  anchor(session, 70000, 170, 1100);
  EXPECT_EQ(session.takeTrackedSeconds(70000 + 3600000, epoch), 60u);  // walked away: one typical page
}

TEST(ReaderSession, SleepingOnAPageCountsAtMostTheReadersPace) {
  ReaderSession session;
  uint32_t ms = 1000;
  anchor(session, ms, 100, 1000);
  for (int i = 0; i < 4; ++i) {
    ms += 40000;
    turnAndRender(session, true, true, ms, 100 + ms / 1000, 1100 + i);
  }
  int64_t epoch = 0;
  EXPECT_EQ(session.takeTrackedSeconds(ms + 600000, epoch), 160u + 46u);  // auto-sleep after ten idle minutes

  session.reset();
  EXPECT_EQ(session.paceSeconds(), 46u);  // the pace outlives the session
}

TEST(ReaderSession, TakeIgnoresAnUntrustedClock) {
  ReaderSession session;
  anchor(session, 1000, 0, 1000);
  int64_t epoch = 1;
  EXPECT_EQ(session.takeTrackedSeconds(61000, epoch), 0u);
  EXPECT_EQ(epoch, 0);
}

}  // namespace
