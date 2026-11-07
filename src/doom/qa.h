//
// PLAYER TWO QA build: JSONL event log and scripted QA actions.
// The log is for the verifier only; the tester agent never sees it.
//

#ifndef __QA__
#define __QA__

#include "doomtype.h"
#include "p_mobj.h"
#include "r_defs.h"

extern boolean qa_logging;

void QA_Init(void);
void QA_PreTick(void);
void QA_PostTick(void);

// data is the body of a JSON object (no braces), or NULL.
void QA_Event(const char *type, mobj_t *actor, const char *datafmt, ...)
    PRINTF_ATTR(3, 4);

// JSON-escaped copy of s, valid until a few more calls.
const char *QA_Str(const char *s);

void QA_SkyDrawn(int fixedcolormap);
void QA_WallDistance(line_t *line, fixed_t vanilla, double precise);

#endif
