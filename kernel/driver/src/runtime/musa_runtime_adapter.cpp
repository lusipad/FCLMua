#include "fclmusa/runtime/musa_runtime_adapter.h"

#include "fclmusa/logging.h"

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C"
NTSTATUS
FclRunMusaRuntimeSmokeTests() noexcept {
    try {
        std::vector<int> values;
        values.reserve(4);
        values.push_back(1);
        values.push_back(2);
        values.push_back(3);

        if (values.size() != 3) {
            return STATUS_DATA_ERROR;
        }

        std::map<int, std::wstring> dictionary;
        dictionary.emplace(7, L"fcl");
        dictionary[42] = L"musa";
        if (dictionary.size() != 2 || dictionary.at(42) != L"musa") {
            return STATUS_DATA_ERROR;
        }

        auto shared = std::make_shared<int>(1234);
        auto sharedCopy = shared;
        if (shared.use_count() != 2) {
            return STATUS_DATA_ERROR;
        }

        std::function<int(int)> doubler = [](int v) { return v * 2; };
        if (doubler(5) != 10) {
            return STATUS_DATA_ERROR;
        }

        bool exceptionCaptured = false;
        try {
            throw std::runtime_error("musa-runtime-smoke");
        } catch (const std::runtime_error&) {
            exceptionCaptured = true;
        }

        if (!exceptionCaptured) {
            return STATUS_INTERNAL_ERROR;
        }

        FCL_LOG_INFO0("Musa.Runtime STL smoke test passed");
        return STATUS_SUCCESS;
    } catch (...) {
        return STATUS_INTERNAL_ERROR;
    }
}
