/*
    ExportEngineTests.cpp
    ---------------------
    Regression tests for the offline export pipeline (Core/ExportEngine).

    Verifies against synthetic capture sessions (chunks.bin files written the
    same way CaptureEngine writes them):
      - a fully covered mono stream renders to a WAV of the exact captured
        length with the requested spatial channel count and audible content
      - capture gaps render as silence and are reported per stream with exact
        sample bounds
      - multiple streams sum into one bus; the "all streams" coverage percent
        reflects the intersection of their coverage
      - overlapping re-recorded regions resolve deterministically to the
        newest take (bufferId order), and exports are reproducible
*/

#include <JuceHeader.h>
#include "Core/ExportEngine.h"
#include "Core/CaptureEngine.h" // ChunkHeader / StateSnapshot
#include <cmath>
#include <iostream>

namespace {

int exportFailures = 0;

#define EXPORT_CHECK(cond, message)                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ++exportFailures;                                                   \
            std::cout << "FAILED: " << message << " (" << #cond << ") at "      \
                      << __FILE__ << ":" << __LINE__ << std::endl;              \
        }                                                                       \
    } while (false)

using Mach1::ChunkHeader;
using Mach1::ExportEngine;
using Mach1::StateSnapshot;

StateSnapshot makeSnapshot()
{
    StateSnapshot snapshot;
    snapshot.azimuthDeg = 0.0f;
    snapshot.elevationDeg = 0.0f;
    snapshot.diverge = 50.0f;   // stored as -100..100, engine scales to -1..1
    snapshot.gainDb = 0.0f;
    snapshot.inputMode = 0;     // Mono
    snapshot.outputMode = 1;    // M1Spatial_8 (informational only)
    snapshot.pannerMode = 0;
    return snapshot;
}

void appendChunk(juce::FileOutputStream& stream, int64_t startSample, int numSamples,
                 int channels, uint32_t sampleRate, uint64_t bufferId, float fillValue,
                 const StateSnapshot& snapshot)
{
    ChunkHeader header;
    header.startSample = startSample;
    header.numSamples = numSamples;
    header.numChannels = static_cast<int16_t>(channels);
    header.sampleRate = sampleRate;
    header.bufferId = bufferId;
    header.sequenceNumber = static_cast<uint32_t>(bufferId);
    header.audioDataSize = static_cast<uint32_t>(channels * numSamples * sizeof(float));

    std::vector<float> audio(static_cast<size_t>(channels) * numSamples, fillValue);

    stream.write(&header, ChunkHeader::SIZE);
    stream.write(&snapshot, StateSnapshot::SIZE);
    stream.write(audio.data(), header.audioDataSize);
}

struct TestSession
{
    juce::File root;
    juce::File dir;

    explicit TestSession(const juce::String& name)
    {
        root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("m1-export-tests")
                   .getChildFile(name + "_" + juce::String(juce::Random::getSystemRandom().nextInt(1 << 30)));
        dir = root.getChildFile("Session");
        dir.createDirectory();
    }

    ~TestSession() { root.deleteRecursively(); }

    juce::File streamDir(const juce::String& streamName) const
    {
        juce::File d = dir.getChildFile(streamName);
        d.createDirectory();
        return d;
    }
};

juce::AudioBuffer<float> readWav(const juce::File& file, double& sampleRateOut)
{
    juce::AudioBuffer<float> buffer;
    sampleRateOut = 0.0;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(), true));
    if (reader == nullptr)
        return buffer;

    buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    sampleRateOut = reader->sampleRate;
    return buffer;
}

float maxAbsInRegion(const juce::AudioBuffer<float>& buffer, int start, int end)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int s = start; s < end && s < buffer.getNumSamples(); ++s)
            peak = std::max(peak, std::abs(buffer.getSample(ch, s)));
    return peak;
}

//==============================================================================
void testFullCoverageMonoStream()
{
    TestSession session("full");

    {
        juce::FileOutputStream out(session.streamDir("pannerA_100").getChildFile("chunks.bin"));
        StateSnapshot snapshot = makeSnapshot();
        for (int i = 0; i < 20; ++i)
            appendChunk(out, 1000 + i * 480, 480, 1, 48000, static_cast<uint64_t>(i + 1), 1.0f, snapshot);
    }

    ExportEngine::Request request;
    request.sessionDir = session.dir;
    request.outputFile = session.root.getChildFile("out.wav");
    request.outputMode = 1; // M1Spatial_8

    const auto result = ExportEngine::exportSession(request);

    EXPORT_CHECK(result.success, "full-coverage export succeeds");
    EXPORT_CHECK(result.channels == 8, "export bus is 8 channels");
    EXPORT_CHECK(result.sampleRate == 48000, "sample rate taken from capture");
    EXPORT_CHECK(result.startSample == 1000 && result.endSample == 1000 + 20 * 480,
                 "range derived from captured bounds");
    EXPORT_CHECK(std::abs(result.allStreamsCoveragePercent - 100.0f) < 0.01f,
                 "full coverage reported");
    EXPORT_CHECK(result.streams.size() == 1 && result.streams[0].missing.empty(),
                 "no gaps reported");
    EXPORT_CHECK(result.reportFile.existsAsFile(), "JSON report written");

    double wavSampleRate = 0.0;
    const auto rendered = readWav(result.outputFile, wavSampleRate);
    EXPORT_CHECK(rendered.getNumChannels() == 8, "WAV has 8 channels");
    EXPORT_CHECK(rendered.getNumSamples() == 20 * 480, "WAV length matches captured range");
    EXPORT_CHECK(wavSampleRate == 48000.0, "WAV sample rate matches");
    EXPORT_CHECK(maxAbsInRegion(rendered, 0, rendered.getNumSamples()) > 0.01f,
                 "rendered mix contains audio");
}

//==============================================================================
void testGapRendersSilenceAndIsReported()
{
    TestSession session("gap");

    {
        juce::FileOutputStream out(session.streamDir("pannerA_100").getChildFile("chunks.bin"));
        StateSnapshot snapshot = makeSnapshot();
        uint64_t bufferId = 1;
        for (int i = 0; i < 10; ++i) // [0, 4800)
            appendChunk(out, i * 480, 480, 1, 48000, bufferId++, 1.0f, snapshot);
        for (int i = 0; i < 10; ++i) // [9600, 14400)
            appendChunk(out, 9600 + i * 480, 480, 1, 48000, bufferId++, 1.0f, snapshot);
    }

    ExportEngine::Request request;
    request.sessionDir = session.dir;
    request.outputFile = session.root.getChildFile("out.wav");
    request.outputMode = 1;

    const auto result = ExportEngine::exportSession(request);

    EXPORT_CHECK(result.success, "gap export succeeds");
    EXPORT_CHECK(result.streams.size() == 1, "one stream reported");
    if (result.streams.size() == 1)
    {
        const auto& missing = result.streams[0].missing;
        EXPORT_CHECK(missing.size() == 1, "exactly one gap reported");
        if (missing.size() == 1)
            EXPORT_CHECK(missing[0].start == 4800 && missing[0].end == 9600,
                         "gap bounds are sample-exact");
    }
    EXPORT_CHECK(std::abs(result.allStreamsCoveragePercent - (100.0f * 9600.0f / 14400.0f)) < 0.1f,
                 "coverage percent reflects the gap");

    double wavSampleRate = 0.0;
    const auto rendered = readWav(result.outputFile, wavSampleRate);
    EXPORT_CHECK(rendered.getNumSamples() == 14400, "WAV spans the full range");
    EXPORT_CHECK(maxAbsInRegion(rendered, 0, 4800) > 0.01f, "covered head has audio");
    EXPORT_CHECK(maxAbsInRegion(rendered, 4800, 9600) == 0.0f, "gap renders as silence");
    EXPORT_CHECK(maxAbsInRegion(rendered, 9600, 14400) > 0.01f, "covered tail has audio");
}

//==============================================================================
void testTwoStreamsSumAndIntersectionCoverage()
{
    TestSession session("two");

    {
        juce::FileOutputStream out(session.streamDir("pannerA_100").getChildFile("chunks.bin"));
        StateSnapshot snapshot = makeSnapshot();
        for (int i = 0; i < 10; ++i) // [0, 4800)
            appendChunk(out, i * 480, 480, 1, 48000, static_cast<uint64_t>(i + 1), 1.0f, snapshot);
    }
    {
        juce::FileOutputStream out(session.streamDir("pannerB_200").getChildFile("chunks.bin"));
        StateSnapshot snapshot = makeSnapshot();
        for (int i = 0; i < 10; ++i) // [2400, 7200)
            appendChunk(out, 2400 + i * 480, 480, 1, 48000, static_cast<uint64_t>(i + 1), 1.0f, snapshot);
    }

    ExportEngine::Request request;
    request.sessionDir = session.dir;
    request.outputFile = session.root.getChildFile("out.wav");
    request.outputMode = 1;

    const auto result = ExportEngine::exportSession(request);

    EXPORT_CHECK(result.success, "two-stream export succeeds");
    EXPORT_CHECK(result.startSample == 0 && result.endSample == 7200,
                 "range is the union of both streams");
    EXPORT_CHECK(result.streams.size() == 2, "both streams reported");

    // Intersection coverage: only [2400, 4800) has all streams -> 1/3 of range
    EXPORT_CHECK(std::abs(result.allStreamsCoveragePercent - (100.0f * 2400.0f / 7200.0f)) < 0.1f,
                 "all-streams coverage is the intersection");

    double wavSampleRate = 0.0;
    const auto rendered = readWav(result.outputFile, wavSampleRate);

    // Region covered by both should be exactly the sum of the per-stream
    // contributions; with identical params/content it is 2x a single stream.
    const float soloPeak = maxAbsInRegion(rendered, 0, 2400);         // stream A only
    const float duoPeak = maxAbsInRegion(rendered, 2400, 4800);       // A + B
    EXPORT_CHECK(soloPeak > 0.01f, "solo region has audio");
    EXPORT_CHECK(std::abs(duoPeak - 2.0f * soloPeak) < 0.001f, "overlap region sums both streams");
}

//==============================================================================
void testNewestTakeWinsAndDeterminism()
{
    // Session A: single take with value 1.0
    TestSession sessionA("takeA");
    {
        juce::FileOutputStream out(sessionA.streamDir("pannerA_100").getChildFile("chunks.bin"));
        appendChunk(out, 0, 480, 1, 48000, 1, 1.0f, makeSnapshot());
    }

    // Session B: same take, then a newer overlapping take with value 2.0
    TestSession sessionB("takeB");
    {
        juce::FileOutputStream out(sessionB.streamDir("pannerA_100").getChildFile("chunks.bin"));
        appendChunk(out, 0, 480, 1, 48000, 1, 1.0f, makeSnapshot());
        appendChunk(out, 0, 480, 1, 48000, 2, 2.0f, makeSnapshot());
    }

    ExportEngine::Request requestA;
    requestA.sessionDir = sessionA.dir;
    requestA.outputFile = sessionA.root.getChildFile("outA.wav");
    requestA.outputMode = 1;

    ExportEngine::Request requestB;
    requestB.sessionDir = sessionB.dir;
    requestB.outputFile = sessionB.root.getChildFile("outB.wav");
    requestB.outputMode = 1;

    const auto resultA = ExportEngine::exportSession(requestA);
    const auto resultB = ExportEngine::exportSession(requestB);
    EXPORT_CHECK(resultA.success && resultB.success, "both take exports succeed");

    double srA = 0.0, srB = 0.0;
    const auto renderedA = readWav(resultA.outputFile, srA);
    const auto renderedB = readWav(resultB.outputFile, srB);

    EXPORT_CHECK(renderedA.getNumSamples() == 480 && renderedB.getNumSamples() == 480,
                 "take renders span the captured range");

    // Newest take (value 2.0) must fully replace the old one: B == 2 * A
    bool newestWins = renderedA.getNumSamples() == renderedB.getNumSamples()
                   && renderedA.getNumChannels() == renderedB.getNumChannels();
    if (newestWins)
    {
        for (int ch = 0; ch < renderedA.getNumChannels() && newestWins; ++ch)
            for (int s = 0; s < renderedA.getNumSamples() && newestWins; ++s)
                if (std::abs(renderedB.getSample(ch, s) - 2.0f * renderedA.getSample(ch, s)) > 0.0001f)
                    newestWins = false;
    }
    EXPORT_CHECK(newestWins, "overlapping region resolves to the newest take");

    // Determinism: exporting session B again yields byte-identical audio
    ExportEngine::Request requestB2 = requestB;
    requestB2.outputFile = sessionB.root.getChildFile("outB2.wav");
    const auto resultB2 = ExportEngine::exportSession(requestB2);
    EXPORT_CHECK(resultB2.success, "repeat export succeeds");

    double srB2 = 0.0;
    const auto renderedB2 = readWav(resultB2.outputFile, srB2);
    bool identical = renderedB.getNumSamples() == renderedB2.getNumSamples()
                  && renderedB.getNumChannels() == renderedB2.getNumChannels();
    if (identical)
    {
        for (int ch = 0; ch < renderedB.getNumChannels() && identical; ++ch)
            for (int s = 0; s < renderedB.getNumSamples() && identical; ++s)
                if (renderedB.getSample(ch, s) != renderedB2.getSample(ch, s))
                    identical = false;
    }
    EXPORT_CHECK(identical, "repeated export is bit-identical");
}

} // namespace

// Called from HelperServiceTests.cpp's main(); returns failed check count.
int runExportEngineTests()
{
    testFullCoverageMonoStream();
    testGapRendersSilenceAndIsReported();
    testTwoStreamsSumAndIntersectionCoverage();
    testNewestTakeWinsAndDeterminism();

    if (exportFailures == 0)
        std::cout << "All ExportEngine tests passed" << std::endl;

    return exportFailures;
}
