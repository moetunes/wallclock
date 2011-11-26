/* A simple transparent analogue clock
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define clockupdate 1        /* 1=Show second hand */
#define hour_hand_colour   "#442211"
#define minute_hand_colour "#664422"
#define second_hand_colour "#886644"
#define clock_face_colour  "#664422"
#define tick_mark_colour   "#442211"
#define hh_l 60             /* length of the hands */
#define mh_l 75
#define sh_l 88
#define hh_w 8              /* Thickness of the hands */
#define mh_w 6
#define sh_w 3
#define tm_w 6

time_t t;
struct tm *tmval;

static int square, center_x, center_y;
static int i, width, height;
static int angle1, angle2, angle3;

static Window win;
static Display *dis;
static GC hour_h, min_h, sec_h, face_cl, tick_m;

/* save from computing sine(x), use pre-computed values
 * There are *100, to avoid using floats */
short sine[]={0,105,208,309,407,500,588,669,743,809,866,914,951,
    978,995,999,995,978,951,914,866,809,743,669,588,500,
	407,309,208,105,0,-105,-208,-309,-407,-500,-588,-669,
	-743,-809,-866,-914,-951,-978,-995,-999,-994,-978,-951,
	-914,-866,-809,-743,-669,-588,-500,-407,-309,-208,-105 };

unsigned long getcolor(const char* color) {
    XColor c;
    Colormap map = DefaultColormap(dis,DefaultScreen(dis));

    if(!XAllocNamedColor(dis,map,color,&c,&c)) {
        fprintf(stderr, "\033[0;31mError parsing color!\n");
        exit(1);
    }
    return c.pixel;
}

int drawface() {
    center_x = (square/2)+((width-square)/2);
    center_y = (square/2)+((height-square)/2);

    XClearWindow(dis, win);
    XDrawArc(dis, win, face_cl, center_x-(square/2), center_y-(square/2), square, square, 0, 360*64);
    // Draw the tick marks
    for(i=0;i<60;i+=5) {
	    angle1  = sine[i]*(square-2);
        angle2  = -sine[(i+15)%60]*(square-2);
        XDrawLine(dis,win,tick_m, (angle1*9)/20000 + center_x,
         (angle2*9)/20000 + center_y, (angle1*10)/20000 + center_x,
          (angle2*10)/20000 + center_y);
    }
    return(0);
}

int update_hands() {
    struct ti {
       int s_x,s_y,m_x,m_y,h_x,h_y;
    } at;
    /* get the current time */
    t=time(0);
    tmval = localtime(&t);
    int i,j;

    /* calculate the positions of the hands */
    angle1 = (tmval->tm_hour%12)*5 + tmval->tm_min/12;
    at.h_x =   sine[angle1] * square  *hh_l/200000 + center_x;
    at.h_y =   -(sine[(angle1+15)%60])* square *hh_l/200000 + center_y;

    angle2 = tmval->tm_min;
    at.m_x =   sine[angle2] * square  *mh_l/200000 + center_x;
    at.m_y = -(sine[(angle2+15)%60])* square *mh_l/200000 + center_y;

    if (clockupdate == 1) {
       angle3 = tmval->tm_sec;
       at.s_x =   sine[angle3] * square  *sh_l/200000 + center_x;
       at.s_y = -(sine[(angle3+15)%60])* square *sh_l/200000 + center_y;
    } else {
       at.s_x = at.s_y = 0;
    }
	drawface();
     for(i=-1;i<2;i++)
	for(j=-1;j<2;j++)
	{
	   XDrawLine(dis,win,hour_h,center_x+i,center_y+j,at.h_x,at.h_y);
	   XDrawLine(dis,win,min_h,center_x+i,center_y+j,at.m_x,at.m_y);
	}
     if (clockupdate == 1)
	XDrawLine(dis,win,sec_h,center_x,center_y,at.s_x,at.s_y);
	XFlush(dis);
	return(0);

}

int main(int argc, char ** argv){
	int screen_num;
	unsigned long background, border;
	XEvent ev;
	int x11_fd;
    fd_set in_fds;
    struct timeval tv;

	/* First connect to the display server, as specified in the DISPLAY environment variable. */
	dis = XOpenDisplay(NULL);
	if (!dis) {fprintf(stderr, "unable to connect to display");return 1;}

	screen_num = DefaultScreen(dis);
	background = None; //BlackPixel(dis, screen_num);
	border = WhitePixel(dis, screen_num);
	width = (XDisplayWidth(dis, screen_num)/4);
	height = (XDisplayHeight(dis, screen_num)/4);
	XGCValues values;

	win = XCreateSimpleWindow(dis, DefaultRootWindow(dis),width*3-20,
			height*3-20,height,height,0,border,background);

	XSetWindowBackgroundPixmap(dis, win, ParentRelative);

	// This returns the FD of the X11 display (or something like that)
    x11_fd = ConnectionNumber(dis);
	
	/* create the hour_h GC to draw the hour hand */
	values.foreground = getcolor(hour_hand_colour);
	values.line_width = hh_w;
	values.line_style = LineSolid;
	hour_h = XCreateGC(dis, win, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* create the min_h GC to draw the minute hand */
	values.foreground = getcolor(minute_hand_colour);
	values.line_width = mh_w;
	values.line_style = LineSolid;
	min_h = XCreateGC(dis, win, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* create the sec_h GC to draw the second hand */
	values.foreground = getcolor(second_hand_colour);
	values.line_width = sh_w;
	values.line_style = LineSolid;
	sec_h = XCreateGC(dis, win, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* create the face_cl GC to draw the clock face */
	values.foreground = getcolor(clock_face_colour);
	values.line_width = 4;
	values.line_style = LineSolid;
	face_cl = XCreateGC(dis, win, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* create the tick_m GC to draw the tick marks */
	values.foreground = getcolor(tick_mark_colour);
	values.line_width = tm_w;
	values.line_style = LineSolid;
	tick_m = XCreateGC(dis, win, GCForeground|GCLineWidth|GCLineStyle,&values);

	XSelectInput(dis, win, ButtonPressMask|StructureNotifyMask|ExposureMask );

	XMapWindow(dis, win);

	while(1) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        // Set our timer.  One second sounds good.
        tv.tv_usec = 0;
        tv.tv_sec = 1;
        // Wait for X Event or a Timer
        if (!(select(x11_fd+1, &in_fds, 0, 0, &tv))) {
            update_hands();
        }

        // Handle XEvents and flush the input 
        while(XPending(dis)) {
            XNextEvent(dis, &ev);
            switch(ev.type){
		    case Expose:
		        if(width >= height)
		            square = height-4;
		        else
		            square = width-4;
   		    	drawface();
		    	break;
		    case ConfigureNotify:
		    	if (width != ev.xconfigure.width || height != ev.xconfigure.height) {
		    		width = ev.xconfigure.width;
		    		height = ev.xconfigure.height;
		    		XClearWindow(dis, win);
		    	}
		    	break;
            /* exit if a button is pressed inside the window */
		    case ButtonPress:
		    	XCloseDisplay(dis);
		    	return(0);
		    }
		}
	}
	return(0);
}
