/* A simple transparent analogue clock with double buffering
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
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TRANSPARENT 0        /* 1=Use background_colour 0=Transparency */
#define background_colour  "#00FF00"
#define clockupdate 0        /* 1=Don't 0=Show second hand */
#define OVERRIDE 1           /* 1= Don't 0=Set overriderediect, so the window manager won't control the window */
#define WIDTH 200            /* Position and Size for when using override direct */
#define HEIGHT 200
#define POS_X 1050
#define POS_Y 550
#define hour_hand_colour   "#232323" //"#777777" //"#004488" //"#446688"
#define minute_hand_colour "#343434" //"#888888" //"#225599" //"#6688aa"
#define second_hand_colour "#454545" //"#999999" //"#4466aa" //"#88aabb"
#define clock_face_background "#dedede" //"#002244" //"#cccccc"
#define clock_face_colour  "#454545" //"#999999" //"#002266" //"#6688aa"
#define tick_mark_colour   "#454545" //"#999999" //"#004488" //"#446688"
#define hh_l 60             /* length of the hands in % of radius*/
#define mh_l 75
#define sh_l 91
#define tm_l 93             /* length of tick marks */
#define hh_w 8              /* Thickness of the hands */
#define mh_w 6
#define sh_w 2
#define tm_w 6
#define face_w 2

time_t t;
struct tm *tmval;

struct ti {
   int s_x,s_y,m_x,m_y,h_x,h_y;
} at;

static int square, center_x, center_y;
static int i, width, height, win_x, win_y;
static int angle1, angle2, angle3, trans;

static Pixmap root_pixmap;
static Drawable win;
static Drawable win_face;
static Window realwin;
static Window root;
static Display *dis;
static Atom id = None, wm_del_win;
static GC hour_h, min_h, sec_h, face_cl, tick_m, bg_gc, face_bg;
static int xerror(Display *dis, XErrorEvent *ee);
static int (*xerrorxlib)(Display *, XErrorEvent *);
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
        fputs(":: Wallclock : \033[0;31mError parsing color!\n", stderr);
        exit(1);
    }
    return c.pixel;
}

void get_background() {
    const char* pixmap_id_names[] = {
        "_XROOTPMAP_ID", "ESETROOT_PMAP_ID", NULL
    };
    unsigned int j = 0;
    root_pixmap = None;
    
    for (j=0; (pixmap_id_names[j] && (None == root_pixmap)); j++) {

        if (None == id) {
            id = XInternAtom(dis, pixmap_id_names[j], True);
        }

        if (None != id) {
            Atom actual_type = None;
            int actual_format = 0;
            unsigned long nitems = 0;
            unsigned long bytes_after = 0;
            unsigned char *properties = NULL;
            int rc = 0;

            rc = XGetWindowProperty(dis, root, id, 0, 1, False,
                    XA_PIXMAP, &actual_type, &actual_format, &nitems,
                    &bytes_after, &properties);

            if (Success == rc && properties) {
                root_pixmap = *((Pixmap*)properties);
                if(properties != NULL) XFree(properties);
            }
        }
    }
    if(root_pixmap == None) trans = 1;
}

int drawface() {
    center_x = width/2; // (square/2)+((width-square)/2);
    center_y = height/2; // (square/2)+((height-square)/2);

	if(trans == 0) {
        XCopyArea(dis, root_pixmap, win_face, hour_h, win_x, win_y, width, height, 0, 0);
    } else
        XFillRectangle(dis, win_face, bg_gc, 0, 0, width, height);

    // Draw the face background
    XFillArc(dis, win_face, face_bg, center_x-(square/2), center_y-(square/2), square, square, 0, 360*64);

    // Draw the tick marks
    for(i=0;i<60;i+=5) {
	    angle1  = sine[i]*(square-face_w+2);
        angle2  = -sine[(i+15)%60]*(square-face_w+2);
        XDrawLine(dis,win_face,tick_m, (angle1*tm_l)/200000 + center_x,
         (angle2*tm_l)/200000 + center_y, (angle1*100)/200000 + center_x,
          (angle2*100)/200000 + center_y);
    }
    XDrawArc(dis, win_face, face_cl, center_x-(square/2), center_y-(square/2), square, square, 0, 360*64);
    return(0);
}

int update_hands() {
    /* get the current time */
    t=time(0);
    tmval = localtime(&t);
    int i,j;

    XCopyArea(dis, win_face, win, hour_h, 0, 0, width, height, 0, 0);
    /* calculate the positions of the hands */
    angle1 = (tmval->tm_hour%12)*5 + tmval->tm_min/12;
    at.h_x =   sine[angle1] * square  *hh_l/200000 + center_x;
    at.h_y =   -(sine[(angle1+15)%60])* square *hh_l/200000 + center_y;

    angle2 = tmval->tm_min;
    at.m_x =   sine[angle2] * square  *mh_l/200000 + center_x;
    at.m_y = -(sine[(angle2+15)%60])* square *mh_l/200000 + center_y;

     for(i=-1;i<2;i++)
	for(j=-1;j<2;j++)
	{
	   XDrawLine(dis,win,hour_h,center_x+i,center_y+j,at.h_x,at.h_y);
	   XDrawLine(dis,win,min_h,center_x+i,center_y+j,at.m_x,at.m_y);
    }

    if (clockupdate < 1) {
       angle3 = tmval->tm_sec;
       at.s_x =   sine[angle3] * square  *sh_l/200000 + center_x;
       at.s_y = -(sine[(angle3+15)%60])* square *sh_l/200000 + center_y;
       XDrawLine(dis,win,sec_h,center_x,center_y,at.s_x,at.s_y);
    } else {
       at.s_x = at.s_y = 0;
    }
    XCopyArea(dis, win, realwin, hour_h, 0, 0, width, height, 0, 0);
	XFlush(dis);
	return(0);
}

int xerror(Display *dis, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        fputs(":: Wallclock : Somethings Up !\n", stderr);
    if(ee->error_code == BadDrawable) {
        fputs(":: Wallclock : Did the background change?\n", stderr);
        get_background();
        drawface();
        return 0;
    } else fputs(":: Wallclock : \033[0;31mBad Window Error!\n", stderr);
    return xerrorxlib(dis, ee); /* may call exit */
}


void quit() {
    XFreeGC(dis, hour_h);
	XFreeGC(dis, min_h);
	XFreeGC(dis, sec_h);
	XFreeGC(dis, face_cl);
	XFreeGC(dis, tick_m);
	XFreeGC(dis, bg_gc);
	XFreePixmap(dis, win);
	XFreePixmap(dis, win_face);
	//XFreePixmap(dis, root_pixmap);
	XCloseDisplay(dis);
}

int main(){
	int screen_num;
	unsigned long border;
	XEvent ev;
	int x11_fd;
    fd_set in_fds;
    struct timeval tv;

	/* First connect to the display server, as specified in the DISPLAY environment variable. */
	dis = XOpenDisplay(NULL);
	if (!dis) {fputs(":: Wallclock : unable to connect to display", stderr);return 1;}

	screen_num = DefaultScreen(dis);
    root = RootWindow(dis,screen_num);
    XSetErrorHandler(xerror);
    trans = TRANSPARENT;
    if(trans == 0) get_background();
	//background = None; //BlackPixel(dis, screen_num);
	border = WhitePixel(dis, screen_num);
	width = WIDTH;
	height = HEIGHT;
	XGCValues values;

	win = XCreatePixmap(dis, root, width, height, DefaultDepth(dis, screen_num));
	win_face = XCreatePixmap(dis, root, width, height, DefaultDepth(dis, screen_num));

	realwin = XCreateSimpleWindow(dis, root, POS_X,POS_Y,
	              width,height,0,border,None);

	// This returns the FD of the X11 display
    x11_fd = ConnectionNumber(dis);
	
	/* create the hour_h GC to draw the hour hand */
	values.foreground = getcolor(hour_hand_colour);
	values.line_width = hh_w;
	values.line_style = LineSolid;
	values.cap_style = CapRound;
	hour_h = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle|GCCapStyle,&values);

	/* create the min_h GC to draw the minute hand */
	values.foreground = getcolor(minute_hand_colour);
	values.line_width = mh_w;
	values.line_style = LineSolid;
	values.cap_style = CapRound;
	min_h = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle|GCCapStyle,&values);

	/* create the sec_h GC to draw the second hand */
	values.foreground = getcolor(second_hand_colour);
	values.line_width = sh_w;
	values.line_style = LineSolid;
	values.cap_style = CapRound;
	sec_h = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle|GCCapStyle,&values);

	/* create the face_cl GC to draw the clock face */
	values.foreground = getcolor(clock_face_colour);
	values.line_width = face_w;
	values.line_style = LineSolid;
	face_cl = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* create the face_bg GC to draw the clock face */
	values.foreground = getcolor(clock_face_background);
	values.line_width = face_w;
	values.line_style = LineSolid;
	face_bg = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* create the tick_m GC to draw the tick marks */
	values.foreground = getcolor(tick_mark_colour);
	values.line_width = tm_w;
	values.line_style = LineSolid;
	tick_m = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* create the bg_gc GC to draw the background */
	values.foreground = getcolor(background_colour);
	values.line_width = 8;
	values.line_style = LineSolid;
	bg_gc = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle,&values);

    /* want to accept the delete window protocol */
    wm_del_win = XInternAtom(dis,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(dis,realwin,&wm_del_win,1);
    XStoreName(dis, realwin, "WallClock");
	XClassHint* classHint;
    classHint = XAllocClassHint();
    classHint->res_name = "WallClock";
    classHint->res_class = "WallClock";
    XSetClassHint(dis, realwin, classHint);
    XFree(classHint);
	XSelectInput(dis, realwin, KeyPressMask|ButtonPressMask|StructureNotifyMask|ExposureMask );
    //XSelectInput(dis, root, PropertyChangeMask);

	XMapWindow(dis, realwin);

    drawface();
    update_hands();
	while(1) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        // Set our timer.  One second sounds good.
        tv.tv_usec = 0;
        if(clockupdate < 1) tv.tv_sec = 1;
        else tv.tv_sec = 30;

        // Wait for X Event or a Timer
        if (!(select(x11_fd+1, &in_fds, 0, 0, &tv))) {
            update_hands();
        }

        // Handle XEvents and flush the input 
        //while(XPending(dis) != 0) {
            XNextEvent(dis, &ev);
            switch(ev.type){
		    case Expose:
		        square = (width >= height) ? height-4 : width-4;
   		    	XFreePixmap(dis, win);
   		        win = XCreatePixmap(dis, root, width, height, DefaultDepth(dis, screen_num));	
   		    	XFreePixmap(dis, win_face);
   		        win_face = XCreatePixmap(dis, root, width, height, DefaultDepth(dis, screen_num));	
   		    	drawface();
   		    	update_hands();
		    	break;
		    case ConfigureNotify:
                win_x = ev.xconfigure.x;
                win_y = ev.xconfigure.y;
                width = ev.xconfigure.width;
                height = ev.xconfigure.height;
                drawface();
                update_hands();
		    	break;
		    case MapNotify:
		    	update_hands();
		    	break;
		    /*case PropertyNotify:
		        if(ev.xproperty.window != root || ev.xproperty.atom != id) continue;
	            get_background();
	            drawface();
	            fputs(":: WallClock :: background updated\n", stderr);
		        break; */
		    case KeyPress:
		        if(ev.xkey.keycode != 24) break;
		        quit();
	        	return(0);
		    case ButtonPress:
		        if(ev.xbutton.button != Button3) break;
		        quit();
                return(0);
            case ClientMessage:
                if(ev.xclient.window != realwin) break;
                quit();
                return(0);
                break;
		    }
		//}
	}
	return(0);
}
