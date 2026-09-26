// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service::NIM {

class NIM_AOC final : public ServiceFramework<NIM_AOC> {
public:
    NIM_AOC();
    ~NIM_AOC();

private:
    /**
     * Generic stub for all nim:aoc commands
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : 0
     *      3 : 0
     */
    void Stub(Kernel::HLERequestContext& ctx);

    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::NIM

BOOST_CLASS_EXPORT_KEY(Service::NIM::NIM_AOC)
