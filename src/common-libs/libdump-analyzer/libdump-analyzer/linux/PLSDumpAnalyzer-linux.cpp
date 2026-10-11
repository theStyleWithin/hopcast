/* HopCast (Linux): crash-dump analyzer stub.
 *
 * On Windows/macOS this library captures crash dumps and uploads them to NAVER's
 * NELO service. HopCast does not use NAVER services, so on Linux the same API is
 * provided as no-ops: nothing is captured or sent anywhere. A local-only crash
 * handler (e.g. writing a backtrace to the log) can be added here later.
 *
 * Original code, GPL-2.0-or-later.
 */
#include "PLSDumpAnalyzer.h"
#include "PLSAnalysisStack.h"

LIBDUMPANALUZER_API void pls_catch_unhandled_exceptions(const std::string &, const std::string &) {}
LIBDUMPANALUZER_API void pls_catch_unhandled_exceptions_and_send_dump(const ProcessInfo &) {}

LIBDUMPANALUZER_API void pls_set_prism_user_id(const std::string &) {}
LIBDUMPANALUZER_API void pls_set_prism_session(const std::string &) {}
LIBDUMPANALUZER_API void pls_set_prism_video_adapter(const std::string &) {}
LIBDUMPANALUZER_API void pls_set_setup_session(const std::string &) {}
LIBDUMPANALUZER_API void pls_set_prism_sub_session(const std::string &) {}
LIBDUMPANALUZER_API void pls_set_prism_pid(const std::string &) {}

LIBDUMPANALUZER_API bool pls_wait_send_dump(ProcessInfo)
{
	return false;
}

LIBDUMPANALUZER_API bool pls_capture_force_exit_dump(const char *)
{
	return false;
}

namespace pls {
/* No crash history is recorded on Linux, so no plugin is ever flagged. */
LIBDUMPANALUZER_API std::optional<ThirdPartyPlugin> check_third_party_crash(std::string &)
{
	return std::nullopt;
}
} // namespace pls
