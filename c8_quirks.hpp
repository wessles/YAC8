#pragma once

namespace yac8 {
    /**
     * A struct for specifying emulation "quirks" that certain games expect.
     */
    struct c8_quirks {
        bool loadStoreQuirk = true;
        bool shiftQuirk = true;
        bool wrap = true;
    };
}