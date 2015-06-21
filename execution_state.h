typedef struct HolycornExecutionState
{
  char  *wrapper_path;
  mrb_value * mrb_state;
  mrb_value iterator;
  List  *options;
  mrb_value options_hash;
  int passes;
} HolycornExecutionState;
