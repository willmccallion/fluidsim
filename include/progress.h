#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdbool.h>

/** Terminal progress bar on stderr; falls back to a line per 10% if piped. */
typedef struct {
  int total;
  double startSeconds;
  bool redrawInPlace;
  int lastReportedTenth;
} ProgressBar;

ProgressBar StartProgress(int total);
void UpdateProgress(ProgressBar *bar, int done);
void FinishProgress(const ProgressBar *bar);

#endif
