/**
 * @file subprocess_mock.h
 * @brief External-CLI invocation mock — scripted argv/stdin/stdout/exit.
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.6
 *
 * Used by V2 providers whose backend layer launches an external tool
 * (KryoFluxProviderV2 → DTC; FluxEngineProviderV2 → fluxengine CLI;
 * FC5025 has a `fcimage` companion tool too — both P1.8+ tasks).
 *
 * The mock surface mimics a typical "run a process synchronously"
 * helper: pass argv + stdin, get back stdout + stderr + exit code. The
 * test scripts a sequence of (argv-pattern → response) entries; the
 * mock pops one per `run()` call.
 *
 * The argv-pattern is a substring matcher: each script entry can list
 * tokens it expects to see on the argv (in order) — strict match would
 * be fragile against optional flags. Tests can also assert via
 * `recorded_runs()` after the fact.
 *
 * Pure C++20, no Qt. Header-only.
 */
#ifndef UFT_TESTS_MOCK_HARDWARE_SUBPROCESS_MOCK_H
#define UFT_TESTS_MOCK_HARDWARE_SUBPROCESS_MOCK_H

#include <algorithm>
#include <cstdio>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

#include "byte_buffer.h"

namespace uft::tests::mocks {

/**
 * @brief Scripted external-CLI mock.
 */
class SubprocessMock {
public:
    /* ── Test scenario API ────────────────────────────────────────────── */

    struct ScriptedRun {
        /** Argv tokens that MUST appear in the call's argv (in order).
         *  Empty list = match any. The test typically lists the binary
         *  name + the distinctive subcommand flag. */
        std::vector<std::string> require_argv_subseq;

        /** stdout the mock will return. */
        std::string stdout_reply;
        /** stderr the mock will return. */
        std::string stderr_reply;
        /** Exit code. 0 = success in POSIX convention. */
        int exit_code = 0;
    };

    void queue_run(ScriptedRun r) { m_queue.push_back(std::move(r)); }

    /** Convenience for the common case: just stdout, exit 0. */
    void queue_run(std::string stdout_reply) {
        m_queue.push_back(ScriptedRun{ {}, std::move(stdout_reply), "", 0 });
    }

    /** Convenience: stdout + non-zero exit. */
    void queue_run_failed(std::string stderr_reply, int exit_code) {
        m_queue.push_back(ScriptedRun{ {}, "", std::move(stderr_reply), exit_code });
    }

    /* ── Recorded runs — for post-hoc inspection ──────────────────────── */

    struct RecordedRun {
        std::vector<std::string> argv;
        std::string stdin_data;
        std::string stdout_returned;
        std::string stderr_returned;
        int exit_code_returned = 0;
    };
    const std::deque<RecordedRun> &recorded_runs() const noexcept {
        return m_recorded;
    }

    void assert_consumed() const {
        if (!m_queue.empty()) {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                "SubprocessMock: %zu scripted run(s) left UNCONSUMED. "
                "The provider invoked fewer subprocesses than the test "
                "expected.",
                m_queue.size());
            throw std::logic_error(msg);
        }
    }

    /* ── Backend API (called by V2 provider's do_* methods) ───────────── */

    struct RunResult {
        std::string stdout_text;
        std::string stderr_text;
        int exit_code = 0;
    };

    /**
     * Synchronous "execute" entry. Validates argv against the next
     * scripted run's `require_argv_subseq`, records the actual call
     * verbatim, and returns the scripted reply.
     */
    RunResult run(const std::vector<std::string> &argv,
                  const std::string &stdin_data = "")
    {
        if (m_queue.empty()) {
            throw std::out_of_range(
                "SubprocessMock::run(): provider invoked subprocess but "
                "no scripted run was queued. Add queue_run(...) before "
                "exercising the provider.");
        }
        ScriptedRun r = std::move(m_queue.front());
        m_queue.pop_front();

        /* Subseq match: each required token must appear in argv,
         * in order, but other tokens may appear between them. */
        if (!r.require_argv_subseq.empty()) {
            auto it = argv.begin();
            for (const auto &needle : r.require_argv_subseq) {
                it = std::find(it, argv.end(), needle);
                if (it == argv.end()) {
                    char msg[512];
                    std::string joined;
                    for (const auto &tok : argv) {
                        if (!joined.empty()) joined += " ";
                        joined += tok;
                    }
                    std::snprintf(msg, sizeof(msg),
                        "SubprocessMock::run(): argv subseq mismatch — "
                        "expected token %s not found (after prior matches) "
                        "in actual argv: [%s]",
                        needle.c_str(), joined.c_str());
                    throw std::logic_error(msg);
                }
                ++it;  /* advance past the matched token */
            }
        }

        RecordedRun rec{
            argv,
            stdin_data,
            r.stdout_reply,
            r.stderr_reply,
            r.exit_code,
        };
        m_recorded.push_back(std::move(rec));

        return RunResult{
            r.stdout_reply,
            r.stderr_reply,
            r.exit_code,
        };
    }

private:
    std::deque<ScriptedRun> m_queue;
    std::deque<RecordedRun> m_recorded;
};

}  // namespace uft::tests::mocks

#endif  // UFT_TESTS_MOCK_HARDWARE_SUBPROCESS_MOCK_H
