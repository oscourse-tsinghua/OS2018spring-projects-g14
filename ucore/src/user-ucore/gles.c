#include <EGL/egl.h>
#include <GLES/gl.h>

int main(int argc, char *argv[])
{
	cprintf("Hello GLES!!!\n");

	if (glOpen()) {
		return 1;
	}

	if (eglCreateWindowSurface()) {
		return 1;
	}

	glDrawTriangle();

	glClose();

	return 0;
}
