#ifndef HERBCEPTIONS_CATCH_RESULT_PCH_H
#define HERBCEPTIONS_CATCH_RESULT_PCH_H

struct pch_tracked_value {
  ~pch_tracked_value();
};

using payload_alias = pch_tracked_value;
payload_alias pch_make_value() return_failure { int };

using imported_catch_result = decltype(catch return_failure(pch_make_value()));

#endif
