#define _POSIX_C_SOURCE 200112L

#include "progress.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define BAR_WIDTH 30

static double MonotonicSeconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void FormatDuration(char *out, size_t size, double seconds) {
  int total = (int)(seconds + 0.5);
  snprintf(out, size, "%d:%02d", total / 60, total % 60);
}

static void FormatProgressLine(char *out, size_t size, int done, int total,
                               double elapsed) {
  double fraction = (double)done / (double)total;
  char bar[BAR_WIDTH + 1];
  int filled = (int)(fraction * BAR_WIDTH);
  for (int i = 0; i < BAR_WIDTH; i++)
    bar[i] = i < filled ? '#' : '-';
  bar[BAR_WIDTH] = '\0';

  char elapsedText[16];
  char remainingText[16];
  FormatDuration(elapsedText, sizeof(elapsedText), elapsed);
  double remaining = done > 0 ? elapsed * (total - done) / done : 0.0;
  FormatDuration(remainingText, sizeof(remainingText), remaining);

  snprintf(out, size, "[%s] %3d%%  step %d/%d  %s elapsed  %s left", bar,
           (int)(fraction * 100.0), done, total, elapsedText, remainingText);
}

ProgressBar StartProgress(int total) {
  return (ProgressBar){.total = total,
                       .startSeconds = MonotonicSeconds(),
                       .redrawInPlace = isatty(fileno(stderr)) == 1,
                       .lastReportedTenth = -1};
}

void UpdateProgress(ProgressBar *bar, int done) {
  char line[128];
  FormatProgressLine(line, sizeof(line), done, bar->total,
                     MonotonicSeconds() - bar->startSeconds);

  if (bar->redrawInPlace) {
    fprintf(stderr, "\r%s", line);
    fflush(stderr);
    return;
  }

  int tenth = done * 10 / bar->total;
  if (tenth == bar->lastReportedTenth)
    return;
  bar->lastReportedTenth = tenth;
  fprintf(stderr, "%s\n", line);
}

void FinishProgress(const ProgressBar *bar) {
  if (bar->redrawInPlace)
    fputc('\n', stderr);
}
