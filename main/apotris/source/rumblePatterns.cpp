#include "def.h"

#include "rumblePatterns.hpp"
#include "platform.hpp"
#include "save.h"

// Each part of a pattern's strength ranges from 0..=8 (inclusive),
// representing the target active duty cycle for those frames.
// NOTE: As a result, if you change the PERIOD, you have to update all the sequence strengths.
constexpr int PERIOD = 8;

static_assert((PERIOD & (PERIOD - 1)) == 0, "PERIOD must be a power of two");

#pragma mark - Sequence data

struct RumbleSequencePart {
    // duration in frames
    const int frames;
    // PWM duty cycle, strength / PERIOD
    const u8 strength;
};
struct RumbleSequenceData {
    const RumbleSequencePart* data;
    const int len;
};

#define DEFINE_RUMBLE_SEQUENCE_DATA(name, ...) \
    RumbleSequencePart _rumbleseq_##name##_partdata[] = { __VA_ARGS__ }

#define RUMBLE_SEQUENCE(name) \
    RumbleSequenceData { \
        .data = _rumbleseq_##name##_partdata, \
        .len = sizeof(_rumbleseq_##name##_partdata) / sizeof(RumbleSequencePart) \
    }

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_PLACE,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{4, 5},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_CLASSIC_CLEAR,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{6, 6},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ZONE_RIPPLE,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 3},
    RumbleSequencePart{3, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 3},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{1, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_SINGLE,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{4, 6},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_DOUBLE,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{4, 6},
    RumbleSequencePart{2, 0},
    RumbleSequencePart{4, 2},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_TRIPLE,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{4, 6},
    RumbleSequencePart{2, 0},
    RumbleSequencePart{4, 3},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_QUAD,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{4, 6},
    RumbleSequencePart{2, 0},
    RumbleSequencePart{4, 4},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_QUAD_B2B,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{4, 6},
    RumbleSequencePart{2, 0},
    RumbleSequencePart{4, 6},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_PERFECT_CLEAR,
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 6},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 6},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 6},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 6},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 6},
    RumbleSequencePart{3, 0},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_TSPIN,
    RumbleSequencePart{4, 4},
    RumbleSequencePart{4, 6},
    RumbleSequencePart{4, 4},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_TSPIN_B2B,
    RumbleSequencePart{4, 4},
    RumbleSequencePart{4, 8},
    RumbleSequencePart{4, 4},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_TSPIN_MINI,
    RumbleSequencePart{4, 3},
    RumbleSequencePart{4, 5},
    RumbleSequencePart{4, 3},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_LEVEL_UP,
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 3},
    RumbleSequencePart{3, 5},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ZONE,
    RumbleSequencePart{3, 1},
    RumbleSequencePart{1, 0},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ZONE_2,
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ZONE_3,
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ZONE_4,
    RumbleSequencePart{3, 4},
    RumbleSequencePart{1, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ZONE_5,
    RumbleSequencePart{3, 5},
    RumbleSequencePart{1, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ZONE_6,
    RumbleSequencePart{3, 6},
    RumbleSequencePart{1, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_LONG_ZONE,
    RumbleSequencePart{3, 1},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{1, 0},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_LONG_ZONE_2,
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{3, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_LONG_ZONE_3,
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 1},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 1},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_REGRET,
    RumbleSequencePart{4, 6},
    RumbleSequencePart{4, 5},
    RumbleSequencePart{4, 4},
    RumbleSequencePart{4, 3},
    RumbleSequencePart{8, 2},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_COOL,
    RumbleSequencePart{4, 1},
    RumbleSequencePart{4, 2},
    RumbleSequencePart{4, 3},
    RumbleSequencePart{4, 4},
    RumbleSequencePart{4, 5},
    RumbleSequencePart{4, 6},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_OCTORIS,
    RumbleSequencePart{4, 6},
    RumbleSequencePart{1, 5},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2,4},
    RumbleSequencePart{3, 5},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_DODECATRIS,
    RumbleSequencePart{4, 6},
    RumbleSequencePart{1, 5},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2,4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{1, 3},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_DECAHEXATRIS,
    RumbleSequencePart{4, 8},
    RumbleSequencePart{1, 5},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2,4},
    RumbleSequencePart{6, 7},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{1, 3},
    RumbleSequencePart{1, 4},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_PERFECTRIS,
    RumbleSequencePart{4, 7},
    RumbleSequencePart{1, 5},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2,4},
    RumbleSequencePart{6, 7},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{1, 3},
    RumbleSequencePart{1, 4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{10, 4},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ULTIMATRIS,
    RumbleSequencePart{4, 8},
    RumbleSequencePart{1, 5},
    RumbleSequencePart{3, 4},
    RumbleSequencePart{3, 2},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{2, 3},
    RumbleSequencePart{2,4},
    RumbleSequencePart{6, 7},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{1, 3},
    RumbleSequencePart{1, 4},
    RumbleSequencePart{2, 5},
    RumbleSequencePart{1, 0},
    RumbleSequencePart{10, 8},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ERR_2,
    RumbleSequencePart{6, 3},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{6, 3},
    RumbleSequencePart{3, 0},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_ERR_4,
    RumbleSequencePart{6, 3},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{6, 3},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{6, 3},
    RumbleSequencePart{3, 0},
    RumbleSequencePart{6, 3},
    RumbleSequencePart{3, 0},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_YOUWIN,
    RumbleSequencePart{5, 3},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 5},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 3},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 6},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 3},
    RumbleSequencePart{5, 0},
     RumbleSequencePart{10, 6},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_YOULOSE,
    RumbleSequencePart{5, 2},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 4},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 2},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 5},
    RumbleSequencePart{5, 0},
    RumbleSequencePart{5, 2},
    RumbleSequencePart{5, 0},
     RumbleSequencePart{15, 3},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_SECRET,
    RumbleSequencePart{15, 6},
    RumbleSequencePart{15, 0},
    RumbleSequencePart{7, 4},
    RumbleSequencePart{7, 0},
    RumbleSequencePart{7, 4},
    RumbleSequencePart{7, 0},
    RumbleSequencePart{7, 5},
    RumbleSequencePart{11, 0},
    RumbleSequencePart{7, 4},
    RumbleSequencePart{30, 0},
    RumbleSequencePart{15, 6},
    RumbleSequencePart{15, 0},
    RumbleSequencePart{15, 6},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_TEST_DOUBLE,
    RumbleSequencePart{2, 0},
    RumbleSequencePart{2, 2},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 6},
    RumbleSequencePart{2, 8},
    RumbleSequencePart{2, 0},
    RumbleSequencePart{2, 2},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 6},
    RumbleSequencePart{4, 6},
);

DEFINE_RUMBLE_SEQUENCE_DATA(RUMBLE_TEST_TRIPLE,
    RumbleSequencePart{2, 0},
    RumbleSequencePart{2, 2},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 6},
    RumbleSequencePart{2, 8},
    RumbleSequencePart{2, 0},
    RumbleSequencePart{2, 2},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{2, 6},
    RumbleSequencePart{2, 8},
    RumbleSequencePart{2, 0},
    RumbleSequencePart{2, 2},
    RumbleSequencePart{2, 4},
    RumbleSequencePart{4, 6},
);

const RumbleSequenceData sequences[RUMBLE_ARRAY_MAX] = {
    DEFINE_RUMBLE_LIST
};

#pragma mark - Rumble implementation

struct RumblePatternState
{
    const RumbleSequencePart* sequenceData;
    int sequenceLen;
    int partIndex;
    u8 partStrength;
    int partEndFrame;
    u32 partTick;
    bool lastRumble;
};

constexpr RumblePatternState c_stoppedState = RumblePatternState {
    .sequenceData = nullptr,
    .sequenceLen = 0,
    .partIndex = 0,
    .partStrength = 0,
    .partEndFrame = 0,
    .partTick = 0,
    .lastRumble = false,
};

// owned by rumble interrupt
static RumblePatternState s_state = c_stoppedState;
static RumbleSequence s_currentSequence = RUMBLE_NONE;

// owned by main code
static RumbleSequence s_newSequence = RUMBLE_NONE;
static u8 s_newStrengthMod = 0;

void rumblePattern(const RumbleSequence sequence, int strength_mod)
{
    // If Classic Style selected and it's not a classic rumble, skip it.
    // Bit of a hack but also simple enough for now.
    if (!savefile->settings.rumbleStyle && sequence > RUMBLE_CLASSIC_CLEAR) {
        return;
    }

    s_currentSequence = RUMBLE_NONE;
    s_newSequence = sequence;
    if (strength_mod >= 3)
        s_newStrengthMod = 3;
    else if (strength_mod <= 0)
        s_newStrengthMod = 0;
    else
        s_newStrengthMod = strength_mod;
}

void rumblePatternLowPri(const RumbleSequence sequence, int strength_mod)
{
    if (s_newSequence == RUMBLE_NONE || sequence > s_newSequence) {
        rumblePattern(sequence, strength_mod);
    }
}

void rumblePatternStop()
{
    // Clear any active pattern,
    s_newSequence = RUMBLE_NONE;
    s_newStrengthMod = 0;
    // and disable motor if it's running.
    rumbleStop();
}

const RumbleSequence rumbleGetCurrent() {
    return s_newSequence;
}

INLINE void getNewRumble()
{
    if (s_newSequence == s_currentSequence)
        return;

    if (s_newSequence != RUMBLE_NONE)
    {
        const RumbleSequenceData* newSequenceData = &sequences[s_newSequence];

        s_state = RumblePatternState {
            .sequenceData = newSequenceData->data,
            .sequenceLen = newSequenceData->len,
            .partIndex = 0,
            .partStrength = newSequenceData->data[0].strength,
            .partEndFrame = frameCounter + newSequenceData->data[0].frames,
            .partTick = 0,
            .lastRumble = false
        };
    }
    else
    {
        s_state = c_stoppedState;
    }

    s_currentSequence = s_newSequence;
}

void rumblePatternLoop()
{
    // profiler hello world
    // for (int i = 0; i < 300; ++i)
    // {
    //     __asm__("nop");
    // }
    getNewRumble();

    if (s_currentSequence == RUMBLE_NONE || !savefile->settings.rumbleStrength)
    {
        rumblePatternStop();
        return;
    }

    if (frameCounter > s_state.partEndFrame)
    {
        // reached end of part, advance sequence
        s_state.partIndex++;

        if (s_state.partIndex >= s_state.sequenceLen)
        {
            rumblePatternStop();
            return;
        }
        else
        {
            s_state.partStrength = s_state.sequenceData[s_state.partIndex].strength;
            s_state.partEndFrame = frameCounter + s_state.sequenceData[s_state.partIndex].frames;

            // reset to start of the pattern (cheaper ofc)
            s_state.partTick = 0;

            // or alternatively, find out where we'd have been if we had always been
            // playing back the new pattern and start from there
            // s_state.partTick &= (PERIOD - 1);
        }
    }

    const u8 userStrength = static_cast<u8>(savefile->settings.rumbleStrength);

    const u8 tickStrength = s_state.partTick & (PERIOD - 1);

    const u8 strengthModifier = std::min(s_state.partStrength + s_newStrengthMod, PERIOD);

    // multiply strengths and round up, in integer math
    const u8 partStrength = s_state.partStrength ? (strengthModifier * userStrength + (PERIOD - 1)) / PERIOD : 0;

    const bool rumble = partStrength != 0 && tickStrength < partStrength;

    #if defined(SWITCH) || defined(PORTMASTER) || defined(WEB) || defined(PC)
    // SDL2 can just recieve the part's strength directly, sending the final (user-scaled) strength to our output func.
    rumbleOutput(partStrength);
    #else
    // Other platforms (like GBA) need the full PWM approach.
    if (rumble) {
        if (!s_state.lastRumble)
            rumbleOutput(partStrength);
    } else {
        rumbleStop();
    }
    #endif

    s_state.partTick++;
    s_state.lastRumble = rumble;
}
