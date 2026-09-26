// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/nim/nim_aoc.h"

SERIALIZE_EXPORT_IMPL(Service::NIM::NIM_AOC)

namespace Service::NIM {

NIM_AOC::NIM_AOC() : ServiceFramework("nim:aoc", 2) {
    const FunctionInfo functions[] = {
        // clang-format off
        {0x0003, &NIM_AOC::Stub, "SetApplicationId"},
        {0x0004, &NIM_AOC::Stub, "SetTin"},
        {0x0009, &NIM_AOC::Stub, "ListContentSetsEx"},
        {0x0018, &NIM_AOC::Stub, "GetBalance"},
        {0x001D, &NIM_AOC::Stub, "GetCustomerSupportCode"},
        {0x0021, &NIM_AOC::Stub, "Initialize"},
        {0x0024, &NIM_AOC::Stub, "CalculateContentsRequiredSize"},
        {0x0025, &NIM_AOC::Stub, "RefreshServerTime"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

NIM_AOC::~NIM_AOC() = default;

void NIM_AOC::Stub(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
    rb.Push<u32>(0);

    LOG_WARNING(Service_NIM, "(STUBBED) nim:aoc command 0x{:04X} called", ctx.CommandID());
}

} // namespace Service::NIM
