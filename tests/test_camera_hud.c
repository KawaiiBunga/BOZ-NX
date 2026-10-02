#include "camera_fov.h"
#include "camera_controller.h"
#include "hud_controls.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned char heap[0x1000];
static int sets, updates;
static uint32_t bits(float value) { uint32_t out; memcpy(&out,&value,4); return out; }
static void put(unsigned offset, float value) { uint32_t v=bits(value); memcpy(heap+offset,&v,4); }
static float get(unsigned offset) { float out; memcpy(&out,heap+offset,4); return out; }
static uint32_t call(void *user,uint32_t fn,uint32_t a,uint32_t b) {
    (void)user;
    if (fn==1) { assert(a==0x1000); ++updates; return 0x1234; }
    assert(fn==2 && a==0x1300); ++sets; memcpy(heap+0x340,&b,4); return 0;
}
int main(void) {
    assert(camera_fov_setting(0)==0 && camera_fov_setting(50)==60 && camera_fov_setting(120)==110);
    assert(camera_fov_effective(40,40,0)==40 && camera_fov_effective(40,40,110)==55);
    float zoomed=camera_fov_effective(20,40,110);
    float k=0.017453292519943296f;
    assert(fabsf(tanf(zoomed*k)/tanf(55*k)-tanf(20*k)/tanf(40*k))<1e-6f);
    assert(!camera_fov_valid(NAN) && !camera_fov_valid(INFINITY));
    GuestMem mem={0}; guest_mem_add(&mem,0x1000,sizeof heap,heap,1);
    uint32_t cam=0x1300; memcpy(heap+0x78,&cam,4);
    put(0xc0,40); put(0xc4,40); put(0x340,40);
    CameraController state={0};
    assert(camera_controller_update(&state,&mem,0x1000,1,2,call,NULL)==0x1234);
    assert(updates==1 && sets==0 && state.original==80);
    state.requested=110;
    camera_controller_update(&state,&mem,0x1000,1,2,call,NULL);
    assert(sets==1 && get(0x340)==55 && get(0xc0)==40 && get(0xc4)==40);
    camera_controller_update(&state,&mem,0x1000,1,2,call,NULL); assert(sets==1);
    put(0xc0,20); put(0x340,20);
    camera_controller_update(&state,&mem,0x1000,1,2,call,NULL);
    assert(sets==2 && fabsf(get(0x340)-zoomed)<1e-5f && get(0xc0)==20);
    state.requested=0;
    camera_controller_update(&state,&mem,0x1000,1,2,call,NULL);
    assert(sets==3 && get(0x340)==20 && !state.owned_camera);
    put(0xc0,NAN); state.requested=100;
    camera_controller_update(&state,&mem,0x1000,1,2,call,NULL); assert(sets==3);
    float ortho[16]={0}; ortho[0]=2.0f/1280; ortho[5]=-2.0f/720;
    ortho[10]=ortho[15]=1; ortho[12]=-1; ortho[13]=1;
    assert(hud_stick_atlas(1024,512,ortho));
    float perspective[16]={0}; perspective[11]=-1;
    assert(!hud_stick_atlas(1024,512,perspective));
    assert(!hud_stick_atlas(512,512,ortho));
    const int heights[]={360,540,720,1080};
    const float points[][2]={{75,475},{229,630},{794,475},{945,630},{100,80},{1100,600}};
    for (unsigned r=0;r<4;r++) {
        int h=heights[r],w=h*16/9,vp[4]={0,0,w,h};
        for (unsigned i=0;i<6;i++) {
            float x,y;
            assert(hud_project_vertex(points[i][0],points[i][1],0,ortho,vp,w,h,&x,&y));
            assert(fabsf(x-points[i][0])<0.001f && fabsf(y-points[i][1])<0.001f);
            assert(hud_stick_region(x,y)==(i<4));
        }
    }
    puts("PASS: FOV default/reset, engine lens updates, unchanged ADS state; stick regions at 360/540/720/1080p, HUD/world separation");
}
