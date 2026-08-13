/*
    ExportEngine.h
    --------------
    Offline, deterministic export of a captured session to a multichannel
    Mach1 Spatial mix file.

    Reads the append-only chunk files written by CaptureEngine
    (<sessionDir>/<panner>/chunks.bin), re-encodes every audio block with the
    parameter snapshot that was captured in the same processBlock() as the
    audio (sample-aligned automation), sums all panner streams into a single
    spatial bus and writes a 32-bit float WAV.

    Coverage gating: regions where a stream has no captured data render as
    silence for that stream, and the export result reports exactly which
    input streams were received and where each one has gaps, so the user can
    re-run the section and re-export.

    Determinism: chunks are applied in bufferId order; when a DAW loop or
    punch-in re-records a region, the newest chunk wins. Running the same
    export twice yields byte-identical output.
*/

#pragma once

#include <JuceHeader.h>
#include "CoverageModel.h"
#include <functional>
#include <vector>

namespace Mach1 {

class ExportEngine
{
public:
    //==========================================================================
    struct Request
    {
        // Session directory that contains one subdirectory per panner stream
        // (each with a chunks.bin), i.e. <captureRoot>/<sessionId>.
        juce::File sessionDir;

        // Destination WAV file. Parent directories are created. A JSON
        // coverage report is written next to it (same name, .json).
        juce::File outputFile;

        // Mach1EncodeOutputMode value for the export bus
        // (M1Spatial_4 / M1Spatial_8 / M1Spatial_14).
        int outputMode = 1; // M1Spatial_8

        // Export range in timeline samples. -1 = derive from captured data
        // (union of all stream bounds).
        int64_t startSample = -1;
        int64_t endSample = -1;
    };

    //==========================================================================
    /** Per-input-stream coverage summary for the export range. */
    struct StreamReport
    {
        std::string name;            // panner directory name (stable instance id)
        std::string displayName;     // DAW track name (from name.txt, may be empty)
        int inputMode = 0;           // last captured Mach1EncodeInputMode
        uint32_t sampleRate = 0;
        uint32_t chunkCount = 0;
        int64_t firstSample = 0;     // first captured sample in range
        int64_t lastSample = 0;      // last captured sample in range (exclusive)
        int64_t coveredSamples = 0;  // captured samples within export range
        float coveragePercent = 0.0f;
        std::vector<SampleInterval> missing; // gaps within the export range
    };

    struct Result
    {
        bool success = false;
        juce::String errorMessage;

        juce::File outputFile;
        juce::File reportFile;

        int channels = 0;
        uint32_t sampleRate = 0;
        int64_t startSample = 0;
        int64_t endSample = 0;

        // All distinct sample rates found in the captured chunks (sorted).
        // More than one entry means the DAW rate changed mid-session; sample
        // positions from different rates don't share a timeline, so the
        // export is flagged for the user to re-capture.
        std::vector<uint32_t> capturedSampleRates;

        // Percent of the export range where EVERY stream has coverage.
        float allStreamsCoveragePercent = 0.0f;
        std::vector<StreamReport> streams;
    };

    //==========================================================================
    /**
     * Render the session to request.outputFile. Blocking; run it on a worker
     * thread. Safe to call while capture is still running (exports whatever
     * has been flushed to disk at index time).
     *
     * @param progress optional callback with [0..1] completion
     */
    using ProgressFn = std::function<void(float)>;
    static Result exportSession(const Request& request, const ProgressFn& progress = nullptr);

private:
    struct ChunkIndexEntry
    {
        int64_t fileOffset = 0;   // offset of the ChunkHeader in chunks.bin
        int64_t startSample = 0;
        int32_t numSamples = 0;
        int16_t numChannels = 0;
        uint32_t sampleRate = 0;
        uint64_t bufferId = 0;
    };

    struct StreamSource
    {
        juce::File chunkFile;
        std::vector<ChunkIndexEntry> chunks; // sorted by startSample
        CapturedIntervalSet covered;
        StreamReport report;
    };

    static bool indexChunkFile(const juce::File& chunkFile, StreamSource& out);

    ExportEngine() = delete;
};

} // namespace Mach1
