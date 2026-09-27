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
