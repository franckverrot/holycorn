struct HolycornOption {
  const char *optname;
  Oid         optcontext;
};

// Only allow setting the repository's path
static const struct HolycornOption valid_options[] = {
  {"wrapper_path", ForeignTableRelationId, false},
  {NULL,     InvalidOid, false}
};
