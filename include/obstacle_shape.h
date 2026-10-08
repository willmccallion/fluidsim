#ifndef OBSTACLE_SHAPE_H
#define OBSTACLE_SHAPE_H

/** Wind tunnel obstacle; positions and size are fractions of the domain. */
typedef struct {
  float centerX;
  float centerY;
  /** Height (circle diameter) as a fraction of the domain height. */
  float size;
  /** Silhouette image whose dark, opaque pixels are solid; NULL = circle. */
  const char *maskPath;
} ObstacleShape;

static inline ObstacleShape DefaultObstacleShape(void) {
  return (ObstacleShape){0.5f, 0.5f, 0.2f, 0};
}

#endif
