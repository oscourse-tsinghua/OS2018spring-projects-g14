#include <file.h>
#include <unistd.h>

#include "gl.h"

#define DRM_IOCTL_VC4_SUBMIT_CL                         0x00
#define DRM_IOCTL_VC4_WAIT_SEQNO                        0x01
#define DRM_IOCTL_VC4_WAIT_BO                           0x02
#define DRM_IOCTL_VC4_CREATE_BO                         0x03
#define DRM_IOCTL_VC4_MMAP_BO                           0x04
#define DRM_IOCTL_VC4_CREATE_SHADER_BO                  0x05
#define DRM_IOCTL_VC4_GET_HANG_STATE                    0x06
#define DRM_IOCTL_VC4_GET_PARAM                         0x07
#define DRM_IOCTL_VC4_SET_TILING                        0x08
#define DRM_IOCTL_VC4_GET_TILING                        0x09
#define DRM_IOCTL_VC4_LABEL_BO                          0x0a

static int fd = 0;

void glOpen(void)
{
    fd = open("gpu0:", O_RDWR);
}

void glDrawTriangle(void)
{
    ioctl(fd, DRM_IOCTL_VC4_SUBMIT_CL, 0);
}

void glClose(void)
{
    if (fd) {
        close(fd);
    }
}
