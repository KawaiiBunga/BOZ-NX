#ifndef BOZ_CAMERA_CONTROLLER_H
#define BOZ_CAMERA_CONTROLLER_H
#include "guest.h"
typedef struct {
    int requested, original;
    uint32_t owned_camera;
} CameraController;
typedef uint32_t (*CameraCall)(void *user, uint32_t function, uint32_t a, uint32_t b);
uint32_t camera_controller_update(CameraController *state, GuestMem *memory,
    uint32_t device, uint32_t original_update, uint32_t setter, CameraCall call, void *user);
#endif
