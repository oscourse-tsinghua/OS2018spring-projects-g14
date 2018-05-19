#include <GLES/gl.h>


int main(int argc, char *argv[])
{
	cprintf("Hello GLES!!!\n");

	glOpen();

	glDrawTriangle();

	glClose();
}
