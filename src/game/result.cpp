#include "result.h"
#include "camera.h"

constexpr int kNumScorers = 8;

static void hideResult();

void resetResult()
{
    swos.resultOnHalftimeSprite.show();
    swos.resultAfterTheGameSprite.show();
//    swos.team1NameSprite.pictureIndex = SPR_TEAM1_NAME;
    A0 = &swos.team2NameSprite;
//    swos.team2NameSprite.pictureIndex = SPR_TEAM2_NAME;
    swos.gridUpperRowLeftSprite.hide();
    swos.gridUpperRowRightSprite.hide();
    swos.gridMiddleRowLeftSprite.hide();
    swos.gridMiddleRowRightSprite.hide();
    swos.gridBottomRowLeftSprite.hide();
    swos.gridBottomRowRightSprite.hide();
    swos.team2NameSprite.hide();
    swos.team2NameSprite.saveSprite = 0;
    swos.team1GoalsDigit2Sprite.hide();
    swos.team1GoalsDigit2Sprite.saveSprite = 0;
    swos.team1GoalsDigit1Sprite.hide();
    swos.team1GoalsDigit1Sprite.saveSprite = 0;
    swos.team1TotalGoalsDigit2Sprite.hide();
    swos.team1TotalGoalsDigit2Sprite.saveSprite = 0;
    swos.team1TotalGoalsDigit1Sprite.hide();
    swos.team1TotalGoalsDigit1Sprite.saveSprite = 0;
    swos.team2GoalsDigit2Sprite.hide();
    swos.team2GoalsDigit2Sprite.saveSprite = 0;
    swos.team2GoalsDigit1Sprite.hide();
    swos.team2GoalsDigit1Sprite.saveSprite = 0;
    swos.team2TotalGoalsDigit2Sprite.hide();
    swos.team2TotalGoalsDigit2Sprite.saveSprite = 0;
    swos.team2TotalGoalsDigit1Sprite.hide();
    swos.team2TotalGoalsDigit1Sprite.saveSprite = 0;
    swos.team1NameSprite.hide();
    swos.team1NameSprite.saveSprite = 0;
    swos.dashSprite.hide();
    swos.dashSprite.saveSprite = 0;

    A1 = &swos.team1Scorer1Sprite;
    A2 = &swos.team2Scorer1Sprite;
    //A3 = swos.team1Scorers;
    //A4 = swos.team2Scorers;
    //A5 = swos.team1ScorerSpritesWidths;
    //A6 = swos.team2ScorerSpritesWidths;
    //LOWORD(D0) = 7;
    //do {
    //    v0 = A1;
    //    *(A1 + offsetof(Sprite, visible)) = 0;
    //    *(v0 + offsetof(Sprite, saveSprite)) = 0;
    //    v1 = A2;
    //    *(A2 + offsetof(Sprite, visible)) = 0;
    //    *(v1 + offsetof(Sprite, saveSprite)) = 0;
    //    *A3 = 0;
    //    *A4 = 0;
    //    *A5 = 0;
    //    A5 += 2;
    //    *A6 = 0;
    //    A6 += 2;
    //    A1 += 110;
    //    A2 += 110;
    //    A3 += 66;
    //    A4 += 66;
    //    LOWORD(D0) = D0 - 1;
    //} while ((D0 & 0x8000u) == 0);

    InitDisplaySprites();
}

void showResult()
{
    if (swos.resultTimer < 0) {
        hideResult();
    } else if (swos.resultTimer > 0) {
        if (swos.resultTimer == 30'000 || swos.resultTimer == 31'000 || swos.resultTimer == 29'000) {
            switch (swos.resultTimer) {
            case 30'000:
                swos.resultTimer = 275;
                break;
            case 31'000:
                swos.resultTimer = 265;
                break;
            case 32'000:
                swos.resultTimer = 29'000;
                break;
            }

            if (swos.gameState == GameState::kResultOnHalftime)
                swos.resultOnHalftimeSprite.show();
            else if (swos.gameState == GameState::kResultAfterTheGame)
                swos.resultAfterTheGameSprite.show();

            if (swos.dontShowScorers || swos.secondLeg) {
                swos.team1TotalGoalsDigit2Sprite.show();
                if (swos.team1TotalGoals / 10)
                    swos.team1TotalGoalsDigit1Sprite.show();

                swos.team2TotalGoalsDigit2Sprite.show();
                if (swos.team2TotalGoals / 10)
                    swos.team2TotalGoalsDigit1Sprite.show();
            }

            swos.gridUpperRowLeftSprite.show();
            swos.gridUpperRowRightSprite.show();
            swos.gridMiddleRowLeftSprite.show();
            swos.gridMiddleRowRightSprite.show();
            swos.gridBottomRowLeftSprite.show();
            swos.gridBottomRowRightSprite.show();
            swos.team1NameSprite.show();
            swos.team2NameSprite.show();
            swos.team1GoalsDigit2Sprite.show();

            if (swos.team1GoalsDigit1)
                swos.team1GoalsDigit1Sprite.show();

            swos.team2GoalsDigit2Sprite.show();

            if (swos.team2GoalsDigit1)
                swos.team2GoalsDigit1Sprite.show();

            swos.team1NameSprite.show();
            swos.dashSprite.show();

            // show all the visible scorer sprites (8) which have non-zero scorer sprite width
            // team 1 & 2
            InitDisplaySprites();
        }

        if (swos.resultTimer <= swos.timerDifference) {
            if (swos.gameState == GameState::kResultOnHalftime || swos.gameState == GameState::kResultAfterTheGame)
                swos.statsTimeout = 32'000;
            hideResult();
        } else {
            // move team name up 7 pixels for each scorer line
            // force at least 14 pixels between team name and scorer list

            // set up team name sprites coordinates

            auto cameraX = getCameraX();
            auto cameraY = getCameraY();

            constexpr int kResultGridZ = 9'000;
            constexpr int kBigDigitZ = 10'000;
            constexpr int kSmallDigitZ = 10'002;

            auto y = cameraY - 64 + 246 - 15 + kResultGridZ;    // + 7 * num.scorers

            swos.gridUpperRowLeftSprite.x = cameraX;
            swos.gridUpperRowLeftSprite.y = y;
            swos.gridUpperRowLeftSprite.z = kResultGridZ;

            swos.gridUpperRowRightSprite.x = cameraX + 152;
            swos.gridUpperRowRightSprite.y = y;
            swos.gridUpperRowRightSprite.z = kResultGridZ;

            y += 32;

            swos.gridMiddleRowLeftSprite.x = cameraX;
            swos.gridMiddleRowLeftSprite.y = y;
            swos.gridMiddleRowLeftSprite.z = kResultGridZ;

            swos.gridMiddleRowRightSprite.x = cameraX + 152;
            swos.gridMiddleRowRightSprite.y = y;
            swos.gridMiddleRowRightSprite.z = kResultGridZ;

            y += 32;

            swos.gridBottomRowLeftSprite.x = cameraX;
            swos.gridBottomRowLeftSprite.y = y;
            swos.gridBottomRowLeftSprite.z = kResultGridZ;

            swos.gridBottomRowRightSprite.x = cameraX + 152;
            swos.gridBottomRowRightSprite.y = y;
            swos.gridBottomRowRightSprite.z = kResultGridZ;

            swos.team1GoalsDigit2Sprite.x = cameraX + 143;
            swos.team1GoalsDigit2Sprite.y = cameraY + 241 - 64 + kBigDigitZ;   // + 7 * num.scorers
            swos.team1GoalsDigit2Sprite.z = kBigDigitZ;
            swos.team1GoalsDigit2Sprite.pictureIndex = kBigZeroSprite + swos.team1GoalsDigit2;

            if (swos.team1GoalsDigit1Sprite.visible) {
                swos.team1GoalsDigit1Sprite.x = swos.team1GoalsDigit2Sprite.x - 12;
                swos.team1GoalsDigit1Sprite.y = swos.team1GoalsDigit2Sprite.y;
                swos.team1GoalsDigit1Sprite.z = swos.team1GoalsDigit2Sprite.z;
                swos.team1GoalsDigit2Sprite.pictureIndex = kBigZeroSprite + swos.team1GoalsDigit1;
            }

            if (swos.team1TotalGoalsDigit2Sprite.visible) {
                auto team1GoalsDigit1 = swos.team1TotalGoals / 10;
                auto team1GoalsDigit2 = swos.team1TotalGoals % 10;

                swos.team1TotalGoalsDigit2Sprite.x = cameraX + 150;
                if (team1GoalsDigit2 == 1)
                    swos.team1TotalGoalsDigit2Sprite.x += 4;
                swos.team1TotalGoalsDigit2Sprite.y = cameraY + 263 - 64 + kSmallDigitZ;
                swos.team1TotalGoalsDigit2Sprite.z = kSmallDigitZ;
                swos.team1TotalGoalsDigit2Sprite.pictureIndex = kSmallZeroSprite + team1GoalsDigit2;

                if (swos.team1TotalGoalsDigit1Sprite.visible) {
                    swos.team1TotalGoalsDigit1Sprite.x = swos.team1TotalGoalsDigit2Sprite.x - 6;
                    if (team1GoalsDigit1 == 1)
                        swos.team1TotalGoalsDigit1Sprite.x += 4;

                    swos.team1TotalGoalsDigit1Sprite.y = swos.team1TotalGoalsDigit2Sprite.y - 1;
                    swos.team1TotalGoalsDigit1Sprite.z = swos.team1TotalGoalsDigit2Sprite.z - 1;
                    swos.team1TotalGoalsDigit1Sprite.pictureIndex = kSmallZeroSprite + team1GoalsDigit1;
                }
            }

            swos.team2GoalsDigit2Sprite.x = cameraX + 165;
            swos.team2GoalsDigit2Sprite.y = cameraY + 241 - 64 + kBigDigitZ;
            swos.team2GoalsDigit2Sprite.z = kBigDigitZ;
            swos.team2GoalsDigit2Sprite.pictureIndex = swos.team2GoalsDigit2 + kBigZeroSprite;

            if (swos.team2GoalsDigit1Sprite.visible) {
                swos.team2GoalsDigit1Sprite.x = swos.team2GoalsDigit2Sprite.x + 12;
                swos.team2GoalsDigit1Sprite.y = swos.team2GoalsDigit2Sprite.y;
                swos.team2GoalsDigit1Sprite.z = swos.team2GoalsDigit2Sprite.z;
            }

            if (swos.team2TotalGoalsDigit2Sprite.visible) {
                auto team2GoalsDigit1 = swos.team2TotalGoals / 10;
                auto team2GoalsDigit2 = swos.team2TotalGoals % 10;

                swos.team2TotalGoalsDigit2Sprite.x = cameraX + 163;
                swos.team2TotalGoalsDigit2Sprite.y = cameraY + 263 - 64 + kSmallDigitZ;
                swos.team2TotalGoalsDigit2Sprite.z = kSmallDigitZ;
                swos.team2TotalGoalsDigit2Sprite.pictureIndex = kSmallZeroSprite + team2GoalsDigit2;

                if (swos.team2TotalGoalsDigit1Sprite.visible) {
                    swos.team2TotalGoalsDigit1Sprite.x = swos.team2TotalGoalsDigit2Sprite.x + 6;
                    if (team2GoalsDigit1 == 1)
                        swos.team2TotalGoalsDigit1Sprite.x -= 4;
                    swos.team2TotalGoalsDigit1Sprite.y = swos.team2TotalGoalsDigit2Sprite.y - 1;
                    swos.team2TotalGoalsDigit1Sprite.z = swos.team2TotalGoalsDigit2Sprite.z - 1;
                    swos.team2TotalGoalsDigit1Sprite.pictureIndex = kSmallZeroSprite + team2GoalsDigit1;

                }
            }

            swos.dashSprite.x = cameraX + 157;
            swos.dashSprite.y = cameraY + 249 - 64 + kBigDigitZ;
            swos.dashSprite.z = kBigDigitZ;

            // position scorer name sprites from ScorerInfo arrays

            swos.resultOnHalftimeSprite.x = cameraX + 160;
            swos.resultOnHalftimeSprite.y = swos.gridUpperRowLeftSprite.y + 990;
            swos.resultOnHalftimeSprite.z = kBigDigitZ;

            swos.resultAfterTheGameSprite.x = swos.resultOnHalftimeSprite.x;
            swos.resultAfterTheGameSprite.y = swos.resultOnHalftimeSprite.y;
            swos.resultAfterTheGameSprite.z = swos.resultOnHalftimeSprite.z;
        }
    }
}

void SWOS::ShowResult()
{
    showResult();
}

static void hideResult()
{
    swos.resultTimer = 0;

    if (swos.team1NameSprite.visible) {
        swos.resultOnHalftimeSprite.hide();
        swos.resultAfterTheGameSprite.hide();
        swos.gridUpperRowLeftSprite.hide();
        swos.gridUpperRowRightSprite.hide();
        swos.gridMiddleRowLeftSprite.hide();
        swos.gridMiddleRowRightSprite.hide();
        swos.gridBottomRowLeftSprite.hide();
        swos.gridBottomRowRightSprite.hide();
        swos.team1NameSprite.hide();
        swos.team2NameSprite.hide();
        swos.team1GoalsDigit2Sprite.hide();
        swos.team1GoalsDigit1Sprite.hide();
        swos.team1TotalGoalsDigit2Sprite.hide();
        swos.team1TotalGoalsDigit1Sprite.hide();
        swos.team2GoalsDigit2Sprite.hide();
        swos.team2GoalsDigit1Sprite.hide();
        swos.team2TotalGoalsDigit2Sprite.hide();
        swos.team2TotalGoalsDigit1Sprite.hide();
        swos.team1NameSprite.hide();
        swos.dashSprite.hide();

        auto team1Scorer = &swos.team1Scorer1Sprite;
        auto team2Scorer = &swos.team2Scorer1Sprite;

        for (int i = 0; i < kNumScorers; i++) {
            team1Scorer[i].hide();
            team2Scorer[i].hide();
        }

        InitDisplaySprites();
    }
}
