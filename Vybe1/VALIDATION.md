# Validation — 24 September 2026

## Completed in Linux using GCC C++20

Compiled the portable core and three test programs with `-O2 -Wall -Wextra -Werror -Wpedantic -pthread`. All passed:

- `reconcile_test`: synonym mapping, stable/composite keys, modified and unmatched records, duplicate/missing key review, schemas, selected/all attributes, normalization, masking, formula escaping, PAN blocking and cancellation.
- Large reconciliation: 200,000 rows on each side; 150,000 modified, one removed, one added and 49,999 unchanged. All 150,002 differences retained. Final optimized run: 319 ms comparison only. This is a synthetic benchmark, not an import, GUI or production throughput guarantee.
- `file_diff_test`: retained legacy tuple engine coverage for field pairing, duplicate counts/presence, blanks, zeros, punctuation, normalization, ragged/empty inputs and cancellation. 100,000 A / 101,000 B rows, 1,000 A-only, 2,000 B-only, 99,000 matches; 45 ms comparison only.
- `security_regression_test tests/fixtures`: strict policy value helpers, safe filenames, macro-format rejection, legitimate CSV text, HTML rejection, ZIP container fixtures, 401 serialized audit entries, and rejection of a truncated audit tail. Portable audit hashing is a test fallback; this does not validate Windows BCrypt or cross-process locking.

Recompiled the portable core and reconciliation/security tests with AddressSanitizer and UndefinedBehaviorSanitizer. Both passed, including the 200,000-row case and ZIP fixtures. Leak detection was disabled (`ASAN_OPTIONS=detect_leaks=0`) because of this traced environment; no leak-safety claim is made.

## Not executed here

No Windows compiler/runtime was available. Windows GUI compilation and interaction, red-cell rendering, build/deployment batch scripts, protected installation ACL checks, real group tokens, clipboard behavior, DPAPI encryption, process-tree containment, BCrypt audit hashing, filesystem notifications, browser/API fetchers and PowerShell signing were not run.

`test_file_differences.bat` supplies portable regressions, a real Windows DPAPI round-trip/policy-parser test and an interface compile check. It is a build gate for the updated build scripts. The policy parser unit test does not exercise production installation ACLs; manually test standard-user and administrator deployment and denied operations too.

The native XLSX reader and original installer definition were not supplied. XLSX end-to-end import and installer packaging remain unverified. CSV/XLSX ZIP checks are structural validation, not exhaustive hostile-input testing. Performance remains limited by RAM, import and snapshot costs.

Do not treat these test results as proof that every security defect has been removed. Complete Windows and deployment verification before using production employee records.

## Installation packaging correction — 25 September 2026 UTC

Verified YAML parsing, checker inclusion in each build script and artifact, policy source lookup, and archive integrity. Windows installer/checker execution remains unrun locally. The workflow includes a Windows protected-installation gate before artifact upload. This update does not alter comparison algorithms.

## Single EXE installer update

Added Inno Setup source, native account validation/provisioning and installer build/silent-test gates. YAML structure, referenced source paths, helper linkage and ZIP integrity checked locally. Inno compilation, Windows group provisioning, interactive/silent installation and uninstall are not executed here. The workflow explicitly tests installer exit status, policy checks and chosen account membership on Windows before uploading the EXE.

## Automatic account detection and shortcut correction

Added original-user detection with explicit-account fallback for elevated/unavailable original contexts. Preserved opt-in export access. Added exact-target legacy Desktop/Start Menu shortcut updates and a Windows CI regression for shortcut repair. YAML parsing, build linkage and source wiring checked locally; actual UAC/original-user detection, Inno compilation and Windows runtime checks remain unrun here.

## Startup recovery update

Added a validated installed-copy handoff and administrator-approved repair helper. Included the startup header in the Windows compile gate and added a legacy-location repair CI step. Local checks verify source wiring, helper linkage, workflow YAML and archive integrity only. Neither repair APIs nor GUI/UAC paths have been executed in this environment.

## 8.32.2 installer correction

Replaced hidden batch deployment with direct Inno Files entries so Restart Manager sees actual destination executables. Added preflight link checks, exact destination enforcement, native ACL finalization and a reinstall CI gate. Distinct versioned artifact prevents confusion with 8.10.4. Local wiring/YAML/archive checks only; Windows/Inno and in-use-file interaction remain unrun here. Negative Windows crash exit statuses now fail the native test gate.

## Security regression runner fix — 26 September 2026

Closed the audit input stream before test cleanup (required for Windows file deletion), replaced aborting assertions with checked exceptions, captured worker-thread exceptions, used unique test directories, and printed test stages and native exit codes. Recompiled core and security test with GCC C++20 -O2 -Wall -Wextra -Werror -Wpedantic -pthread. All security cases and ZIP fixtures passed locally. A deliberately missing fixture produced a readable failure and exit code 1. Windows rerun remains pending; the screenshot did not expose the original exception, so the cleanup defect is a concrete fix rather than a proven sole cause.


## Installer delivery correction
Removed the outer directory from the source ZIP. Added INSTALLER-WORKFLOW.txt, identical to the canonical workflow, to make replacing the old GitHub installer workflow straightforward. Renamed the workflow and added installer configuration preflight. No Windows execution claimed.


# Point 8.32.3: traversal limits and data-folder permissions

## Review of the reported risks

Audit append already seeks to the final 8,192 bytes. It does not scan the whole
log. Keep the existing thread mutex and Windows process lock; an uncoordinated
in-memory hash cache could become stale when another Point process appends.
Tail retrieval is bounded, but storage latency and contention still affect writes.
Appending is not a full-log integrity verification.

Relationship traversal already uses exact-value indexes, result-row limits and
a guard against multiple one-to-many expansions. The remaining risk is work
performed before a limit is reached. This revision adds:
- Limits::max_join_work, default 5,000,000 counted traversal operations per query.
- Early rejection while collecting related row IDs, before a large branch list
  is allocated; it uses the existing max_result_rows limit (100,000 by default).
- Early rejection of a second one-to-many expansion.
- Skipping expansion when the state already has all required field schemas.

Limit errors return no partial QueryResult. They do not silently discard matching
rows. Narrow the query when a limit is reached. The traversal budget is not a
wall-clock timeout and does not bound CSV parsing, index creation or relationship
discovery. It applies to Engine::query, not the separate file comparison engine.

## Data-folder startup correction

Old ACLs granted access to OWNER RIGHTS, which can resolve to Administrators
after elevation. New ACLs explicitly grant the current TokenUser SID, SYSTEM
and Administrators access. The data root and seven data directories are protected.
Installation policy checks and access groups remain enforced.

On access denied, the main app offers one elevated helper repair, then verifies
access and continues in the original process. Repair changes permissions, not
saved file contents. UAC must use the SAME account: different administrator
credentials are explicitly rejected to avoid repairing the wrong profile.
That scenario needs an administrator to repair the intended user's folder.
The UI never automatically elevates the main Point application.

## Verification

Linux GCC C++20 -O2 -Wall -Wextra -Werror -Wpedantic tests passed:
- scaling_test: complete 20-row one-to-many results, traversal work budget,
  candidate limit, complete-source pruning, 64 MiB audit-tail append.
- security_regression_test: policy, file validation, concurrent audit chaining,
  incomplete-log rejection and ZIP validation fixtures.
- file_diff_test: correctness and 100,000 vs 101,000 rows.
- reconcile_test: correctness and 200,000 rows per file; 150,002 change rows.

Added Windows regression coverage for legacy Administrators-owned workspace ACL
repair, repeat hardening and writing with the Administrators SID disabled.
Windows tests, UAC repair and installer execution were NOT run in this environment.
The included GitHub workflow runs native tests before publishing the installer.

The existing XLSX trusted signed reader dependency remains unsupplied; CSV does
not depend on that reader. Source package, not a prebuilt EXE.
