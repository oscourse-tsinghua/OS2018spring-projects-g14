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

void init()
{
	glClearColor(0.64f, 0.81f, 0.38f, 1.0f);
}

void draw()
{
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexPointer(2, GL_FLOAT, 0, vertexs);

	glEnableClientState(GL_VERTEX_ARRAY);

	glViewport(0, 0, 960, 540);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glViewport(960, 540, 960, 540);
	glDrawArrays(GL_TRIANGLES, 3, 3);

	glDisableClientState(GL_VERTEX_ARRAY);

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
