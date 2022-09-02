#pragma once

#include "BaseTest.h"

class CommentaryTest : public BaseTest
{
    void init() override;
    void finish() override;
    void defaultCaseInit() override {}
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

private:
    using EnqueuedSamplesData = std::vector<void (*)()>;

    void setupCustomSamplesLoadingTest();
    void testCustomSamples();
    void setupLoadingCustomExtensionsTest();
    void testLoadingCustomExtensions();
    void setupHandlingBadFileTest();
    void testHandlingBadFile();
    void setupCommentInterruptionTest();
    void testCommentInterruption();
    void setupMutingCommentsTest();
    void testMutingComments();
    void setupEnqueuedCommentsTest();
    void testEnqueuedComments();
    void setupEndGameCommentsTest();
    void testEndGameComment();
    void testEndGameChantsAndCrowdSamples();

    void applyEnqueuedSamplesData(const EnqueuedSamplesData& data, const std::vector<int>& values);
    bool createNextPermutation(std::vector<int>& values);
    void testResultChants();
    void testEndGameCrowdSample();
};
