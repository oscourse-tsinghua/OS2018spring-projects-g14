#include <EGL/egl.h>
#include <GLES/gl.h>

void init()
{
	glClearColor(0.64f, 0.81f, 0.38f, 1.0f);
}

void draw()
{
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawTriangle();
	glFlush();
}

int main(int argc, char *argv[])
{
	cprintf("Hello GLES!!!\n");

	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLContext ctx = eglCreateContext(dpy);

	if (!eglMakeCurrent(dpy, ctx)) {
		return 1;
	}

	init();

	draw();

	eglDestroyContext(dpy, ctx);

	return 0;
}
