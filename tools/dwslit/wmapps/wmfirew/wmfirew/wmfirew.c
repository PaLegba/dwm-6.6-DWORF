/*
 * wmfirew is a windowmaker / afterstep dock/wharf applet for GNU/Linux
 *
 * It draws fireworks.
 *
 * Code based on some of the Window maker programs found on the internet
 *
 * Code based on wmpong, by Dag Wieers <dag@digibel.be>
 *
 * Copyright 1999, Dave Turner (turnerd@reed.edu);
 *
 * This program is distributed under the GPL license. 
 *
 * Thanks to Richard Stallman for the GNU system and the GPL.
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <X11/xpm.h>

#include "../wmgeneral/wmgeneral.h"

#include "wmfirew-master.xpm"

#define USLEEP 20000
#define NUM_FW 8
#define NUM_LASTS 7
#define NUM_CMAPS 8

typedef struct {
  int stage;
  float sparksx [20][NUM_LASTS], sparksy [20][NUM_LASTS];
  float sparksvx [20], sparksvy [20];
  float sparkc [20];
  int color_map;
  int numsparks;
} firew;

struct xpm_position
  {
    int x, y;
  };
struct xpm_surface
  {
    int x, y, w, h;
  };



struct xpm_position pos[] =
{
  {5, 50},
  {16, 50},
  {27, 50},
  {38, 50},
  {49, 50}
};
struct xpm_surface rel[] =
{
  {1, 70, 11, 9},
  {12, 70, 11, 9},
  {23, 70, 11, 9},
  {34, 70, 11, 9},
  {45, 70, 11, 9}
};
struct xpm_surface prs[] =
{
  {1, 79, 11, 9},
  {12, 79, 11, 9},
  {23, 79, 11, 9},
  {34, 79, 11, 9},
  {45, 79, 11, 9}
};


extern GC NormalGC;

firew works [NUM_FW];

char wmfirew_pm[64][48];


/*****************************************************************************/

void do_frame () {

  int done;
  int i, j, k;
  int x, y;

  //clear screen;

  for (x = 0; x < 64; x ++) {
    for (y = 0; y < 48; y ++) {
      XCopyArea(display, wmgen.pixmap, wmgen.pixmap, NormalGC, 0, 65, 1, 1, x, y);
    }
  }
  for (i = 0; i < NUM_FW; i ++) {

    switch (works [i].stage) {
    case 0:
      
      works [i].sparksy[0][0] = 0; 
      works [i].sparksx[0][0] = (rand () % 44) + 10;

      works [i].sparksvx[0] = 0;
      works [i].sparksvy[0] = 3;

      works [i].sparkc [0] = 20;
      
      works [i].color_map = rand () % NUM_CMAPS;

      works [i].stage = 1;

      works [i].numsparks = 1;

      break;

    case 1:

      for (k = NUM_LASTS; k > 0; k --) {
	works [i].sparksy[0][k] = works [i].sparksy[0][k - 1];
	works [i].sparksx[0][k] = works [i].sparksx[0][k - 1];
      }

      works [i].sparksy[0][0] += works [i].sparksvy[0];
      works [i].sparksvy[0] -= .130 + (float)(rand () % 300) / 10000;
      works [i].sparkc[0] ++;

      if (works [i].sparksvy [0] < (rand () % 200) /10000) {
	works [i].stage = 2;
	works [i].numsparks = (rand () % 10) + 10;

	for (j = 0; j < works [i].numsparks; j ++) {
	  
	  works [i].sparksx[j][0] = works [i].sparksx[0][0];
	  works [i].sparksy[j][0] = works [i].sparksy[0][0];

	  works [i].sparksvx[j] = ((float)(rand () % 200)) / 100 - 1;
	  works [i].sparksvy[j] = (float)(rand () % 100) / 100;

	  works [i].sparkc [j] = 63;
	}
      }
      break;


    case 2:
      done = 1;

      for (j = 0; j < works [i].numsparks; j ++) {
	
	for (k = NUM_LASTS - 1; k > 0; k --) {
	  works [i].sparksy[j][k] = works [i].sparksy[j][k - 1];
	  works [i].sparksx[j][k] = works [i].sparksx[j][k - 1];
	}

	works [i].sparksx[j][0] += works [i].sparksvx[j];
	works [i].sparksy[j][0] += works [i].sparksvy[j];
	
	works [i].sparksvx[j] *= .98;
	works [i].sparksvy[j] -= .140 + (float)(rand () % 200) / 10000;

	works [i].sparkc [j] --;

	if (works [i].sparksy[j][NUM_LASTS - 1] > 0) done = 0;
      }

      if (done == 1) {
	works [i].stage = 3;
	works [i].numsparks = 0;
      }

      break;
    }

    for (j = 0; j < works [i].numsparks; j ++) {
      for (k = 0; k < NUM_LASTS; k ++) {
	x = works [i].sparksx[j][k];
	y = works [i].sparksy[j][k];
	if ( y > 0 && y < 63 && x >= 0 && x < 64) {
	  XCopyArea(display, wmgen.pixmap, wmgen.pixmap, NormalGC, 70 + works[i].color_map, works[i].sparkc[j] * (NUM_LASTS - k) / NUM_LASTS, 1, 1, x, 48 - y);
	}
      }
    }

    if (works [i].stage == 3 && (rand () % 40) == 0) {
      works [i].stage = 0;
    }
  }  



}


/*****************************************************************************/

void main (int argc, char *argv[]) {
  XEvent Event;

  char wmfirew_mask_bits[64*64];
  int wmfirew_mask_width = 64;
  int wmfirew_mask_height = 64;

  int i;

  int paused = 0;

  createXBMfromXPM (wmfirew_mask_bits, wmfirew_master_xpm, wmfirew_mask_width, wmfirew_mask_height);
  openXwindow (argc, argv, wmfirew_master_xpm, wmfirew_mask_bits, wmfirew_mask_width, wmfirew_mask_height);

  for (i = 0; i < 5; i++)
    AddMouseRegion (i, pos[i].x, pos[i].y, pos[i].x + prs[i].w, pos[i].y + prs[i].h);


  srand (time (NULL));
  for (i = 0; i < NUM_FW; i ++) {
    works [i].stage = 3;
  }

  while (1) {

    if (!paused) {
      do_frame ();    
      RedrawWindow ();
    }

    while (XPending (display)) {
      XNextEvent (display, &Event);

      switch (Event.type) {

      case Expose:
	RedrawWindow ();
	break;
      case DestroyNotify:
	XCloseDisplay (display);
	exit (0);
	break;
      case ButtonPress:
	//check for pause
        if ( Event.xbutton.button == 2 ) {
          paused = !paused;
          break;
        }

	i = CheckMouseRegion (Event.xbutton.x, Event.xbutton.y);
	copyXPMArea (prs[i].x, prs[i].y, prs[i].w, prs[i].h, pos[i].x, pos[i].y);
	switch (i)
	  {
	  case 0:

            break;
	  case 1:

	    break;
	  case 2:

	    break;
	  case 3:

	    break;
	  case 4:

            break;
	  }
	break;
      case ButtonRelease:
	i = CheckMouseRegion (Event.xbutton.x, Event.xbutton.y);
	copyXPMArea (rel[i].x, rel[i].y, rel[i].w, rel[i].h, pos[i].x, pos[i].y);
	break;
      }
    }
    usleep (USLEEP);
  }
}

