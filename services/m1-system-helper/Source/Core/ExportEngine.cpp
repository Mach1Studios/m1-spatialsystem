#include "ExportEngine.h"
#include "CaptureEngine.h" // ChunkHeader / StateSnapshot layout
#include <Mach1Encode.h>
#include <algorithm>
#include <map>

namespace Mach1 {

namespace {

// Samples rendered per pass; bounds memory regardless of session length.
constexpr int kWindowLength = 65536;

// Reject obviously corrupt chunk headers before trusting their sizes.
bool isSaneHeader(const ChunkHeader& header)
{
    return header.magic == 0x4D314348
        && header.version == 1
        && header.numSamples > 0
        && header.numSamples <= 1 << 20
        && header.numChannels > 0
        && header.numChannels <= 32
        && header.sampleRate > 0
        && header.stateSize == StateSnapshot::SIZE
        && header.audioDataSize == static_cast<uint32_t>(header.numChannels) * static_cast<uint32_t>(header.numSamples) * sizeof(float);
}

// Mirrors M1PannerAudioProcessor::applyStateToEncode() so the offline render
// matches what the panner would have produced locally.
void configureEncoderFromSnapshot(Mach1Encode<float>& encode, const StateSnapshot& snapshot, int exportOutputMode)
{
    encode.setInputMode(static_cast<Mach1EncodeInputMode>(snapshot.inputMode));
    encode.setOutputMode(static_cast<Mach1EncodeOutputMode>(exportOutputMode));

    encode.setAzimuthDegrees(snapshot.azimuthDeg);
    encode.setElevationDegrees(snapshot.elevationDeg);
    encode.setDiverge(snapshot.diverge / 100.0f);
    encode.setOutputGain(snapshot.gainDb, true);
    encode.setAutoOrbit(snapshot.autoOrbit);
    encode.setOrbitRotationDegrees(snapshot.stereoOrbitAzimuth);
    encode.setStereoSpread(snapshot.stereoSpread / 100.0f);
    encode.setPannerMode(static_cast<Mach1EncodePannerMode>(snapshot.pannerMode));

    encode.generatePointResults();
}

std::vector<SampleInterval> computeMissingIntervals(const CapturedIntervalSet& covered,
                                                    int64_t rangeStart, int64_t rangeEnd)
{
    std::vector<SampleInterval> missing;
    int64_t cursor = rangeStart;

    for (const auto& interval : covered.getIntervals())
    {
        const int64_t start = std::max(interval.start, rangeStart);
        const int64_t end = std::min(interval.end, rangeEnd);
        if (end <= start)
            continue;

        if (start > cursor)
            missing.emplace_back(cursor, start);
        cursor = std::max(cursor, end);
    }

    if (cursor < rangeEnd)
        missing.emplace_back(cursor, rangeEnd);

    return missing;
}

juce::var intervalToVar(const SampleInterval& interval, uint32_t sampleRate)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("startSample", static_cast<juce::int64>(interval.start));
    obj->setProperty("endSample", static_cast<juce::int64>(interval.end));
    if (sampleRate > 0)
    {
        obj->setProperty("startSeconds", static_cast<double>(interval.start) / sampleRate);
        obj->setProperty("endSeconds", static_cast<double>(interval.end) / sampleRate);
    }
    return juce::var(obj);
}

} // namespace

//==============================================================================
bool ExportEngine::indexChunkFile(const juce::File& chunkFile, StreamSource& out)
{
    juce::FileInputStream stream(chunkFile);
    if (!stream.openedOk())
        return false;

    const int64_t fileSize = stream.getTotalLength();
    int64_t offset = 0;

    while (offset + static_cast<int64_t>(ChunkHeader::SIZE + StateSnapshot::SIZE) <= fileSize)
    {
        stream.setPosition(offset);

        ChunkHeader header;
        if (stream.read(&header, static_cast<int>(ChunkHeader::SIZE)) != static_cast<int>(ChunkHeader::SIZE))
            break;

        if (!isSaneHeader(header))
            break; // corrupt or partially-written tail; keep what we have

        const int64_t chunkBytes = static_cast<int64_t>(ChunkHeader::SIZE) + header.stateSize + header.audioDataSize;
        if (offset + chunkBytes > fileSize)
            break; // partially flushed tail

        ChunkIndexEntry entry;
        entry.fileOffset = offset;
        entry.startSample = header.startSample;
        entry.numSamples = header.numSamples;
        entry.numChannels = header.numChannels;
        entry.sampleRate = header.sampleRate;
        entry.bufferId = header.bufferId;
        out.chunks.push_back(entry);

        out.covered.addInterval(header.startSample, header.startSample + header.numSamples);

        offset += chunkBytes;
    }

    std::stable_sort(out.chunks.begin(), out.chunks.end(),
                     [](const ChunkIndexEntry& a, const ChunkIndexEntry& b) {
                         return a.startSample < b.startSample;
                     });

    return !out.chunks.empty();
}

//==============================================================================
ExportEngine::Result ExportEngine::exportSession(const Request& request, const ProgressFn& progress)
{
    Result result;
    result.outputFile = request.outputFile;

    if (!request.sessionDir.isDirectory())
    {
        result.errorMessage = "Session directory not found: " + request.sessionDir.getFullPathName();
        return result;
    }

    //==========================================================================
    // Discover and index the captured streams

    std::vector<StreamSource> streams;
    for (const auto& entry : juce::RangedDirectoryIterator(request.sessionDir, false, "*", juce::File::findDirectories))
    {
        juce::File chunkFile = entry.getFile().getChildFile("chunks.bin");
        if (!chunkFile.existsAsFile())
            continue;

        StreamSource source;
        source.chunkFile = chunkFile;
        source.report.name = entry.getFile().getFileName().toStdString();
        if (indexChunkFile(chunkFile, source))
            streams.push_back(std::move(source));
    }

    if (streams.empty())
    {
        result.errorMessage = "No captured audio found in " + request.sessionDir.getFullPathName();
        return result;
    }

    //==========================================================================
    // Resolve the export range and bus format

    int64_t rangeStart = request.startSample;
    int64_t rangeEnd = request.endSample;
    if (rangeStart < 0 || rangeEnd <= rangeStart)
    {
        rangeStart = std::numeric_limits<int64_t>::max();
        rangeEnd = std::numeric_limits<int64_t>::min();
        for (const auto& stream : streams)
        {
            const auto bounds = stream.covered.getBoundingInterval();
            rangeStart = std::min(rangeStart, bounds.start);
            rangeEnd = std::max(rangeEnd, bounds.end);
        }
    }

    if (rangeEnd <= rangeStart)
    {
        result.errorMessage = "Captured range is empty";
        return result;
    }

    uint32_t sampleRate = 0;
    for (const auto& stream : streams)
        if (!stream.chunks.empty())
            sampleRate = std::max(sampleRate, stream.chunks.front().sampleRate);
    if (sampleRate == 0)
        sampleRate = 48000;

    Mach1Encode<float> formatProbe;
    formatProbe.setOutputMode(static_cast<Mach1EncodeOutputMode>(request.outputMode));
    const int outputChannels = formatProbe.getOutputChannelsCount();
    if (outputChannels <= 0)
    {
        result.errorMessage = "Invalid output mode: " + juce::String(request.outputMode);
        return result;
    }

    //==========================================================================
    // Open the destination WAV (32-bit float, N channels)

    request.outputFile.getParentDirectory().createDirectory();
    request.outputFile.deleteFile();

    std::unique_ptr<juce::FileOutputStream> outStream(request.outputFile.createOutputStream());
    if (!outStream || !outStream->openedOk())
    {
        result.errorMessage = "Cannot create output file: " + request.outputFile.getFullPathName();
        return result;
    }

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outStream.get(), sampleRate,
                                  static_cast<unsigned int>(outputChannels),
                                  32, {}, 0));
    if (!writer)
    {
        result.errorMessage = "Cannot create WAV writer";
        return result;
    }
    outStream.release(); // writer owns the stream now

    //==========================================================================
    // Windowed deterministic render

    std::vector<std::vector<float>> busBuffer(static_cast<size_t>(outputChannels),
                                              std::vector<float>(kWindowLength, 0.0f));
    std::vector<std::vector<float>> streamBuffer(static_cast<size_t>(outputChannels),
                                                 std::vector<float>(kWindowLength, 0.0f));
    std::vector<float*> busPointers(static_cast<size_t>(outputChannels));

    // One reusable encoder + open input stream per captured stream
    std::vector<std::unique_ptr<Mach1Encode<float>>> encoders;
    std::vector<std::unique_ptr<juce::FileInputStream>> inputs;
    for (auto& stream : streams)
    {
        encoders.push_back(std::make_unique<Mach1Encode<float>>());
        auto input = std::make_unique<juce::FileInputStream>(stream.chunkFile);
        if (!input->openedOk())
        {
            result.errorMessage = "Cannot re-open " + stream.chunkFile.getFullPathName();
            return result;
        }
        inputs.push_back(std::move(input));
    }

    std::vector<std::vector<float>> encodeIn;
    std::vector<std::vector<float>> encodeOut;

    const int64_t totalSamples = rangeEnd - rangeStart;

    for (int64_t windowStart = rangeStart; windowStart < rangeEnd; windowStart += kWindowLength)
    {
        const int windowLen = static_cast<int>(std::min<int64_t>(kWindowLength, rangeEnd - windowStart));
        const int64_t windowEnd = windowStart + windowLen;

        for (auto& channel : busBuffer)
            std::fill(channel.begin(), channel.begin() + windowLen, 0.0f);

        for (size_t streamIndex = 0; streamIndex < streams.size(); ++streamIndex)
        {
            auto& stream = streams[streamIndex];
            auto& input = *inputs[streamIndex];
            auto& encoder = *encoders[streamIndex];

            // Collect chunks overlapping this window, oldest bufferId first so
            // re-recorded regions (DAW loops / punch-ins) deterministically
            // resolve to the newest take.
            std::vector<const ChunkIndexEntry*> candidates;
            for (const auto& chunk : stream.chunks)
            {
                if (chunk.startSample >= windowEnd)
                    break; // sorted by startSample
                if (chunk.startSample + chunk.numSamples > windowStart)
                    candidates.push_back(&chunk);
            }
            if (candidates.empty())
                continue;

            std::stable_sort(candidates.begin(), candidates.end(),
                             [](const ChunkIndexEntry* a, const ChunkIndexEntry* b) {
                                 return a->bufferId < b->bufferId;
                             });

            for (auto& channel : streamBuffer)
                std::fill(channel.begin(), channel.begin() + windowLen, 0.0f);

            for (const auto* chunk : candidates)
            {
                input.setPosition(chunk->fileOffset);

                ChunkHeader header;
                StateSnapshot snapshot;
                if (input.read(&header, static_cast<int>(ChunkHeader::SIZE)) != static_cast<int>(ChunkHeader::SIZE)
                    || input.read(&snapshot, static_cast<int>(StateSnapshot::SIZE)) != static_cast<int>(StateSnapshot::SIZE))
                    continue;

                const int chunkSamples = chunk->numSamples;
                const int chunkChannels = chunk->numChannels;

                std::vector<float> interleaved(static_cast<size_t>(chunkSamples) * chunkChannels);
                const int audioBytes = static_cast<int>(interleaved.size() * sizeof(float));
                if (input.read(interleaved.data(), audioBytes) != audioBytes)
                    continue;

                configureEncoderFromSnapshot(encoder, snapshot, request.outputMode);

                const int encoderInputs = encoder.getInputChannelsCount();
                const int encoderOutputs = encoder.getOutputChannelsCount();
                if (encoderInputs <= 0 || encoderOutputs != outputChannels)
                    continue;

                if (static_cast<int>(encodeIn.size()) < encoderInputs)
                    encodeIn.resize(static_cast<size_t>(encoderInputs));
                for (int ch = 0; ch < encoderInputs; ++ch)
                {
                    encodeIn[static_cast<size_t>(ch)].assign(static_cast<size_t>(chunkSamples), 0.0f);
                    if (ch < chunkChannels)
                        for (int s = 0; s < chunkSamples; ++s)
                            encodeIn[static_cast<size_t>(ch)][static_cast<size_t>(s)] = interleaved[static_cast<size_t>(s) * chunkChannels + ch];
                }

                if (static_cast<int>(encodeOut.size()) < encoderOutputs)
                    encodeOut.resize(static_cast<size_t>(encoderOutputs));
                for (int ch = 0; ch < encoderOutputs; ++ch)
                    encodeOut[static_cast<size_t>(ch)].assign(static_cast<size_t>(chunkSamples), 0.0f);

                encoder.encodeBuffer(encodeIn, encodeOut, chunkSamples);

                // Copy the window-overlapping part, overwriting older takes
                const int64_t overlapStart = std::max<int64_t>(chunk->startSample, windowStart);
                const int64_t overlapEnd = std::min<int64_t>(chunk->startSample + chunkSamples, windowEnd);
                const int chunkOffset = static_cast<int>(overlapStart - chunk->startSample);
                const int windowOffset = static_cast<int>(overlapStart - windowStart);
                const int overlapLen = static_cast<int>(overlapEnd - overlapStart);

                for (int ch = 0; ch < outputChannels; ++ch)
                    std::copy_n(encodeOut[static_cast<size_t>(ch)].begin() + chunkOffset,
                                overlapLen,
                                streamBuffer[static_cast<size_t>(ch)].begin() + windowOffset);
            }

            for (int ch = 0; ch < outputChannels; ++ch)
                juce::FloatVectorOperations::add(busBuffer[static_cast<size_t>(ch)].data(),
                                                 streamBuffer[static_cast<size_t>(ch)].data(),
                                                 windowLen);
        }

        for (int ch = 0; ch < outputChannels; ++ch)
            busPointers[static_cast<size_t>(ch)] = busBuffer[static_cast<size_t>(ch)].data();

        if (!writer->writeFromFloatArrays(busPointers.data(), outputChannels, windowLen))
        {
            result.errorMessage = "Failed writing audio data";
            return result;
        }

        if (progress)
            progress(static_cast<float>(windowStart + windowLen - rangeStart) / static_cast<float>(totalSamples));
    }

    writer->flush();
    writer.reset();

    //==========================================================================
    // Coverage report

    CapturedIntervalSet unionMissing;
    for (auto& stream : streams)
    {
        auto& report = stream.report;

        // Last captured input mode / sample rate for this stream
        {
            juce::FileInputStream reread(stream.chunkFile);
            if (reread.openedOk() && !stream.chunks.empty())
            {
                const auto& last = stream.chunks.back();
                reread.setPosition(last.fileOffset + static_cast<int64_t>(ChunkHeader::SIZE));
                StateSnapshot snapshot;
                if (reread.read(&snapshot, static_cast<int>(StateSnapshot::SIZE)) == static_cast<int>(StateSnapshot::SIZE))
                    report.inputMode = snapshot.inputMode;
                report.sampleRate = last.sampleRate;
            }
        }

        report.chunkCount = static_cast<uint32_t>(stream.chunks.size());

        int64_t covered = 0;
        int64_t first = rangeEnd, last = rangeStart;
        for (const auto& interval : stream.covered.getIntervals())
        {
            const int64_t start = std::max(interval.start, rangeStart);
            const int64_t end = std::min(interval.end, rangeEnd);
            if (end <= start)
                continue;
            covered += end - start;
            first = std::min(first, start);
            last = std::max(last, end);
        }
        report.coveredSamples = covered;
        report.firstSample = covered > 0 ? first : 0;
        report.lastSample = covered > 0 ? last : 0;
        report.coveragePercent = totalSamples > 0 ? 100.0f * static_cast<float>(covered) / static_cast<float>(totalSamples) : 0.0f;
        report.missing = computeMissingIntervals(stream.covered, rangeStart, rangeEnd);

        for (const auto& gap : report.missing)
            unionMissing.addInterval(gap);

        result.streams.push_back(report);
    }

    const int64_t allCovered = totalSamples - unionMissing.getTotalCapturedSamples();
    result.allStreamsCoveragePercent = totalSamples > 0 ? 100.0f * static_cast<float>(allCovered) / static_cast<float>(totalSamples) : 0.0f;

    result.channels = outputChannels;
    result.sampleRate = sampleRate;
    result.startSample = rangeStart;
    result.endSample = rangeEnd;

    //==========================================================================
    // JSON sidecar so the coverage data survives next to the audio file

    {
        auto* root = new juce::DynamicObject();
        root->setProperty("sessionDir", request.sessionDir.getFullPathName());
        root->setProperty("outputFile", request.outputFile.getFullPathName());
        root->setProperty("outputMode", request.outputMode);
        root->setProperty("channels", outputChannels);
        root->setProperty("sampleRate", static_cast<juce::int64>(sampleRate));
        root->setProperty("startSample", static_cast<juce::int64>(rangeStart));
        root->setProperty("endSample", static_cast<juce::int64>(rangeEnd));
        root->setProperty("allStreamsCoveragePercent", result.allStreamsCoveragePercent);

        juce::Array<juce::var> streamsVar;
        for (const auto& report : result.streams)
        {
            auto* streamObj = new juce::DynamicObject();
            streamObj->setProperty("name", juce::String(report.name));
            streamObj->setProperty("inputMode", report.inputMode);
            streamObj->setProperty("sampleRate", static_cast<juce::int64>(report.sampleRate));
            streamObj->setProperty("chunkCount", static_cast<juce::int64>(report.chunkCount));
            streamObj->setProperty("coveredSamples", static_cast<juce::int64>(report.coveredSamples));
            streamObj->setProperty("coveragePercent", report.coveragePercent);
            streamObj->setProperty("received", report.coveredSamples > 0);

            juce::Array<juce::var> missingVar;
            for (const auto& gap : report.missing)
                missingVar.add(intervalToVar(gap, report.sampleRate));
            streamObj->setProperty("missing", missingVar);

            streamsVar.add(juce::var(streamObj));
        }
        root->setProperty("streams", streamsVar);

        result.reportFile = request.outputFile.withFileExtension("json");
        result.reportFile.replaceWithText(juce::JSON::toString(juce::var(root), false));
    }

    result.success = true;
    return result;
}

} // namespace Mach1
