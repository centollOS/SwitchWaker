/*
 * pc_figurines.h - "Completar la Galería Nintendo" (issue #44, native/src/pc/features/pc_figurines.cpp):
 * every figurine Carlov makes, on the file being played, from the options menu's Calidad de vida tab.
 *
 * Only in memory: the file keeps them when the player saves. The game thread calls these.
 */
#ifndef PC_FIGURINES_H
#define PC_FIGURINES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PcFigurinesResult {
    PC_FIGURINES_DONE = 0,     // every figurine set
    PC_FIGURINES_NOT_MEMBER,   // the file is not a gallery member yet: nothing set
    PC_FIGURINES_ALREADY,      // the file already has them all: nothing set
} PcFigurinesResult;

// How many of the 134 figurines the file being played has (daNpcMt_c::getFigureMakeNum).
int pc_figurines_made(void);
// Every figurine on the file being played, as daNpcMt_c::setFigure does for one.
PcFigurinesResult pc_figurines_complete(void);

#ifdef __cplusplus
}
#endif

#endif
