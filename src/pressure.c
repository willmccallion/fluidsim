#include "pressure.h"

#define SMOOTH_ITERATIONS_PRE 2
#define SMOOTH_ITERATIONS_POST 2
#define SMOOTH_ITERATIONS_COARSEST 32
#define MIN_COARSE_DIMENSION 4

static unsigned int GroupCount(int cells) { return (unsigned int)(cells + 15) / 16; }

static void BindR32F(unsigned int unit, Texture2D_GL tex, GLenum access) {
  glBindImageTexture(unit, tex.id, 0, GL_FALSE, 0, access, GL_R32F);
}

static void ImageBarrier(void) {
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

static bool CanCoarsen(int width, int height) {
  return width % 2 == 0 && height % 2 == 0 &&
         width / 2 >= MIN_COARSE_DIMENSION &&
         height / 2 >= MIN_COARSE_DIMENSION;
}

static unsigned int LoadShaderFile(const char *path) {
  char *code = LoadFileText(path);
  unsigned int program = LoadCompute(code);
  UnloadFileText(code);
  return program;
}

PressureSolver InitPressureSolver(Texture2D_GL pressure, Texture2D_GL divergence,
                                  Texture2D_GL obstacles) {
  PressureSolver solver = {0};
  solver.levels[0] = (PressureLevel){pressure, divergence, obstacles};
  solver.levelCount = 1;

  int width = pressure.width;
  int height = pressure.height;
  while (solver.levelCount < MAX_PRESSURE_LEVELS && CanCoarsen(width, height)) {
    width /= 2;
    height /= 2;
    solver.levels[solver.levelCount++] = (PressureLevel){
        CreateTexture2D(width, height, GL_R32F),
        CreateTexture2D(width, height, GL_R32F),
        CreateTexture2D(width, height, GL_R32F)};
  }

  solver.shdSmooth = LoadShaderFile("resources/shaders/mg_smooth.glsl");
  solver.shdRestrict = LoadShaderFile("resources/shaders/mg_restrict.glsl");
  solver.shdProlong = LoadShaderFile("resources/shaders/mg_prolong.glsl");
  solver.shdRestrictMask =
      LoadShaderFile("resources/shaders/mg_restrict_mask.glsl");
  return solver;
}

void RebuildPressureMasks(const PressureSolver *solver) {
  rlEnableShader(solver->shdRestrictMask);
  for (int l = 1; l < solver->levelCount; l++) {
    const PressureLevel *coarse = &solver->levels[l];
    BindR32F(0, solver->levels[l - 1].mask, GL_READ_ONLY);
    BindR32F(1, coarse->mask, GL_WRITE_ONLY);
    rlComputeShaderDispatch(GroupCount(coarse->mask.width),
                            GroupCount(coarse->mask.height), 1);
    ImageBarrier();
  }
  rlDisableShader();
}

static void Smooth(const PressureSolver *solver, int level, int iterations) {
  const PressureLevel *lvl = &solver->levels[level];
  int parityLoc = rlGetLocationUniform(solver->shdSmooth, "parity");
  unsigned int groupsX = GroupCount((lvl->pressure.width + 1) / 2);
  unsigned int groupsY = GroupCount(lvl->pressure.height);

  rlEnableShader(solver->shdSmooth);
  BindR32F(0, lvl->pressure, GL_READ_WRITE);
  BindR32F(1, lvl->rhs, GL_READ_ONLY);
  BindR32F(2, lvl->mask, GL_READ_ONLY);
  for (int i = 0; i < iterations; i++) {
    for (int parity = 0; parity <= 1; parity++) {
      rlSetUniform(parityLoc, &parity, RL_SHADER_UNIFORM_INT, 1);
      rlComputeShaderDispatch(groupsX, groupsY, 1);
      ImageBarrier();
    }
  }
}

static void RestrictResidual(const PressureSolver *solver, int fineLevel) {
  const PressureLevel *fine = &solver->levels[fineLevel];
  const PressureLevel *coarse = &solver->levels[fineLevel + 1];

  rlEnableShader(solver->shdRestrict);
  BindR32F(0, fine->pressure, GL_READ_ONLY);
  BindR32F(1, fine->rhs, GL_READ_ONLY);
  BindR32F(2, fine->mask, GL_READ_ONLY);
  BindR32F(3, coarse->rhs, GL_WRITE_ONLY);
  BindR32F(4, coarse->pressure, GL_WRITE_ONLY);
  rlComputeShaderDispatch(GroupCount(coarse->pressure.width),
                          GroupCount(coarse->pressure.height), 1);
  ImageBarrier();
}

static void ProlongCorrection(const PressureSolver *solver, int fineLevel) {
  const PressureLevel *fine = &solver->levels[fineLevel];
  const PressureLevel *coarse = &solver->levels[fineLevel + 1];

  rlEnableShader(solver->shdProlong);
  BindR32F(0, coarse->pressure, GL_READ_ONLY);
  BindR32F(1, coarse->mask, GL_READ_ONLY);
  BindR32F(2, fine->pressure, GL_READ_WRITE);
  BindR32F(3, fine->mask, GL_READ_ONLY);
  rlComputeShaderDispatch(GroupCount(fine->pressure.width),
                          GroupCount(fine->pressure.height), 1);
  ImageBarrier();
}

static void VCycle(const PressureSolver *solver) {
  int coarsest = solver->levelCount - 1;
  for (int l = 0; l < coarsest; l++) {
    Smooth(solver, l, SMOOTH_ITERATIONS_PRE);
    RestrictResidual(solver, l);
  }
  Smooth(solver, coarsest, SMOOTH_ITERATIONS_COARSEST);
  for (int l = coarsest - 1; l >= 0; l--) {
    ProlongCorrection(solver, l);
    Smooth(solver, l, SMOOTH_ITERATIONS_POST);
  }
}

void SolvePressure(const PressureSolver *solver, int vCycles) {
  for (int i = 0; i < vCycles; i++)
    VCycle(solver);
  rlDisableShader();
}
