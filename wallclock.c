/* A simple transparent analogue clock
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 2 of the License, or
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

#define WIDTH 100
#define HEIGHT 100
#define POS_X 1250
#define POS_Y 650
#define OVERRIDE 0 // 1= Don't 0=Set overriderediect
#define clockupdate 1        /* 1=Show second hand */
#define hour_hand_colour   "#446688"
#define minute_hand_colour "#6688aa"
#define second_hand_colour "#88aabb"
#define clock_face_colour  "#6688aa"
#define tick_mark_colour   "#446688"
#define hh_l 60             /* length of the hands */
#define mh_l 75
#define sh_l 91
#define tm_l 93             /* length of tick marks */
#define hh_w 4              /* Thickness of the hands */
#define mh_w 2
#define sh_w 1
#define tm_w 3

time_t t;
struct tm *tmval;

struct ti {
   int s_x,s_y,m_x,m_y,h_x,h_y;
} at;

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
        XDrawLine(dis,win,tick_m, (angle1*tm_l)/200000 + center_x,
         (angle2*tm_l)/200000 + center_y, (angle1*100)/200000 + center_x,
          (angle2*100)/200000 + center_y);
    }
    return(0);
}

int update_hands() {
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

	drawface();
     for(i=-1;i<2;i++)
	for(j=-1;j<2;j++)
	{
	   XDrawLine(dis,win,hour_h,center_x+i,center_y+j,at.h_x,at.h_y);
	   XDrawLine(dis,win,min_h,center_x+i,center_y+j,at.m_x,at.m_y);
    }

    if (clockupdate == 1) {
       angle3 = tmval->tm_sec;
       at.s_x =   sine[angle3] * square  *sh_l/200000 + center_x;
       at.s_y = -(sine[(angle3+15)%60])* square *sh_l/200000 + center_y;
       XDrawLine(dis,win,sec_h,center_x,center_y,at.s_x,at.s_y);
    } else {
       at.s_x = at.s_y = 0;
    }
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
	if(WIDTH > 0) width = WIDTH;
	else width = (XDisplayWidth(dis, screen_num)/4);
	if(HEIGHT > 0 ) height = HEIGHT;
	else height = (XDisplayHeight(dis, screen_num)/4);
	XGCValues values;

	win = XCreateSimpleWindow(dis, DefaultRootWindow(dis),POS_X,
			POS_Y,height,height,0,border,background);

	XSetWindowBackgroundPixmap(dis, win, ParentRelative);

	// This returns the FD of the X11 display (or something like that)
    x11_fd = ConnectionNumber(dis);
	
	/* create the hour_h GC to draw the hour hand */
	values.foreground = getcolor(hour_hand_colour);
	values.line_width = hh_w;
	values.line_style = LineSolid;
	values.cap_style = CapRound;
	hour_h = XCreateGC(dis, win, GCForeground|GCLineWidth|GCLineStyle|GCCapStyle,&values);

	/* create the min_h GC to draw the minute hand */
	values.foreground = getcolor(minute_hand_colour);
	values.line_width = mh_w;
	values.line_style = LineSolid;
	values.cap_style = CapRound;
	min_h = XCreateGC(dis, win, GCForeground|GCLineWidth|GCLineStyle|GCCapStyle,&values);

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

    if(OVERRIDE == 0) {
        XSetWindowAttributes att;
        att.override_redirect = True;
        XChangeWindowAttributes( dis, win, CWOverrideRedirect, &att );
	}
	XSelectInput(dis, win, ButtonPressMask|StructureNotifyMask|ExposureMask );

	XMapWindow(dis, win);
	update_hands();

	while(1) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        // Set our timer.  One second sounds good.
        tv.tv_usec = 0;
        if(clockupdate == 1) tv.tv_sec = 1;
        else tv.tv_sec = 30;
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
		        XFreeGC(dis, hour_h);
		        XFreeGC(dis, min_h);
		        XFreeGC(dis, sec_h);
		        XFreeGC(dis, face_cl);
		        XFreeGC(dis, tick_m);
		    	XCloseDisplay(dis);
		    	return(0);
		    }
		}
	}
	return(0);
}
