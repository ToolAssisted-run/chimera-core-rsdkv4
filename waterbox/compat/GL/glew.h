/* glew.h - the engine finds its OpenGL functions with GLEW; the core links
 * Mesa's (setup-mesa.sh) and every function is there at link time, so this
 * is GLEW's interface with nothing to look up: Mesa's headers, with the
 * prototypes of every extension, and a glewInit that has nothing to do. */
#ifndef CHIMERA_COMPAT_GLEW_H
#define CHIMERA_COMPAT_GLEW_H

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#define GLEW_OK 0
#define GLEW_ERROR_NO_GLX_DISPLAY 4

static inline GLenum glewInit(void) { return GLEW_OK; }
static inline const GLubyte *glewGetErrorString(GLenum error) { (void)error; return (const GLubyte *)"no error"; }

#endif
