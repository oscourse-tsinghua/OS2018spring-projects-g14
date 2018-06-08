#include <EGL/egl.h>
#include <GLES/gl.h>

EGLDisplay dpy;
EGLDisplay ctx;

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

GLfloat u = 0.01, v = 0.01;

void draw()
{
	int i;
	for (i = 0; i < 12; i += 2) {
		vertexs[i] += u;
		vertexs[i + 1] += v;
	}

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

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glFlush();

	eglSwapBuffers(dpy);
}

int main(int argc, char *argv[])
{
	cprintf("Hello GLES!!!\n");

	dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	ctx = eglCreateContext(dpy);

	if (!eglMakeCurrent(dpy, ctx)) {
		return 1;
	}

	init();

	int i;
	for (i = 0; i < 100; i++)
		draw();

	eglDestroyContext(dpy, ctx);

	return 0;
}
