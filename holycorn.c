#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mruby.h"
#include "mruby/array.h"
#include "mruby/proc.h"
#include "mruby/compile.h"
#include "mruby/string.h"
#include "mruby/range.h"
#include "mruby/hash.h"

#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "catalog/pg_foreign_table.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "commands/vacuum.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/var.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/numeric.h"
#include "plan_state.h"
#include "execution_state.h"
#include "options.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(holycorn_handler);
PG_FUNCTION_INFO_V1(holycorn_validator);

#define POSTGRES_TO_UNIX_EPOCH_DAYS (POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE)
#define POSTGRES_TO_UNIX_EPOCH_USECS (POSTGRES_TO_UNIX_EPOCH_DAYS * USECS_PER_DAY)

static void rbGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void rbGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static ForeignScan *rbGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid,
    ForeignPath *best_path, List *tlist, List *scan_clauses);
static void rbBeginForeignScan(ForeignScanState *node, int eflags);
static void rbExplainForeignScan(ForeignScanState *node, ExplainState *es);
static TupleTableSlot *rbIterateForeignScan(ForeignScanState *node);
static void fileReScanForeignScan(ForeignScanState *node);
static void rbEndForeignScan(ForeignScanState *node);

static void rbGetOptions(Oid foreigntableid, HolycornPlanState *state, List **other_options);
static void estimate_costs(PlannerInfo *root, RelOptInfo *baserel,
    HolycornPlanState *fdw_private,
    Cost *startup_cost, Cost *total_cost);

Datum handle_column(mrb_value *, TupleTableSlot *, int, mrb_value);

Datum holycorn_handler(PG_FUNCTION_ARGS) {
  FdwRoutine *fdwroutine = makeNode(FdwRoutine);

  fdwroutine->GetForeignRelSize  = rbGetForeignRelSize;
  fdwroutine->GetForeignPaths    = rbGetForeignPaths;
  fdwroutine->GetForeignPlan     = rbGetForeignPlan;

  fdwroutine->BeginForeignScan   = rbBeginForeignScan;
  fdwroutine->IterateForeignScan = rbIterateForeignScan;
  fdwroutine->EndForeignScan     = rbEndForeignScan;
  fdwroutine->ExplainForeignScan = rbExplainForeignScan;

  fdwroutine->ReScanForeignScan  = fileReScanForeignScan;

  PG_RETURN_POINTER(fdwroutine);
}

Datum holycorn_validator(PG_FUNCTION_ARGS) {
  List     *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
  Oid      catalog = PG_GETARG_OID(1);
  List     *other_options = NIL;
  ListCell   *cell;

  /*
   * These strings won't be used but they will hold temporary values extracted
   * from the foreign table definition
   */
  char * wrapper_path = NULL;
  char * wrapper_class = NULL;

  foreach(cell, options_list) {
    DefElem    *def = (DefElem *) lfirst(cell);

    if (strcmp(def->defname, "wrapper_path") == 0) {
      if (wrapper_path)
        ereport(DEBUG1,
            (errcode(ERRCODE_SYNTAX_ERROR),
             errmsg("conflicting or redundant options")));
      wrapper_path = defGetString(def);
    }
    else if (strcmp(def->defname, "wrapper_class") == 0) {
      if (wrapper_class)
        ereport(DEBUG1,
            (errcode(ERRCODE_SYNTAX_ERROR),
             errmsg("conflicting or redundant options")));
      wrapper_class = defGetString(def);
    } else {
      other_options = lappend(other_options, def);
    }
  }

  if (catalog == ForeignTableRelationId && (wrapper_path != NULL) && (wrapper_class != NULL)) {
    elog(ERROR, "[holycorn validator] wrapper_path or wrapper_class are required (and not both) for defining a holycorn foreign table");
  }

  PG_RETURN_VOID();
}

static void rbGetOptions(Oid foreigntableid, HolycornPlanState *state, List **other_options) {
  ForeignTable *table;
  List     *options;
  ListCell   *lc,
             *prev;

  table = GetForeignTable(foreigntableid);

  options = NIL;
  options = list_concat(options, table->options);

  prev = NULL;

  /* Set default values */
  state->wrapper_path  = NULL;
  state->wrapper_class = NULL;

  foreach(lc, options) {
    DefElem *def = (DefElem *) lfirst(lc);

    if (strcmp(def->defname, "wrapper_path") == 0) { /* Extract the wrapper_path */
      state->wrapper_path = defGetString(def);
      options = list_delete_cell(options, lc, prev);
    } else if (strcmp(def->defname, "wrapper_class") == 0) { /* Extract the wrapper_class */
      state->wrapper_class = defGetString(def);
      options = list_delete_cell(options, lc, prev);
    }

    prev = lc;
  }

  if (state->wrapper_path != NULL && state->wrapper_class != NULL) {
    elog(ERROR, "[holycorn rbGetOptions] wrapper_path or wrapper_class are required (and not both) for defining a holycorn foreign table");
  }

  *other_options = options;
}

static void rbGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid) {

  HolycornPlanState *fdw_private = (HolycornPlanState *) palloc(sizeof(HolycornPlanState));
  rbGetOptions(foreigntableid, fdw_private, &fdw_private->options);
  baserel->fdw_private = (void *) fdw_private;
}

static void rbGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid) {
  HolycornPlanState *fdw_private = (HolycornPlanState *) baserel->fdw_private;
  Cost    startup_cost;
  Cost    total_cost;

  /* Estimate costs */
  estimate_costs(root, baserel, fdw_private, &startup_cost, &total_cost);

  /*
   * Create a ForeignPath node and add it as only possible path.  We use the
   * fdw_private list of the path to carry the convert_selectively option;
   * it will be propagated into the fdw_private list of the Plan node.
   */
  Path * path = (Path*)create_foreignscan_path(root, baserel,
      baserel->rows, startup_cost, total_cost,
      NULL,    /* no pathkeys */
      NIL,   /* no outer rel either */
      NULL,
      NULL);

  add_path(baserel, path);
}

static ForeignScan * rbGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses) {
  Index scan_relid = baserel->relid;
  scan_clauses = extract_actual_clauses(scan_clauses, false);

  best_path->fdw_private = baserel->fdw_private;
  ForeignScan * scan = make_foreignscan(tlist, scan_clauses, scan_relid, NIL, best_path->fdw_private, NULL, NULL, NULL);

  return scan;
}

static void rbExplainForeignScan(ForeignScanState *node, ExplainState *es) {
}

static void rbBeginForeignScan(ForeignScanState *node, int eflags) {
  HolycornExecutionState *exec_state = (HolycornExecutionState *)palloc(sizeof(HolycornExecutionState));

  ForeignScan *plan = (ForeignScan *)node->ss.ps.plan;

  HolycornPlanState * hps = (HolycornPlanState*)plan->fdw_private;

  exec_state->mrb_state = mrb_open();

  mrb_value class;

  if(hps->wrapper_path) {
    FILE *source = fopen(hps->wrapper_path,"r");
    class = mrb_load_file(exec_state->mrb_state, source);
    fclose(source);
  } else {
    class = mrb_obj_value(mrb_class_get(exec_state->mrb_state, hps->wrapper_class));
  }

  mrb_value params = mrb_hash_new(exec_state->mrb_state);

#define HASH_SET(hash, key, val) \
  mrb_hash_set(exec_state->mrb_state, hash, mrb_str_new_lit(exec_state->mrb_state, key), val);

  HASH_SET(params, "PG_VERSION",         mrb_str_new_lit(exec_state->mrb_state, PG_VERSION));
  HASH_SET(params, "PG_VERSION_NUM",     mrb_float_value(exec_state->mrb_state, PG_VERSION_NUM));
  HASH_SET(params, "PACKAGE_STRING",     mrb_str_new_lit(exec_state->mrb_state, PACKAGE_STRING));
  HASH_SET(params, "PACKAGE_VERSION",    mrb_str_new_lit(exec_state->mrb_state, PACKAGE_VERSION));
  HASH_SET(params, "MRUBY_RUBY_VERSION", mrb_str_new_lit(exec_state->mrb_state, MRUBY_RUBY_VERSION));

  ListCell   *cell;
  foreach(cell, hps->options) {
    DefElem    *def = (DefElem *) lfirst(cell);

    char * key = def->defname;
    char * val = defGetString(def);

    mrb_hash_set( \
        exec_state->mrb_state, \
        params, \
        mrb_str_new(exec_state->mrb_state, key, strlen(key)), \
        mrb_str_new(exec_state->mrb_state, val, strlen(val)));
  }

  exec_state->iterator = mrb_funcall(exec_state->mrb_state, class, "new", 1, params);

  if (mrb_exception_p(exec_state->iterator)) {
    mrb_value message = mrb_funcall(exec_state->mrb_state, exec_state->iterator, "inspect", NULL);
    mrb_value pretty_params = mrb_funcall(exec_state->mrb_state, params, "inspect", NULL);
    elog(ERROR,
        "[holycorn] Instantiating %s raised an exception:\n%s\n (params: %s)\n",
        hps->wrapper_class,
        RSTRING_PTR(message),
        RSTRING_PTR(pretty_params));
  }

  node->fdw_state = (void *) exec_state;
}

static TupleTableSlot * rbIterateForeignScan(ForeignScanState *node) {
  HolycornExecutionState *exec_state = (HolycornExecutionState *) node->fdw_state;
  TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
  char * output_str;

#define StringDatumFromChars(column) \
  output_str = (char *)palloc(sizeof(char) * RSTRING_LEN(column) + 2); /* 2 = 1 prefix (string) + 1 suffix (NULL) */ \
  sprintf(output_str, "s%s\0", RSTRING_PTR(column)); \
  CStringGetDatum(output_str);

#define INSPECT \
  column = mrb_funcall(exec_state->mrb_state, column, "inspect", 0, NULL); \
  slot->tts_isnull[i] = false; \
  slot->tts_values[i] = StringDatumFromChars(column);

  ExecClearTuple(slot);

  mrb_value output = mrb_funcall(exec_state->mrb_state, exec_state->iterator, "each", 0, NULL);

  if (!mrb_array_p(output)) {
    output = mrb_funcall(exec_state->mrb_state, output, "inspect", 0, NULL);
    elog(LOG, "#each must provide an array (was %s)", RSTRING_PTR(output));
    return NULL;
  } else {
    slot->tts_nvalid = RARRAY_LEN(output);

    slot->tts_isempty = false;
    slot->tts_isnull = (bool *)palloc(sizeof(bool) * slot->tts_nvalid);
    slot->tts_values = (Datum *)palloc(sizeof(Datum) * slot->tts_nvalid);

    if (slot->tts_nvalid <= 0) { //TODO: the size can't be < 0 but being defensive is OK :-)
      slot->tts_isempty = true;
    } else {
      Datum outputDatum;
      for(int i = 0; i < slot->tts_nvalid; i++) {
        slot->tts_isnull[i] = true;
        slot->tts_values[i] = NULL;
        mrb_value column = mrb_ary_entry(output, i);

        switch(mrb_type(column)) {
          case MRB_TT_FREE:
            slot->tts_isnull[i] = true;
            break;
          case MRB_TT_FALSE: //TODO: use BoolGetDatum
            if (!mrb_nil_p(column)) {
                slot->tts_isnull[i] = true;
                slot->tts_values[i] = BoolGetDatum(false);
            }
            break;
          case MRB_TT_TRUE:
            slot->tts_isnull[i] = false;
            slot->tts_values[i] = BoolGetDatum(true);
            break;
          case MRB_TT_FIXNUM:
            outputDatum = Int64GetDatum(mrb_fixnum(column));
            slot->tts_isnull[i] = false;
            slot->tts_values[i] = outputDatum;
            break;
          case MRB_TT_SYMBOL: //TODO: use mrb_sym
            INSPECT
              break;
          case MRB_TT_UNDEF:
            elog(ERROR,"MRB_TT_UNDEF not supported (yet?)");
            break;
          case MRB_TT_FLOAT:
            outputDatum = Float8GetDatum(mrb_flo_to_fixnum(column));
            slot->tts_isnull[i] = false;
            slot->tts_values[i] = outputDatum;
            break;
          case MRB_TT_CPTR:
            elog(ERROR,"MRB_TT_CPTR not supported (yet?)");
            break;
          case MRB_TT_OBJECT:
            column = mrb_funcall(exec_state->mrb_state, column, "to_s", 0, NULL);
            slot->tts_isnull[i] = false;
            slot->tts_values[i] = StringDatumFromChars(column);
            break;
          case MRB_TT_CLASS:
            column = mrb_funcall(exec_state->mrb_state, column, "class", 0, NULL);
            column = mrb_funcall(exec_state->mrb_state, column, "to_s", 0, NULL);
            slot->tts_isnull[i] = false;
            slot->tts_values[i] = StringDatumFromChars(column);
            break;
          case MRB_TT_MODULE:
            column = mrb_funcall(exec_state->mrb_state, column, "to_s", 0, NULL);
            slot->tts_isnull[i] = false;
            slot->tts_values[i] = StringDatumFromChars(column);
            break;
          case MRB_TT_ICLASS:
            elog(ERROR, "MRB_TT_ICLASS not supported (yet?)");
            break;
          case MRB_TT_SCLASS:
            elog(ERROR, "MRB_TT_SCLASS not supported (yet?)");
            break;
          case MRB_TT_PROC:
            INSPECT
              break;
          case MRB_TT_ARRAY: //TODO: use array type
            INSPECT
              break;
          case MRB_TT_HASH: //TODO: use json type
            INSPECT
              break;
          case MRB_TT_STRING:
            slot->tts_isnull[i] = false;
            slot->tts_values[i] = StringDatumFromChars(column);
            break;
          case MRB_TT_RANGE: //TODO: Use the range type
            // struct RangeBound *range = (RangeBound*)malloc(sizeof(RangeBound));
            //   {
            //       Datum       val;            /* the bound value, if any */
            //       bool        infinite;       /* bound is +/- infinity */
            //       bool        inclusive;      /* bound is inclusive (vs exclusive) */
            //       bool        lower;          /* this is the lower (vs upper) bound */
            //   } RangeBound;
            // mrb_range_ptr(column)->edges->beg,
            // mrb_range_ptr(column)->edges->end,
            // mrb_range_ptr(column)->excl);
            INSPECT
              break;
          case MRB_TT_EXCEPTION:
            elog(ERROR, "MRB_TT_EXCEPTION not supported (yet?)");
            break;
          case MRB_TT_FILE:
            elog(ERROR, "MRB_TT_FILE not supported (yet?)");
            break;
          case MRB_TT_ENV:
            elog(ERROR, "MRB_TT_ENV not supported (yet?)");
            break;
          case MRB_TT_DATA:
            handle_column(exec_state->mrb_state, slot, i, column);
            break;
          case MRB_TT_FIBER:
            INSPECT
              break;
          case MRB_TT_MAXDEFINE:
            elog(ERROR, "MRB_TT_MAX_DEFINE not supported (yet?)");
            break;
          default:
            slot->tts_isnull[i] = true;
            elog(ERROR,"unknown type (0x%x)", mrb_type(column));
            break;
        }
        ExecStoreVirtualTuple(slot);
      }
    }
    return slot;
  }
}

static void fileReScanForeignScan(ForeignScanState *node) {
}

static void rbEndForeignScan(ForeignScanState *node) {
  HolycornExecutionState *exec_state = (HolycornExecutionState *) node->fdw_state;
  mrb_close(exec_state->mrb_state);
}

static void estimate_costs(PlannerInfo *root, RelOptInfo *baserel, HolycornPlanState *fdw_private, Cost *startup_cost, Cost *total_cost)
{
  BlockNumber pages = fdw_private->pages;
  double    ntuples = fdw_private->ntuples;
  Cost    run_cost;
  Cost    cpu_per_tuple;

  *startup_cost = baserel->baserestrictcost.startup;

  // Arbitrary choice to make it very expensive
  run_cost = seq_page_cost * pages;
  cpu_per_tuple = cpu_tuple_cost * 100 + baserel->baserestrictcost.per_tuple;
  run_cost += cpu_per_tuple * ntuples;
  *total_cost = *startup_cost + run_cost;
}

#define IS_A(actual, expected) strcmp(RSTRING_PTR(actual), expected) == 0

Datum handle_column(mrb_value * mrb, TupleTableSlot *slot, int idx, mrb_value rb_obj) {
  char * output_str = NULL;
  mrb_value class = mrb_funcall(mrb, rb_obj, "class", 0, NULL);
  mrb_value class_name = mrb_funcall(mrb, class, "to_s", 0, NULL);

  if(IS_A(class_name, "Time")) {
    rb_obj = mrb_funcall(mrb, rb_obj, "to_i", 0, NULL);
    long value = (long)mrb_fixnum(rb_obj);

    long int val = (value * 1000000L) - POSTGRES_TO_UNIX_EPOCH_USECS;
    slot->tts_isnull[idx] = false;
    slot->tts_values[idx] = Int64GetDatum(val);
  } else {
    //TODO: serialize the error type in the column?
    elog(WARNING, "Unsupported Ruby object (%s). Using #to_s by default", RSTRING_PTR(class_name));

    slot->tts_isnull[idx] = false;
    slot->tts_values[idx] = StringDatumFromChars(class_name);
  }
}
