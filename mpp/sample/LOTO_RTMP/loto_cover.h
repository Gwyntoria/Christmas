#ifndef LOTO_COVER_H
#define LOTO_COVER_H

#include "loto_comm.h"


#ifdef __cplusplus
extern "C" {
#endif

HI_S32 LOTO_COVER_InitCoverRegion() ;
HI_S32 LOTO_COVER_UninitCoverRegion();

// HI_S32 LOTO_COVER_AttachCover();
// HI_S32 LOTO_COVER_DetachCover();

HI_S32 LOTO_COVER_AddCover();
HI_S32 LOTO_COVER_RemoveCover();

void LOTO_COVER_Switch(int state);

HI_S32 LOTO_COVER_ChangeCover();
// HI_S32 LOTO_COVER_ChangeCover(int* state);

#ifdef __cplusplus
}
#endif

#endif