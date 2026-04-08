#include "platform.hpp"

#include "n3ds/audio.hpp"
#include "n3ds/ctru++.hpp"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <strings.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <3ds.h>

#include "def.h"

#include <xmp.h>

#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>

#include <opusfile.h>

#include <mpg123.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

// --- Poor-man's WAV parser ---
//
// Based on http://soundfile.sapp.org/doc/WaveFormat/
namespace wav {

struct __attribute((packed)) WavUnknownSubchunk {
    u32 SubchunkID;
    u32 SubchunkSize; // size of the the rest of the chunk following this field
};

struct __attribute__((packed)) WavSubchunk1 {
    u32 Subchunk1ID;   // "fmt ", 0x20746d66
    u32 Subchunk1Size; // 16 for PCM
    u16 AudioFormat;   // 1 for PCM
    u16 NumChannels;   // 1 or 2
    u32 SampleRate;    // sample rate
    u32 ByteRate;      // SampleRate * NumChannels * BitsPerSample/8
    u16 BlockAlign;    // NumChannels * BitsPerSample/8
    u16 BitsPerSample; // bits per sample
};

struct __attribute((packed)) WavSubchunk2 {
    u32 Subchunk2ID;   // "data", 0x61746164
    u32 Subchunk2Size; // NumSamples * NumChannels * BitsPerSample/8
    u8 Data[];         // actual sound data.
};

struct __attribute__((packed)) WavFile {
    u32 ChunkID;   // "RIFF", 0x46464952
    u32 ChunkSize; // size of the the rest of the chunk following this field
    u32 Format;    // "WAVE", 0x45564157
    u8 SubchunkData[];
};

constexpr u32 MAGIC_CHUNK_ID = 0x46464952;
constexpr u32 MAGIC_FORMAT = 0x45564157;
constexpr u32 MAGIC_SUBCHUNK1_ID = 0x20746d66;
constexpr u32 MAGIC_SUBCHUNK2_ID = 0x61746164;

constexpr u32 SIZE_SUBCHUNK1_PCM = 16;
constexpr u16 AUDIO_FORMAT_PCM = 1;

std::optional<std::tuple<const WavSubchunk1*, const WavSubchunk2*>>
check_file(const WavFile* wav_file, size_t file_size) {
    if (file_size <= sizeof(WavFile)) {
        return std::nullopt;
    }

    if (wav_file->ChunkID != MAGIC_CHUNK_ID ||
        wav_file->Format != MAGIC_FORMAT) {
        return std::nullopt;
    }

    if (wav_file->ChunkSize + sizeof(WavUnknownSubchunk) > file_size) {
        return std::nullopt;
    }

    const u8* const file_end =
        reinterpret_cast<const u8*>(wav_file) + file_size;

    const u8* cursor = wav_file->SubchunkData;
    std::optional<const WavSubchunk1*> wav_subchunk1;
    std::optional<const WavSubchunk2*> wav_subchunk2;

    while (1) {
        if (cursor + sizeof(WavUnknownSubchunk) > file_end) {
            break;
        }

        const WavUnknownSubchunk* const subchunk =
            reinterpret_cast<const WavUnknownSubchunk*>(cursor);
        if (cursor + sizeof(WavUnknownSubchunk) + subchunk->SubchunkSize >
            file_end) {
            // Don't treat incomplete last chunk as an error.
            break;
        }

        switch (subchunk->SubchunkID) {
        case MAGIC_SUBCHUNK1_ID: {
            if (wav_subchunk1) {
                // duplicated chunk
                return std::nullopt;
            }

            if (subchunk->SubchunkSize != SIZE_SUBCHUNK1_PCM) {
                return std::nullopt;
            }

            const WavSubchunk1& subchunk1 =
                *reinterpret_cast<const WavSubchunk1*>(cursor);

            if (subchunk1.AudioFormat != AUDIO_FORMAT_PCM) {
                return std::nullopt;
            }

            if (subchunk1.NumChannels != 1 && subchunk1.NumChannels != 2) {
                return std::nullopt;
            }

            if (subchunk1.BitsPerSample != 8 && subchunk1.BitsPerSample != 16) {
                return std::nullopt;
            }

            if (subchunk1.ByteRate != subchunk1.SampleRate *
                                          subchunk1.NumChannels *
                                          (subchunk1.BitsPerSample / 8)) {
                return std::nullopt;
            }

            if (subchunk1.BlockAlign !=
                subchunk1.NumChannels * (subchunk1.BitsPerSample / 8)) {
                return std::nullopt;
            }

            wav_subchunk1 = &subchunk1;
        } break;

        case MAGIC_SUBCHUNK2_ID: {
            if (wav_subchunk2) {
                // duplicated chunk
                return std::nullopt;
            }

            wav_subchunk2 = reinterpret_cast<const WavSubchunk2*>(cursor);
        } break;

        default:
            // Ignore any unknown chunks.
            break;
        }

        cursor += sizeof(WavUnknownSubchunk) + subchunk->SubchunkSize;
    }

    if (!wav_subchunk1 || !wav_subchunk2) {
        return std::nullopt;
    }

    return {{*wav_subchunk1, *wav_subchunk2}};
}

} /* namespace wav */

class Sfx {
    size_t m_size{};
    ctru::linear::ptr<const u8[]> m_data{};

    wav::WavSubchunk1 m_wav_info;

public:
    bool load(std::filesystem::path path) {
#ifdef TRACY_ENABLE
        ZoneScopedC(tracy::Color::Lime);
        auto filename = path.filename().string();
        ZoneText(filename.c_str(), filename.size());
#endif

        std::fstream fs(path, std::ios::binary | std::ios::in);
        if (!fs.is_open()) {
            return false;
        }

        fs.seekg(0, std::ios::end);
        auto len = fs.tellg();
        if (len <= sizeof(wav::WavFile)) {
            return false;
        }
        fs.seekg(0, std::ios::beg);

        std::vector<u8> file_data(len);
        fs.read((char*)file_data.data(), len);
        if (fs.gcount() != len) {
            printf(
                "Failed to read file %s (expected size: %d, actual size: %d)\n",
                path.c_str(), (int)len, fs.gcount());
            return false;
        }

        const wav::WavFile* wav_file = (const wav::WavFile*)file_data.data();
        auto wav_checked = wav::check_file(wav_file, len);
        if (!wav_checked) {
            return false;
        }
        const wav::WavSubchunk1* wav_subchunk1;
        const wav::WavSubchunk2* wav_subchunk2;
        std::tie(wav_subchunk1, wav_subchunk2) = *wav_checked;

        size_t audio_size = wav_subchunk2->Subchunk2Size;

        ctru::linear::ptr<u8[]> audio_data =
            ctru::linear::make_ptr_nothrow_for_overwrite<u8[]>(audio_size);

        memcpy(audio_data.get(), wav_subchunk2->Data, audio_size);

        this->m_size = audio_size;
        this->m_data = std::move(audio_data);
        this->m_wav_info = *wav_subchunk1;
        return true;
    }

    inline void unload() {
        m_size = 0;
        m_data.reset();
        m_wav_info = {};
    }

    constexpr operator bool() const { return m_size != 0 && m_data; }

    constexpr u16 num_channels() const { return m_wav_info.NumChannels; }

    constexpr u32 sample_rate() const { return m_wav_info.SampleRate; }

    constexpr u16 bits_per_sample() const { return m_wav_info.BitsPerSample; }

    inline std::pair<const u8*, size_t> audio_data() const {
        return {m_data.get(), m_size};
    }
};

class MusicMod {
    size_t m_size{};
    std::unique_ptr<const u8[]> m_data{};

public:
    bool load(std::filesystem::path path) {
#ifdef TRACY_ENABLE
        ZoneScopedC(tracy::Color::Lime);
        auto filename = path.filename().string();
        ZoneText(filename.c_str(), filename.size());
#endif

        std::fstream fs(path, std::ios::binary | std::ios::in);
        if (!fs.is_open()) {
            return false;
        }

        fs.seekg(0, std::ios::end);
        auto len = fs.tellg();
        if (len <= 0) {
            return false;
        }
        fs.seekg(0, std::ios::beg);

        std::unique_ptr<u8[]> file_data = std::make_unique<u8[]>(len);
        fs.read((char*)file_data.get(), len);
        if (fs.gcount() != len) {
            printf(
                "Failed to read file %s (expected size: %d, actual size: %d)\n",
                path.c_str(), (int)len, fs.gcount());
            return false;
        }

        if (xmp_test_module_from_memory(file_data.get(), len, nullptr) != 0) {
            fprintf(stderr, "File '%s' is not a valid music file module.\n",
                    path.c_str());
            return false;
        }

        this->m_size = len;
        this->m_data = std::move(file_data);
        return true;
    }

    inline void unload() {
        m_size = 0;
        m_data.reset();
    }

    constexpr operator bool() const { return m_size != 0 && m_data; }

    inline std::pair<const u8*, size_t> module_data() const {
        return {m_data.get(), m_size};
    }
};

enum class MusicStreamedFormats {
    Invalid,
    Vorbis,
    Opus,
    Mpg123,
};

class MusicStreamed {
    std::filesystem::path m_path{};
    MusicStreamedFormats m_format{};

public:
    void load(std::filesystem::path path, MusicStreamedFormats format) {
        m_path = std::move(path);
        m_format = format;
    }

    inline void unload() {
        m_path.clear();
        m_format = MusicStreamedFormats::Invalid;
    }

    inline operator bool() const {
        return !m_path.empty() && m_format != MusicStreamedFormats::Invalid;
    }

    inline const std::filesystem::path& path() const { return m_path; }

    inline MusicStreamedFormats format() const { return m_format; }
};

class MusicSource {
public:
    virtual ~MusicSource() = default;
    virtual void unload() = 0;
    virtual void set_loop_count(int loop) = 0;
    /**
     * Change the playback rate. Returns true if the music source supports
     * true tempo changes; returns false otherwise to indicate that the source
     * fakes playback rate change by scaling the sample rate.
     */
    virtual bool set_playback_rate(float rate) = 0;
    /**
     * Get the sample rate of the upcoming samples filled by `get_samples`. This
     * is allowed to change mid-file.
     */
    virtual int get_sample_rate() = 0;
    /**
     * Get whether the upcoming samples filled by `get_samples` is stereo. This
     * is allowed to change mid-file.
     */
    virtual bool get_is_stereo() = 0;
    virtual size_t get_samples(u8* buffer, size_t len) = 0;
};

class MusicSourceXMP final : public MusicSource {
    xmp_context m_xmp_ctx;
    int m_loop_count;

public:
    inline MusicSourceXMP() : m_xmp_ctx{xmp_create_context()} {}

    ~MusicSourceXMP() override { xmp_free_context(m_xmp_ctx); }

    MusicSourceXMP(const MusicSourceXMP&) = delete;
    MusicSourceXMP(MusicSourceXMP&&) = delete;
    MusicSourceXMP& operator=(const MusicSourceXMP&) = delete;
    MusicSourceXMP& operator=(MusicSourceXMP&&) = delete;

    bool try_load(const void* data, size_t len);
    void unload() override;
    void set_loop_count(int loop) override;
    bool set_playback_rate(float rate) override;
    int get_sample_rate() override;
    bool get_is_stereo() override;
    size_t get_samples(u8* buffer, size_t len) override;
};

bool MusicSourceXMP::try_load(const void* data, size_t len) {
    if (xmp_load_module_from_memory(m_xmp_ctx, data, len) != 0) {
        fprintf(stderr, "Failed to load song module\n");
        return false;
    }

    // Use native 3DS DSP sampling rate.
    if (xmp_start_player(m_xmp_ctx, NDSP_SAMPLE_RATE, 0) != 0) {
        fprintf(stderr, "Failed to start player to play song module\n");
        xmp_stop_module(m_xmp_ctx);
        return false;
    }

    return true;
}

void MusicSourceXMP::unload() {
    xmp_end_player(m_xmp_ctx);
    xmp_stop_module(m_xmp_ctx);
}

void MusicSourceXMP::set_loop_count(int loop) { m_loop_count = loop; }

bool MusicSourceXMP::set_playback_rate(float rate) {
    xmp_set_tempo_factor(m_xmp_ctx, 1.0 / rate);
    return true;
}

int MusicSourceXMP::get_sample_rate() { return NDSP_SAMPLE_RATE; }

bool MusicSourceXMP::get_is_stereo() { return true; }

size_t MusicSourceXMP::get_samples(u8* buffer, size_t len) {
    auto res = xmp_play_buffer(m_xmp_ctx, buffer, len, m_loop_count);
    if (res == -XMP_END) {
        return 0;
    } else if (res != 0) {
        fprintf(stderr, "Failed to continue playing song.\n");
        return 0;
    }
    return len;
}

class MusicSourceVorbis final : public MusicSource {
    OggVorbis_File m_vorbis_file{};
    float m_playback_rate;
    int m_current_bitstream{0};
    ogg_int64_t m_current_bitstream_end_pos{};
    std::array<char, 65536> m_filebuf;

public:
    inline MusicSourceVorbis() {}

    ~MusicSourceVorbis() override { ov_clear(&m_vorbis_file); }

    MusicSourceVorbis(const MusicSourceVorbis&) = delete;
    MusicSourceVorbis(MusicSourceVorbis&&) = delete;
    MusicSourceVorbis& operator=(const MusicSourceVorbis&) = delete;
    MusicSourceVorbis& operator=(MusicSourceVorbis&&) = delete;

    bool try_load(const std::filesystem::path& path);
    void unload() override;
    void set_loop_count(int loop) override;
    bool set_playback_rate(float rate) override;
    int get_sample_rate() override;
    bool get_is_stereo() override;
    size_t get_samples(u8* buffer, size_t len) override;

    static constexpr MusicStreamedFormats FORMAT = MusicStreamedFormats::Vorbis;

    static bool test(const std::filesystem::path& path);
};

bool MusicSourceVorbis::test(const std::filesystem::path& path) {
#ifdef TRACY_ENABLE
    ZoneScopedNC("MusicSourceVorbis::test", tracy::Color::Lime);
    auto filename = path.filename().string();
    ZoneText(filename.c_str(), filename.size());
#endif

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", path.c_str());
        return false;
    }
    setvbuf(fp, nullptr, _IOFBF, 65536);

    OggVorbis_File vf;
    auto res = ov_test(fp, &vf, nullptr, 0);
    if (res != 0) {
        fprintf(stderr, "Vorbis open failed: %d\n", res);
        fclose(fp);
        return false;
    }

    ov_clear(&vf);
    return true;
}

bool MusicSourceVorbis::try_load(const std::filesystem::path& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", path.c_str());
        return false;
    }
    setvbuf(fp, m_filebuf.data(), _IOFBF, m_filebuf.size());

    // If `ov_open` succeeded, `ov_clear` closes the file for us.
    auto res = ov_open(fp, &m_vorbis_file, nullptr, 0);
    if (res != 0) {
        fprintf(stderr, "Vorbis open failed: %d\n", res);
        fclose(fp);
        return false;
    }

    m_current_bitstream = 0;
    m_current_bitstream_end_pos =
        ov_pcm_total(&m_vorbis_file, m_current_bitstream);

#ifndef NDEBUG
    const vorbis_info* info = ov_info(&m_vorbis_file, 0);
    printf("bitstream %d rate %lu channels %d\n", 0, info->rate,
           info->channels);
    printf("bitstream end pos: %lld\n", m_current_bitstream_end_pos);
#endif

    return true;
}

void MusicSourceVorbis::unload() { ov_clear(&m_vorbis_file); }

void MusicSourceVorbis::set_loop_count(int loop) {
    // TODO
}

bool MusicSourceVorbis::set_playback_rate(float rate) {
    m_playback_rate = rate;
    return false;
}

int MusicSourceVorbis::get_sample_rate() {
    const vorbis_info* info = ov_info(&m_vorbis_file, m_current_bitstream);
    return info->rate * m_playback_rate;
}

bool MusicSourceVorbis::get_is_stereo() {
    const vorbis_info* info = ov_info(&m_vorbis_file, m_current_bitstream);
    return info->channels == 2;
}

size_t MusicSourceVorbis::get_samples(u8* buffer, size_t len) {
    const vorbis_info* info = ov_info(&m_vorbis_file, 0);
    if (info->channels > 2) {
        fprintf(stderr, "Files with more than 2 channels are not supported\n.");
        return 0;
    }

    size_t totalRead = 0;
    int bitstream;
    do {
        long read = ov_read(&m_vorbis_file, (char*)buffer + totalRead,
                            len - totalRead, &bitstream);
        if (read == 0)
            break;
        if (read < 0) {
            fprintf(stderr, "Vorbis error decoding samples: %ld\n", read);
            break;
        }

        if (bitstream != m_current_bitstream) {
            fprintf(stderr, "Unexpected change of vorbis bitstream.\n");
            return 0;
        }

        totalRead += read;

        auto current_pcm = ov_pcm_tell(&m_vorbis_file);
        if (current_pcm >= m_current_bitstream_end_pos) {
#ifndef NDEBUG
            printf("Reached end of bitstream %d at %lld\n", bitstream,
                   current_pcm);
#endif
            if (m_current_bitstream + 1 < m_vorbis_file.links) {
                m_current_bitstream++;
                m_current_bitstream_end_pos +=
                    ov_pcm_total(&m_vorbis_file, m_current_bitstream);
            } else {
#ifndef NDEBUG
                printf("End of file\n");
#endif
            }
            break;
        }

    } while (totalRead < len);

    return totalRead;
}

class MusicSourceOpus final : public MusicSource {
    struct Opus_Deleter {
        void operator()(OggOpusFile* ptr) { op_free(ptr); }
    };

    std::unique_ptr<OggOpusFile, Opus_Deleter> m_opus_file{};
    float m_playback_rate;
    int m_current_bitstream{0};
    ogg_int64_t m_current_bitstream_end_pos{};
    std::array<char, 65536> m_filebuf;

public:
    inline MusicSourceOpus() {}

    ~MusicSourceOpus() override {}

    MusicSourceOpus(const MusicSourceOpus&) = delete;
    MusicSourceOpus(MusicSourceOpus&&) = delete;
    MusicSourceOpus& operator=(const MusicSourceOpus&) = delete;
    MusicSourceOpus& operator=(MusicSourceOpus&&) = delete;

    bool try_load(const std::filesystem::path& path);
    void unload() override;
    void set_loop_count(int loop) override;
    bool set_playback_rate(float rate) override;
    int get_sample_rate() override;
    bool get_is_stereo() override;
    size_t get_samples(u8* buffer, size_t len) override;

    static constexpr MusicStreamedFormats FORMAT = MusicStreamedFormats::Opus;

    static bool test(const std::filesystem::path& path);
};

bool MusicSourceOpus::test(const std::filesystem::path& path) {
#ifdef TRACY_ENABLE
    ZoneScopedNC("MusicSourceOpus::test", tracy::Color::Lime);
    auto filename = path.filename().string();
    ZoneText(filename.c_str(), filename.size());
#endif

    int error;
    OpusFileCallbacks cb;
    FILE* fp = (FILE*)op_fopen(&cb, path.c_str(), "rb");
    setvbuf(fp, nullptr, _IOFBF, 65536);
    std::unique_ptr<OggOpusFile, Opus_Deleter> opus_file{
        op_test_callbacks(fp, &cb, nullptr, 0, &error)};
    if (!opus_file) {
        fprintf(stderr, "OpusFile open failed: %d\n", error);
        fclose(fp);
        return false;
    }

    return true;
}

bool MusicSourceOpus::try_load(const std::filesystem::path& path) {
    // Notes on file buffering:
    // Reading from sdmc in HOS has very high overhead. A single small
    // unbuffered `fread` can take an average of 10 ms, sometimes up to 15 ms.
    // The worst part is that dka/libctru `fopen` has no buffering by default.
    // Therefore, calling `setvbuf` is after fopen is a must. A 64k buffer seems
    // appropriate.

    int error;
    OpusFileCallbacks cb;
    FILE* fp = (FILE*)op_fopen(&cb, path.c_str(), "rb");
    setvbuf(fp, m_filebuf.data(), _IOFBF, m_filebuf.size());
    m_opus_file.reset(op_open_callbacks(fp, &cb, nullptr, 0, &error));
    if (!m_opus_file) {
        fprintf(stderr, "OpusFile open failed: %d\n", error);
        fclose(fp);
        return false;
    }

    error = op_set_gain_offset(m_opus_file.get(), OP_TRACK_GAIN, 0);
    if (error < 0) {
#ifndef NDEBUG
        fprintf(stderr, "OpusFile set gain offset error: %d\n", error);
#endif
    }

    m_current_bitstream = 0;
    m_current_bitstream_end_pos =
        op_pcm_total(m_opus_file.get(), m_current_bitstream);

#ifndef NDEBUG
    int channels = op_channel_count(m_opus_file.get(), m_current_bitstream);
    printf("bitstream %d channels %d\n", 0, channels);
    printf("bitstream end pos: %lld\n", m_current_bitstream_end_pos);
#endif

    return true;
}

void MusicSourceOpus::unload() { m_opus_file.reset(); }

void MusicSourceOpus::set_loop_count(int loop) {
    // TODO
}

bool MusicSourceOpus::set_playback_rate(float rate) {
    m_playback_rate = rate;
    return false;
}

int MusicSourceOpus::get_sample_rate() { return 48000 * m_playback_rate; }

bool MusicSourceOpus::get_is_stereo() {
    int channels = op_channel_count(m_opus_file.get(), m_current_bitstream);
    return channels == 2;
}

size_t MusicSourceOpus::get_samples(u8* buffer, size_t len) {
    int channels = op_channel_count(m_opus_file.get(), m_current_bitstream);
    if (channels > 2) {
        fprintf(stderr, "Files with more than 2 channels are not supported\n.");
        return 0;
    }

    size_t totalRead = 0;
    int bitstream;
    do {
        // Note: op_read request buf_size in total samples count, but returns
        // the samples count _per channel_.
        int read_samples_per_channel =
            op_read(m_opus_file.get(), (opus_int16*)(buffer + totalRead),
                    (len - totalRead) / 2, &bitstream);
        if (read_samples_per_channel == 0)
            break;
        if (read_samples_per_channel < 0) {
            fprintf(stderr, "OpusFile error decoding samples: %d\n",
                    read_samples_per_channel);
            break;
        }

        if (bitstream != m_current_bitstream) {
            fprintf(stderr, "Unexpected change of vorbis bitstream.\n");
            return 0;
        }

        size_t read = (size_t)read_samples_per_channel * (size_t)channels * 2u;
        totalRead += read;

        auto current_pcm = op_pcm_tell(m_opus_file.get());
        if (current_pcm >= m_current_bitstream_end_pos) {
#ifndef NDEBUG
            printf("Reached end of bitstream %d at %lld\n", bitstream,
                   current_pcm);
#endif
            if (m_current_bitstream + 1 < op_link_count(m_opus_file.get())) {
                m_current_bitstream++;
                m_current_bitstream_end_pos +=
                    op_pcm_total(m_opus_file.get(), m_current_bitstream);
            } else {
#ifndef NDEBUG
                printf("End of file\n");
#endif
            }
            break;
        }

    } while (totalRead < len);

    return totalRead;
}

class MusicSourceMpg123 final : public MusicSource {
    struct Mpg123_Deleter {
        void operator()(mpg123_handle* ptr) { mpg123_delete(ptr); }
    };

    std::unique_ptr<mpg123_handle, Mpg123_Deleter> m_mh{};
    float m_playback_rate;
    std::array<char, 65536> m_filebuf;

    static void configure_mpg123(mpg123_handle* mh) {
        auto res = mpg123_param2(mh, MPG123_ADD_FLAGS, MPG123_NO_RESYNC, 0);
        if (res != MPG123_OK) {
            fprintf(stderr, "Failed to set mpg123 flags, err: %d\n", res);
        }

        res = mpg123_param2(mh, MPG123_RVA, MPG123_RVA_MIX, 0);
        if (res != MPG123_OK) {
            fprintf(stderr, "Failed to set mpg123 rva, err: %d\n", res);
        }

        mpg123_format_none(mh);
        res = mpg123_format2(mh, 0, MPG123_MONO, MPG123_ENC_SIGNED_16);
        if (res != MPG123_OK) {
            fprintf(stderr, "Failed to set mpg123 format %d, err: %d\n", 0,
                    res);
        }
        res = mpg123_format2(mh, 0, MPG123_STEREO, MPG123_ENC_SIGNED_16);
        if (res != MPG123_OK) {
            fprintf(stderr, "Failed to set mpg123 format %d, err: %d\n", 1,
                    res);
        }

        static const auto my_read = [](void* fp, void* buf,
                                       size_t nbyte) -> ssize_t {
            auto res = fread(buf, 1, nbyte, (FILE*)fp);
            if (res == 0 && ferror((FILE*)fp)) {
                return -1;
            }
            return res;
        };
        static const auto my_lseek = [](void* fp, off_t offset,
                                        int whence) -> off_t {
            auto res = fseek((FILE*)fp, offset, whence);
            if (res != 0) {
                return -1;
            }
            return ftell((FILE*)fp);
        };
        static const auto my_cleanup = [](void* fp) { fclose((FILE*)fp); };

        res = mpg123_replace_reader_handle(mh, my_read, my_lseek, my_cleanup);
        if (res != MPG123_OK) {
            fprintf(stderr, "Failed to set mpg123 reader handle, err: %d\n",
                    res);
        }
    }

public:
    inline MusicSourceMpg123() : m_mh(mpg123_new(nullptr, nullptr)) {
        configure_mpg123(m_mh.get());
    }

    ~MusicSourceMpg123() override {}

    MusicSourceMpg123(const MusicSourceMpg123&) = delete;
    MusicSourceMpg123(MusicSourceMpg123&&) = delete;
    MusicSourceMpg123& operator=(const MusicSourceMpg123&) = delete;
    MusicSourceMpg123& operator=(MusicSourceMpg123&&) = delete;

    bool try_load(const std::filesystem::path& path);
    void unload() override;
    void set_loop_count(int loop) override;
    bool set_playback_rate(float rate) override;
    int get_sample_rate() override;
    bool get_is_stereo() override;
    size_t get_samples(u8* buffer, size_t len) override;

    static constexpr MusicStreamedFormats FORMAT = MusicStreamedFormats::Mpg123;

    static bool test(const std::filesystem::path& path);
};

bool MusicSourceMpg123::test(const std::filesystem::path& path) {
#ifdef TRACY_ENABLE
    ZoneScopedNC("MusicSourceMpg123::test", tracy::Color::Lime);
    auto filename = path.filename().string();
    ZoneText(filename.c_str(), filename.size());
#endif

    // XXX: Maybe refactor to not need to new/delete every single time?

    std::unique_ptr<mpg123_handle, Mpg123_Deleter> mh{
        mpg123_new(nullptr, nullptr)};

    configure_mpg123(mh.get());

    auto res =
        mpg123_param2(mh.get(), MPG123_ADD_FLAGS, MPG123_NO_FRANKENSTEIN, 0);
    if (res != MPG123_OK) {
        fprintf(stderr, "mpg123 param failed, err: %d\n", res);
        return false;
    }

    // We feed mpg123 manually in 64k blocks. Otherwise if we let mpg123 read
    // the file directly, it decides to do single-byte reads when it sees an
    // invalid file, which can take a couple of minutes before bailing out.
    // Even using fopen and a 64k buffer is still 5x slower than feeding.

    res = mpg123_open_feed(mh.get());
    if (res != MPG123_OK) {
        fprintf(stderr, "mpg123 open failed, err: %d\n", res);
        return false;
    }

    std::fstream fs(path, std::ios::binary | std::ios::in);
    if (!fs.is_open()) {
        return false;
    }

    std::unique_ptr<u8[]> file_data = std::make_unique<u8[]>(65536);
    while (true) {
        fs.read((char*)file_data.get(), 65536);
        if (fs.gcount() == 0) {
#ifndef NDEBUG
            printf("End of file\n");
#endif
            return false;
        }

        res = mpg123_feed(mh.get(), file_data.get(), fs.gcount());
        if (res != MPG123_OK) {
            fprintf(stderr, "Failed to feed mpg123 data, err: %d\n", res);
            return false;
        }

        long rate;
        int channels;
        int encoding;
        res = mpg123_getformat2(mh.get(), &rate, &channels, &encoding, false);
        if (res == MPG123_OK) {
            break;
        } else if (res != MPG123_NEED_MORE) {
            fprintf(stderr, "Failed to get mpg123 format, err: %d\n", res);
            return false;
        }
    }

    return true;
}

bool MusicSourceMpg123::try_load(const std::filesystem::path& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    setvbuf(fp, m_filebuf.data(), _IOFBF, m_filebuf.size());
    auto res = mpg123_open_handle(m_mh.get(), fp);
    if (res != MPG123_OK) {
        fprintf(stderr, "mpg123 open failed, err: %d\n", res);
        fclose(fp);
        return false;
    }

    return true;
}

void MusicSourceMpg123::unload() { mpg123_close(m_mh.get()); }

void MusicSourceMpg123::set_loop_count(int loop) {
    // TODO
}

bool MusicSourceMpg123::set_playback_rate(float rate) {
    m_playback_rate = rate;
    return false;
}

int MusicSourceMpg123::get_sample_rate() {
    long rate;
    int channels;
    int encoding;
    auto res = mpg123_getformat2(m_mh.get(), &rate, &channels, &encoding, true);
    if (res != MPG123_OK) {
        fprintf(stderr, "Failed to get format, err: %d\n", res);
        return NDSP_SAMPLE_RATE;
    }
    return rate * m_playback_rate;
}

bool MusicSourceMpg123::get_is_stereo() {
    long rate;
    int channels;
    int encoding;
    auto res =
        mpg123_getformat2(m_mh.get(), &rate, &channels, &encoding, false);
    if (res != MPG123_OK) {
        return NDSP_SAMPLE_RATE;
    }
    return channels == MPG123_STEREO;
}

size_t MusicSourceMpg123::get_samples(u8* buffer, size_t len) {
    size_t totalRead = 0;
    int bitstream;
    do {
        size_t read = 0;
        auto res =
            mpg123_read(m_mh.get(), buffer + totalRead, len - totalRead, &read);
        if (res != MPG123_OK && res != MPG123_NEW_FORMAT) {
            if (res == MPG123_DONE) {
                break;
            }
            fprintf(stderr, "mpg123 read error: %d\n", res);
            return 0;
        }
        if (read == 0)
            break;

        totalRead += read;

        if (res == MPG123_NEW_FORMAT) {
#ifndef NDEBUG
            printf("MPG123_NEW_FORMAT\n");
#endif
            break;
        }
    } while (totalRead < len);

    return totalRead;
}

class NdspAudioThread {
    // The inter-thread communication state. Only the data in this struct
    // should be touched from any non-audio threads.
    struct State {
        Thread thread;

        LightEvent in_event{};
        std::atomic<bool> in_run{};

        std::atomic<bool> in_stop_song{};
        std::atomic<bool> in_start_song{};
        int in_start_song_arg_song_id{};
        bool in_start_song_arg_loop{};

        std::atomic<float> in_music_playback_rate{1.0};
        static_assert(std::atomic<float>::is_always_lock_free);

        std::atomic<bool> out_song_ended{};
    };
    State m_state;

    // The following fields must only be accessed from the audio thread.

    std::optional<MusicSourceXMP> m_mus_xmp{};
    std::optional<MusicSourceVorbis> m_mus_vorbis{};
    std::optional<MusicSourceOpus> m_mus_opus{};
    std::optional<MusicSourceMpg123> m_mus_mpg123{};
    MusicSource* m_mus_current{nullptr};
    int m_current_sample_rate{};
    bool m_current_is_stereo{};
    float m_music_playback_rate{1.f};
    int m_loop_count{};

    void audio_thread();
    static void audio_thread_start(void* ctx) {
        ((NdspAudioThread*)ctx)->audio_thread();
    }

    bool start_song(int song_id, bool loop);
    void stop_song();

    bool fill_buffer();
    bool fill_buffer_impl();

    static void ndsp_callback(void* ctx);

    NdspAudioThread() = default;
    ~NdspAudioThread() = default;

    NdspAudioThread(const NdspAudioThread&) = delete;
    NdspAudioThread(NdspAudioThread&&) = delete;
    auto operator=(const NdspAudioThread&) = delete;
    auto operator=(NdspAudioThread&&) = delete;

    static std::atomic<NdspAudioThread*> s_instance;

public:
    static bool start_thread();
    static void join_thread();

    static void send_start_song(int song_id, bool loop);
    static void send_stop_song();
    static void send_set_playback_rate(float rate);

    static bool recv_song_ended();
};

std::atomic<NdspAudioThread*> NdspAudioThread::s_instance{};

enum class SoundEngine {
    NONE,
    NDSP, // main audio support
    CSND, // provides rudimentary sfx as a fallback
};

enum ChannelsCSND : int {
    CH_CSND_SFX = 8,
};

enum ChannelsNDSP : int {
    CH_NDSP_SFX_FIRST = 0,
    CH_NDSP_SFX_LAST =
        CH_NDSP_SFX_FIRST + 8 - 1, // 8 channels for sfx ought to be enough???
    CH_NDSP_MUSIC,
};

constexpr int MUSIC_SAMPLING_RATE =
    NDSP_SAMPLE_RATE; // native 3DS DSP sampling rate.
constexpr int MUSIC_NUM_CHANNELS = 2;
constexpr int MUSIC_BITS_PER_SAMPLE = 16;
// 0.1-second buffer
constexpr size_t MUSIC_BUFFER_SAMPLE_COUNT = MUSIC_SAMPLING_RATE * 1 / 10;
constexpr size_t MUSIC_BUFFER_SIZE = MUSIC_BUFFER_SAMPLE_COUNT *
                                     MUSIC_NUM_CHANNELS *
                                     (MUSIC_BITS_PER_SAMPLE / 8);

static SoundEngine sound_engine = SoundEngine::NONE;

static std::array<Sfx, SFX_COUNT> sound_effects;

static std::array<ndspWaveBuf, CH_NDSP_SFX_LAST - CH_NDSP_SFX_FIRST + 1>
    ndsp_wave_buf_sfx{};
static std::list<int> sfx_use_order{};

// Buffers for storing the music output, two of them to allow flipping
// (therefore size is `MUSIC_BUFFER_SIZE * 2`).
static ctru::linear::ptr<u8[]> music_audio_buf{};
static std::array<ndspWaveBuf, 2> ndsp_wave_buf_music{};
static int music_next_buf_idx = 0;

static float music_volume = 1.f;

static std::vector<std::variant<MusicMod, MusicStreamed>> song_list{};
static std::vector<std::string> songNameList;

Songs songs;

inline float sfx_volume() { return savefile->settings.sfxVolume / 10.f; }

// Game engine functions:

void loadAudio(std::string pathPrefix) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    std::filesystem::path prefix{pathPrefix};

    std::filesystem::path assets_path{prefix / "assets"};
    std::error_code ec;
    std::filesystem::directory_iterator dir_it{assets_path, ec};
    if (ec) {
        errorConf err;
        errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
        errorText(&err,
                  "Failed to load audio assets. Please make sure the files has "
                  "been placed on the SD card under `" DATA_DIR_BASE
                  "/assets`.");
        errorDisp(&err);
        return;
    }

    Result res = ndspInit();
    if (R_FAILED(res)) {
        if (R_DESCRIPTION(res) == RD_NOT_FOUND) {
            fprintf(stderr, "Failed to initialize DSP, firmware not found.\n");
        } else {
            fprintf(stderr, "Failed to initialize DSP, err: 0x%" PRIx32 "\n",
                    res);
        }
        errorConf err;
        errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
        errorText(
            &err,
            "Failed to initialize DSP. Game audio will be limited. Please make "
            "sure you have the DSP firmware available.\n\nFor more "
            "information, see https://3ds.hacks.guide/finalizing-setup.html");
        errorDisp(&err);

        Result res = csndInit();
        if (R_FAILED(res)) {
            errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
            errorText(&err, "Failed to initialize CSND. Game audio will be "
                            "fully disabled.");
            errorDisp(&err);
            return;
        }

        sound_engine = SoundEngine::CSND;
    } else {
        sound_engine = SoundEngine::NDSP;
    }

    int failed_sfx_count = 0;
    for (int i = 0; i < SFX_COUNT; i++) {
        auto& sfx = sound_effects[i];
        auto file = prefix / std::string{SoundEffectPaths[i]};
        if (!sfx.load(file.c_str())) {
            failed_sfx_count++;
            fprintf(stderr, "Failed to load sfx '%s'\n", file.c_str());
            continue;
        }

        fprintf(stderr,
                "Loaded sfx '%s' - channels: %d, sample rate: %d, bits per "
                "sample: %d\n",
                file.c_str(), sfx.num_channels(), (int)sfx.sample_rate(),
                sfx.bits_per_sample());
    }

    if (failed_sfx_count > 0) {
        errorConf err;
        errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
        std::array<char, 1536> buf;
        char* cursor = buf.begin();
        int result = snprintf(
            cursor, buf.end() - cursor,
            "Failed to load %d sfx audio assets. Please make sure the "
            "files has been placed on the SD card under `" DATA_DIR_BASE
            "/assets`, and that they are in the correct file format.\n\n",
            failed_sfx_count);
        if (result > 0) {
            cursor += result;
        }
        for (int i = 0; i < SFX_COUNT; i++) {
            if (!sound_effects[i]) {
                result = snprintf(cursor, buf.end() - cursor, "- %s\n",
                                  SoundEffectPaths[i]);
                if (result > 0) {
                    cursor += result;
                    if (cursor >= buf.end() - 1) {
                        // Not enough room.
                        cursor = buf.end() - 1;
                        break;
                    }
                    failed_sfx_count--;
                }
            }
        }
        if (failed_sfx_count > 0) {
            // Not enough room to list all missing SFX files, so replace last
            // 3 characters with an ellipsis.
            for (int i = 1; i <= 3; i++) {
                *(cursor - i) = '.';
            }
        }
        errorText(&err, buf.data());
        errorDisp(&err);
    }

    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    // NDSP-specific initialization:

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    for (int i = CH_NDSP_SFX_FIRST; i <= CH_NDSP_SFX_LAST; i++) {
        auto& wave_buf = ndsp_wave_buf_sfx[i - CH_NDSP_SFX_FIRST];
        wave_buf.status = NDSP_WBUF_FREE;
        wave_buf.looping = false;

        ndspChnInitParams(i);
        ndspChnSetInterp(i, NDSP_INTERP_POLYPHASE);

        sfx_use_order.push_back(i);
    }

    ndspChnInitParams(CH_NDSP_MUSIC);
    ndspChnSetInterp(CH_NDSP_MUSIC, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(CH_NDSP_MUSIC, MUSIC_SAMPLING_RATE);
    ndspChnSetFormat(CH_NDSP_MUSIC, NDSP_FORMAT_STEREO_PCM16);

    music_audio_buf =
        ctru::linear::make_ptr_nothrow<u8[]>(MUSIC_BUFFER_SIZE * 2);
    for (auto& wave_buf : ndsp_wave_buf_music) {
        wave_buf.status = NDSP_WBUF_FREE;
        wave_buf.looping = false;
        wave_buf.nsamples = MUSIC_BUFFER_SAMPLE_COUNT;
    }

    std::map<std::string, std::variant<MusicMod, MusicStreamed>> sorted_music;

    for (const auto& entry : dir_it) {
        if (!entry.is_regular_file()) {
            continue;
        }

        // Test for module music:
        auto is_module_ext = [](const char* file_ext) {
            for (const auto& ext : {".it", ".mod", ".xm", ".s3m"}) {
                if (strcasecmp(file_ext, ext) == 0) {
                    return true;
                }
            }
            return false;
        };
        if (is_module_ext(entry.path().extension().c_str())) {
            MusicMod mod;
            if (!mod.load(entry.path())) {
                continue;
            }

            auto name = entry.path().filename().string();
#ifndef NDEBUG
            fprintf(stderr, "Found module music '%s'\n", entry.path().c_str());
#endif
            sorted_music.emplace(std::move(name), std::move(mod));
            continue;
        }

        // Test for streamed formats:
        auto is_streamed_ext = [](const char* file_ext) {
            for (const auto& ext : {".ogg", ".opus", ".mp3"}) {
                if (strcasecmp(file_ext, ext) == 0) {
                    return true;
                }
            }
            return false;
        };
        MusicStreamed mus;
        if (strcasecmp(entry.path().extension().c_str(), ".opus") == 0) {
            if (MusicSourceOpus::test(entry.path())) {
                mus.load(entry.path(), MusicSourceOpus::FORMAT);
            }
        } else if (strcasecmp(entry.path().extension().c_str(), ".ogg") == 0) {
            if (MusicSourceVorbis::test(entry.path())) {
                mus.load(entry.path(), MusicSourceVorbis::FORMAT);
            } else if (MusicSourceOpus::test(entry.path())) {
                mus.load(entry.path(), MusicSourceOpus::FORMAT);
            }
        } else if (strcasecmp(entry.path().extension().c_str(), ".mp3") == 0) {
            // Mpg123 should be the last one to test since it is relatively
            // slower to bail out than the other formats.
            if (MusicSourceMpg123::test(entry.path())) {
                mus.load(entry.path(), MusicSourceMpg123::FORMAT);
            }
        }
        if (mus) {
            auto name = entry.path().filename().string();
#ifndef NDEBUG
            fprintf(stderr, "Found streamed music '%s', format: %d\n",
                    entry.path().c_str(), (int)mus.format());
#endif
            sorted_music.emplace(std::move(name), std::move(mus));
            continue;
        }
    }

    song_list.reserve(sorted_music.size());
    songNameList.reserve(sorted_music.size());

    for (auto& [name, mus] : sorted_music) {
        if (name.find("MENU_") != std::string::npos) {
            songs.menu.push_back(song_list.size());
#ifndef NDEBUG
            fprintf(stderr, "Music %zd '%s' is menu %zd\n", song_list.size(),
                    name.c_str(), songs.menu.size());
#endif
        } else {
            songs.game.push_back(song_list.size());
#ifndef NDEBUG
            fprintf(stderr, "Music %zd '%s' is game %zd\n", song_list.size(),
                    name.c_str(), songs.game.size());
#endif
        }
        song_list.emplace_back(std::move(mus));
        std::string asciiName = name;
        std::replace_if(
            asciiName.begin(), asciiName.end(),
            [](char c) { return c < 32 || c >= 127; }, '?');
        songNameList.push_back(std::move(asciiName));
    }

    if (!NdspAudioThread::start_thread()) {
        fprintf(stderr, "Failed to start audio thread\n");
        ndspExit();
        sound_engine = SoundEngine::NONE;
    }
}

void freeAudio() {
    if (sound_engine == SoundEngine::NDSP) {
        NdspAudioThread::join_thread();

        for (int i = CH_NDSP_SFX_FIRST; i <= CH_NDSP_SFX_LAST; i++) {
            ndspChnWaveBufClear(i);
        }
        ndspChnWaveBufClear(CH_NDSP_MUSIC);
    }

    music_audio_buf.reset();

    songs.menu.clear();
    songs.game.clear();
    song_list.clear();

    for (auto& sfx : sound_effects) {
        sfx.unload();
    }

    switch (sound_engine) {
    case SoundEngine::NDSP:
        ndspExit();
        break;
    case SoundEngine::CSND:
        csndExit();
        break;
    case SoundEngine::NONE:
        break;
    }

    sound_engine = SoundEngine::NONE;
}

void audioProcess() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    if (music_volume < 1e-6f) {
        return;
    }

    if (NdspAudioThread::recv_song_ended()) {
        // printf("Song ended.\n");
        if (savefile->settings.cycleSongs == 1) { // CYCLE
            playNextSong();
        } else if (savefile->settings.cycleSongs == 2) { // SHUFFLE
            playSongRandom(currentMenu);
        }
    }
}

void sfx(int n) { sfxRate(n, 1.0); }

void sfxRate(int n, float playback_rate) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    const float volume = sfx_volume();
    if (volume < 1e-6) {
        return;
    }

    const auto& sfx = sound_effects[n];
    if (!sfx) {
        return;
    }

    if (playback_rate <= 0.01) {
        playback_rate = 0.01;
    }

    auto [audio_data, audio_size] = sfx.audio_data();

    switch (sound_engine) {

    case SoundEngine::NDSP: {
        auto use_ch_it = std::find_if(
            sfx_use_order.cbegin(), sfx_use_order.cend(), [](int ch) {
                const auto& wave_buf =
                    ndsp_wave_buf_sfx[ch - CH_NDSP_SFX_FIRST];
                // bool playing = ndspChnIsPlaying(use_ch);
                // printf("channel %d playing: %d, status: %d\n", use_ch,
                // playing, wave_buf.status);
                if (wave_buf.status != NDSP_WBUF_QUEUED &&
                    wave_buf.status != NDSP_WBUF_PLAYING) {
                    return true;
                }
                return false;
            });
        if (use_ch_it == sfx_use_order.cend()) {
            use_ch_it = sfx_use_order.cbegin();
            fprintf(stderr,
                    " No free channel left for sfx %d, replacing channel %d\n",
                    n, *use_ch_it);
            ndspChnWaveBufClear(*use_ch_it);
        }
        int use_ch = *use_ch_it;
        sfx_use_order.splice(sfx_use_order.cend(), sfx_use_order, use_ch_it);
        // printf("Playing sfx %d on channel %d\n", n, use_ch);

        ndspChnSetRate(use_ch, sfx.sample_rate() * playback_rate);

        if (sfx.num_channels() == 1 && sfx.bits_per_sample() == 8) {
            ndspChnSetFormat(use_ch, NDSP_FORMAT_MONO_PCM8);
        } else if (sfx.num_channels() == 1 && sfx.bits_per_sample() == 16) {
            ndspChnSetFormat(use_ch, NDSP_FORMAT_MONO_PCM16);
        } else if (sfx.num_channels() == 2 && sfx.bits_per_sample() == 8) {
            ndspChnSetFormat(use_ch, NDSP_FORMAT_STEREO_PCM8);
        } else if (sfx.num_channels() == 2 && sfx.bits_per_sample() == 16) {
            ndspChnSetFormat(use_ch, NDSP_FORMAT_STEREO_PCM16);
        } else {
            assert(false && "Unsupported SFX format");
            return;
        }

        float mix[12]{volume, volume};
        ndspChnSetMix(use_ch, mix);

        auto& wave_buf = ndsp_wave_buf_sfx[use_ch - CH_NDSP_SFX_FIRST];
        wave_buf.data_vaddr = audio_data;
        wave_buf.looping = false;
        wave_buf.nsamples =
            audio_size / (sfx.num_channels() * (sfx.bits_per_sample() / 8));
        // The ndspWaveBuf must have static lifetime.
        ndspChnWaveBufAdd(use_ch, &wave_buf);
    } break;

    case SoundEngine::CSND: {
        u32 flags = SOUND_CHANNEL(sfx.num_channels()) | SOUND_ONE_SHOT;
        switch (sfx.bits_per_sample()) {
        case 8:
            flags |= SOUND_FORMAT_8BIT;
            break;
        case 16:
            flags |= SOUND_FORMAT_16BIT;
            break;
        default:
            return;
        }
        csndPlaySound(CH_CSND_SFX, flags, sfx.sample_rate() * playback_rate,
                      volume, 0.0, (void*)audio_data, (void*)audio_data,
                      audio_size);
    } break;

    case SoundEngine::NONE:
        break;
    }
}

void startSong(int song, bool loop) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    if (ndspChnIsPaused(CH_NDSP_MUSIC)) {
        ndspChnWaveBufClear(CH_NDSP_MUSIC);
        ndspChnSetPaused(CH_NDSP_MUSIC, false);
    }

    NdspAudioThread::send_start_song(song, loop);
}

void stopSong() {
    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    NdspAudioThread::send_stop_song();
}

void pauseSong() {
    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    ndspChnSetPaused(CH_NDSP_MUSIC, true);
}

void resumeSong() {
    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    ndspChnSetPaused(CH_NDSP_MUSIC, false);
}

void setMusicVolume(int volume) {
    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    music_volume = volume * (1.f / 512.f);

    float mix[12]{music_volume, music_volume};
    ndspChnSetMix(CH_NDSP_MUSIC, mix);
}

void setMusicTempo(int tempo) {
    if (sound_engine != SoundEngine::NDSP) {
        return;
    }

    if (tempo < 1) {
        tempo = 1;
    }

    const float playback_rate = tempo * (1.f / 1024.f);
    NdspAudioThread::send_set_playback_rate(playback_rate);
}

std::string getSongName(int song) { return songNameList[song]; }

// Audio thread functions:

bool NdspAudioThread::start_thread() {
    // This is called on the main thread.
    assert(!s_instance.load());

    auto* inst = new NdspAudioThread;
    s_instance.store(inst, std::memory_order_release);

    inst->m_state.in_run.store(true);
    LightEvent_Init(&inst->m_state.in_event, RESET_ONESHOT);

    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    // Make sure our audio thread has lower priority than the main thread.
    priority += 1;
    priority = std::min(std::max(priority, (s32)0x18), (s32)0x3f);

    // APT_SetAppCpuTimeLimit(20);
    inst->m_state.thread =
        threadCreate(audio_thread_start, inst, 64 * 1024, priority, -1, false);
    if (!inst->m_state.thread) {
        fprintf(stderr, "Failed to start audio thread\n");
        return false;
    }

    return true;
}

void NdspAudioThread::join_thread() {
    // This is called on the main thread.
    auto* inst = s_instance.exchange(nullptr, std::memory_order_acq_rel);
    assert(inst);

    inst->m_state.in_run.store(false, std::memory_order_release);
    LightEvent_Signal(&inst->m_state.in_event);
    threadJoin(inst->m_state.thread, U64_MAX);
    threadFree(inst->m_state.thread);
    delete inst;
}

void NdspAudioThread::send_start_song(int song_id, bool loop) {
    // This is called on the main thread.
    auto* inst = s_instance.load(std::memory_order_relaxed);
    assert(inst);

    if (inst->m_state.in_start_song.exchange(false,
                                             std::memory_order_acq_rel)) {
        fprintf(stderr,
                "NdspAudioThread::send_start_song possible race condition\n");
    }
    if (inst->m_state.in_stop_song.exchange(false, std::memory_order_acq_rel)) {
        fprintf(stderr, "send_start_song Cancelled song stop request\n");
    }
    inst->m_state.in_start_song_arg_song_id = song_id;
    inst->m_state.in_start_song_arg_loop = loop;
    inst->m_state.in_start_song.store(true, std::memory_order_release);
    inst->m_state.out_song_ended.store(false, std::memory_order_relaxed);
}

void NdspAudioThread::send_stop_song() {
    // This is called on the main thread.
    auto* inst = s_instance.load(std::memory_order_relaxed);
    assert(inst);

    if (inst->m_state.in_start_song.exchange(false,
                                             std::memory_order_acq_rel)) {
        fprintf(stderr, "send_stop_song cancelled song start request\n");
    }
    inst->m_state.in_stop_song.store(true, std::memory_order_release);
    inst->m_state.out_song_ended.store(false, std::memory_order_relaxed);
}

void NdspAudioThread::send_set_playback_rate(float rate) {
    // This is called on the main thread.
    auto* inst = s_instance.load(std::memory_order_relaxed);
    assert(inst);

    inst->m_state.in_music_playback_rate.store(rate, std::memory_order_release);
}

bool NdspAudioThread::recv_song_ended() {
    // This is called on the main thread.
    auto* inst = s_instance.load(std::memory_order_relaxed);
    assert(inst);

    return inst->m_state.out_song_ended.exchange(false,
                                                 std::memory_order_acq_rel);
}

void NdspAudioThread::ndsp_callback(void* ctx) {
    // This is called on the libctru NDSP thread.
    if (NdspAudioThread::s_instance.load(std::memory_order_consume) != ctx) {
        return;
    }
    auto* inst = (NdspAudioThread*)ctx;
    LightEvent_Signal(&inst->m_state.in_event);
}

void NdspAudioThread::audio_thread() {
#ifdef TRACY_ENABLE
    tracy::SetThreadName("Ndsp audio thread");
#endif

    m_mus_xmp.emplace();
    m_mus_vorbis.emplace();
    m_mus_opus.emplace();
    m_mus_mpg123.emplace();

    ndspSetCallback(ndsp_callback, this);

    while (m_state.in_run.load(std::memory_order_consume)) {

        if (m_state.in_stop_song.exchange(false, std::memory_order_acq_rel)) {
            stop_song();
            // Do not set m_state.out_song_ended
        }

        if (m_state.in_start_song.load(std::memory_order_acquire)) {
            int song_id = m_state.in_start_song_arg_song_id;
            bool loop = m_state.in_start_song_arg_loop;
            if (m_state.in_start_song.exchange(false,
                                               std::memory_order_acq_rel)) {
                if (start_song(song_id, loop)) {
                    m_state.out_song_ended.store(false,
                                                 std::memory_order_release);
                    // It may have taken us a bit of time to load the song, so
                    // we should jump back to the beginning of the loop to check
                    // for stop song request.
                    continue;
                } else {
                    m_state.out_song_ended.store(true,
                                                 std::memory_order_release);
                }
            } else {
                fprintf(stderr,
                        "NdspAudioThread: new song request cancelled\n");
            }
        }

        if (m_mus_current) {
            float new_playback_rate =
                m_state.in_music_playback_rate.load(std::memory_order_consume);
            if (new_playback_rate != m_music_playback_rate) {
                bool real_tempo =
                    m_mus_current->set_playback_rate(new_playback_rate);

                if (!real_tempo) {
                    // Streamed music may change sample rate to alter speed. If
                    // this is the case, change the ndsp sample rate
                    // immediately, bypassing the normal switchover procedure in
                    // `fill_buffer` (which waits until all buffers have been
                    // drained).

                    int new_sample_rate = m_mus_current->get_sample_rate();

                    int source_sample_rate_old =
                        m_current_sample_rate / m_music_playback_rate;
                    int source_sample_rate_new =
                        new_sample_rate / new_playback_rate;
                    if (source_sample_rate_old == source_sample_rate_new) {

                        ndspChnSetRate(CH_NDSP_MUSIC, new_sample_rate);
                        m_current_sample_rate = new_sample_rate;

#ifndef NDEBUG
                        printf("Playback rate change causes immediate sample "
                               "rate change - sample rate: %d\n",
                               new_sample_rate);
#endif
                    } else {
#ifndef NDEBUG
                        printf("Playback rate change _not_ causing immediate "
                               "sample rate change - source sample rate: %d -> "
                               "%d\n",
                               source_sample_rate_old, source_sample_rate_new);
#endif
                    }
                }

                m_music_playback_rate = new_playback_rate;
            }
        }

        if (m_mus_current) {
            if (!fill_buffer()) {
                // printf("Song ended.\n");
                m_state.out_song_ended.store(true, std::memory_order_release);
            }
        }

        LightEvent_Wait(&m_state.in_event);
    }

    ndspSetCallback(nullptr, nullptr);

    stop_song();
    m_mus_mpg123.reset();
    m_mus_opus.reset();
    m_mus_vorbis.reset();
    m_mus_xmp.reset();
}

bool NdspAudioThread::start_song(int song_id, bool loop) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    stop_song();

    if (MusicMod* song_mod = std::get_if<MusicMod>(&song_list[song_id])) {
        if (!*song_mod) {
            return false;
        }

        auto [mod_data, mod_size] = song_mod->module_data();

        if (!m_mus_xmp->try_load(mod_data, mod_size)) {
            fprintf(stderr, "Failed to load song module %d\n", song_id);
            song_mod->unload();
            return false;
        }

        m_mus_current = &*m_mus_xmp;
    } else if (MusicStreamed* mus =
                   std::get_if<MusicStreamed>(&song_list[song_id])) {
        switch (mus->format()) {
        case MusicStreamedFormats::Vorbis: {
            if (!m_mus_vorbis->try_load(mus->path())) {
                fprintf(stderr, "Failed to load streamed song %d\n", song_id);
                mus->unload();
                return false;
            }

            m_mus_current = &*m_mus_vorbis;
        } break;
        case MusicStreamedFormats::Opus: {
            if (!m_mus_opus->try_load(mus->path())) {
                fprintf(stderr, "Failed to load streamed song %d\n", song_id);
                mus->unload();
                return false;
            }

            m_mus_current = &*m_mus_opus;
        } break;
        case MusicStreamedFormats::Mpg123: {
            if (!m_mus_mpg123->try_load(mus->path())) {
                fprintf(stderr, "Failed to load streamed song %d\n", song_id);
                mus->unload();
                return false;
            }

            m_mus_current = &*m_mus_mpg123;
        } break;
        case MusicStreamedFormats::Invalid:
        default:
            break;
        }
    }

    if (!m_mus_current) {
        return false;
    }

    m_mus_current->set_playback_rate(m_music_playback_rate);
    m_mus_current->set_loop_count(loop ? 0 : 1);

    return true;
}

void NdspAudioThread::stop_song() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    ndspChnWaveBufClear(CH_NDSP_MUSIC);

    if (!m_mus_current)
        return;

    m_mus_current->unload();
    m_mus_current = nullptr;
}

bool NdspAudioThread::fill_buffer() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    auto status = ndsp_wave_buf_music[music_next_buf_idx].status;
    if (status == NDSP_WBUF_QUEUED || status == NDSP_WBUF_PLAYING) {
        // Buffers already full.
        return true;
    }

    int new_sample_rate = m_mus_current->get_sample_rate();
    bool new_is_stereo = m_mus_current->get_is_stereo();
    if (new_sample_rate != m_current_sample_rate ||
        new_is_stereo != m_current_is_stereo) {

        // Wait until the other buffer is also drained, since we do not want to
        // mess with the sample rate or format when there is still existing
        // audio being played...
        status = ndsp_wave_buf_music[(music_next_buf_idx + 1) % 2].status;
        if (status == NDSP_WBUF_QUEUED || status == NDSP_WBUF_PLAYING) {
            return true;
        }

        ndspChnSetRate(CH_NDSP_MUSIC, new_sample_rate);
        if (new_is_stereo) {
            ndspChnSetFormat(CH_NDSP_MUSIC, NDSP_FORMAT_STEREO_PCM16);
        } else {
            ndspChnSetFormat(CH_NDSP_MUSIC, NDSP_FORMAT_MONO_PCM16);
        }

#ifndef NDEBUG
        printf("Set music format - sample rate: %d, stereo: %d\n",
               new_sample_rate, new_is_stereo);
#endif

        m_current_sample_rate = new_sample_rate;
        m_current_is_stereo = new_is_stereo;
    }

    if (!fill_buffer_impl()) {
        return false;
    }

    // Also fill the second buffer if it isn't already filled:

    status = ndsp_wave_buf_music[music_next_buf_idx].status;
    if (status == NDSP_WBUF_QUEUED || status == NDSP_WBUF_PLAYING) {
        return true;
    }

    return fill_buffer_impl();
}

bool NdspAudioThread::fill_buffer_impl() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

#ifdef TRACY_ENABLE
    ZoneNamedN(zonefill, "Filling buffer", true);
    ZoneColorV(zonefill,
               music_next_buf_idx ? tracy::Color::Pink : tracy::Color::Lime);
#endif

    u8* buffer = music_audio_buf.get() + music_next_buf_idx * MUSIC_BUFFER_SIZE;
    auto& wave_buf = ndsp_wave_buf_music[music_next_buf_idx];
    wave_buf.data_vaddr = buffer;

    auto len = m_mus_current->get_samples(buffer, MUSIC_BUFFER_SIZE);
    if (len == 0) {
        // Play next song
        return false;
    }

    DSP_FlushDataCache(buffer, MUSIC_BUFFER_SIZE);

    wave_buf.nsamples = len / 2 / (m_current_is_stereo ? 2 : 1);
    ndspChnWaveBufAdd(CH_NDSP_MUSIC, &wave_buf);

    music_next_buf_idx = (music_next_buf_idx + 1) % 2;

    return true;
}
