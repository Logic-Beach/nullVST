#ifndef RENDER_H
#define RENDER_H

void Render();
void SetMouthThroat(unsigned char mouth, unsigned char throat);

/* Set when a render fills the output buffer, or the phoneme tables
   cannot hold the line. Callers should drop that line. */
extern int samOverflow;
extern int samError;

#endif
