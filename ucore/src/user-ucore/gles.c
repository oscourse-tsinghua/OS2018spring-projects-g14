#include <EGL/egl.h>
#include <GLES/gl.h>

static GLfloat vertexs[] = {
	-0.5, -0.5,
	0.5, -0.5,
	0, 0.5,

	0, -0.3,
	0.6, -0.3,
	0.3, 0.6
};

static GLfloat colors[] = {
	1, 0, 0, 1,
	0, 1, 0, 1,
	0, 0, 1, 1,

	0, 0, 1, 1,
	0, 1, 0, 1,
	1, 0, 0, 1,
};

void init()
{
	glClearColor(0.64f, 0.81f, 0.38f, 1.0f);
}

void draw()
{
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexPointer(2, GL_FLOAT, 0, vertexs);
	glColorPointer(4, GL_FLOAT, 0, colors);

	glEnableClientState(GL_VERTEX_ARRAY);

	glColor4f(1, 0.5, 0, 1);
	glViewport(0, 0, 960, 540);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glEnableClientState(GL_COLOR_ARRAY);

	glViewport(960, 540, 960, 540);
	glDrawArrays(GL_TRIANGLES, 3, 3);

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
