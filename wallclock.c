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
#include <X11/Xlocale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

time_t t;
struct tm *tmval;

struct ti {
   int s_x,s_y,m_x,m_y,h_x,h_y;
} at;

static void readrc();
static void reload();
static int read_int(char *buff, int start);
static void read_char (char *buff, int start, char *colour);
static GC hands_gc(char *color, int gcwidth);
static GC fill_gc(char *color, int gcwidth);

static int square, center_x, center_y;
static int i, width, height, win_x, win_y;
static int angle1, angle2, angle3, trans;
static char RCFILE[100];
static unsigned int clockupdate, have_rootp;
static int tm_l, hh_w, hh_l, mh_w, mh_l, sh_w, sh_l, face_w, tm_w;
static char background_colour[10], hour_hand_colour[10], minute_hand_colour[10];
static char second_hand_colour[10], clock_face_background[10], clock_face_border[10];
static char tick_mark_colour[10];

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
 * They are *100, to avoid using floats */
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
    if(root_pixmap == None) {
        trans = have_rootp = 0;
        fputs(":: WALLCLOCK :: Failed Root Pixmap\n", stderr);
    } else have_rootp = 1;
}

int drawface() {
    center_x = width/2; // (square/2)+((width-square)/2);
    center_y = height/2; // (square/2)+((height-square)/2);

    if(trans == 1) {
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

    if (clockupdate == 1) {
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

GC hands_gc(char *color, int gcwidth) {
    XGCValues values;
    GC gc;

    values.foreground = getcolor(color);
    values.line_width = gcwidth;
    values.line_style = LineSolid;
    values.cap_style = CapRound;
    gc = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle|GCCapStyle,&values);
    if(gc < 0) fputs("WallClock :: GC FAIL\n", stderr);
    return gc;
}

GC fill_gc(char *color, int gcwidth) {
    XGCValues values;
    GC gc;

    values.foreground = getcolor(color);
    values.line_width = gcwidth;
    values.line_style = LineSolid;
    gc = XCreateGC(dis, root, GCForeground|GCLineWidth|GCLineStyle,&values);
    if(gc < 0) fputs("WallClock :: GC FAIL\n", stderr);
    return gc;
}

int read_int (char *buff, int start) {
    unsigned int i, j = 0;
    char ret[100];

    for(i=start; i<100;++i) {
        if(buff[i] == ' ') continue;
        if(buff[i] == '\n' || buff[i] == 0 || buff[i] == '\r') break;
        ret[j] = buff[i]; ++j;
    }
    ret[j] = '\0';
    return strtol(ret, NULL, 10);
}

void read_char(char *buff, int start, char *colour) {
    unsigned int i, j = 0;

    for(i=start;i<100;++i) {
        if(buff[i] == ' ' || buff[i] == '"') continue;
        if(buff[i] == '\n' || buff[i] == 0) break;
        colour[j] = buff[i]; ++j;
    }
    colour[j] = '\0';
}

void readrc() {
    FILE *rcfile;
    char buffer[100];

    rcfile = fopen( RCFILE, "r" );
    if (rcfile == NULL) {
        fputs("WallClock :: Failed to open ~/.config/wallclock/rc.conf\n", stderr);
        return;
    } else {
        while(fgets(buffer,sizeof buffer,rcfile) != NULL) {
            /* Check for comments */
            if(buffer[0] == '#' || buffer[0] == ' ' || buffer[0] == '\n') continue;
            /* Now look for info */
            if(strstr(buffer, "Transparent" ) != NULL) {
                trans = read_int(buffer, 12);
            } else if(strstr(buffer, "ShowSecondHand" ) != NULL) {
                clockupdate = read_int(buffer, 15);
            } else if(strstr(buffer, "WindowWidth" ) != NULL) {
                width = (width > 0) ? width : read_int(buffer, 12);
            } else if(strstr(buffer, "WindowHeight" ) != NULL) {
                height = (height > 0) ? height : read_int(buffer, 13);
            } else if(strstr(buffer, "WindowX" ) != NULL) {
                win_x = (win_x > 0) ? win_x : read_int(buffer, 8);
            } else if(strstr(buffer, "WindowY" ) != NULL) {
                win_y = (win_y > 0) ? win_y : read_int(buffer, 8);
            } else if(strstr(buffer, "ClockBorderWidth" ) != NULL) {
                face_w = read_int(buffer, 17);
            } else if(strstr(buffer, "TickMarkWidth" ) != NULL) {
                tm_w = read_int(buffer, 14);
            } else if(strstr(buffer, "TickMarkLength" ) != NULL) {
                tm_l = 100 - read_int(buffer, 15);
            } else if(strstr(buffer, "HourHandWidth" ) != NULL) {
                hh_w = read_int(buffer, 14);
            } else if(strstr(buffer, "HourHandLength" ) != NULL) {
                hh_l = read_int(buffer, 15);
            } else if(strstr(buffer, "MinuteHandWidth" ) != NULL) {
                mh_w = read_int(buffer, 16);
            } else if(strstr(buffer, "MinuteHandLength" ) != NULL) {
                mh_l = read_int(buffer, 17);
            } else if(strstr(buffer, "SecondHandWidth" ) != NULL) {
                sh_w = read_int(buffer, 16);
            } else if(strstr(buffer, "SecondHandLength" ) != NULL) {
                sh_l = read_int(buffer, 17);
            } else if(strstr(buffer, "WindowBackgroundColour" ) != NULL) {
                read_char(buffer, 23, background_colour);
            } else if(strstr(buffer, "ClockBorderColour" ) != NULL) {
                 read_char(buffer, 18, clock_face_border);
            } else if(strstr(buffer, "TickMarkColour" ) != NULL) {
                 read_char(buffer, 15, tick_mark_colour);
            } else if(strstr(buffer, "ClockFaceBackgroundColour" ) != NULL) {
                 read_char(buffer, 26, clock_face_background);
            } else if(strstr(buffer, "HourHandColour" ) != NULL) {
                 read_char(buffer, 15, hour_hand_colour);
            } else if(strstr(buffer, "MinuteHandColour" ) != NULL) {
                 read_char(buffer, 17, minute_hand_colour);
            } else if(strstr(buffer, "SecondHandColour" ) != NULL) {
                 read_char(buffer, 17, second_hand_colour);
            }
        }
    }
    fclose(rcfile);
}

void reload() {
    readrc();
    hour_h = hands_gc(hour_hand_colour, hh_w);
    min_h = hands_gc(minute_hand_colour, mh_w);
    sec_h = hands_gc(second_hand_colour, sh_w);
    tick_m = hands_gc(tick_mark_colour, tm_w);
    face_cl = fill_gc(clock_face_border, face_w);
    face_bg = fill_gc(clock_face_background, face_w);
    bg_gc = fill_gc(background_colour, 8);
    if(trans == 1 && have_rootp == 0) get_background();
    drawface();
}

int main(){
    dis = XOpenDisplay(NULL);
    if (!dis) {fputs(":: Wallclock : unable to connect to display", stderr);return 1;}

    int screen_num;
    unsigned long border;
    XEvent ev;
    int x11_fd;
    fd_set in_fds;
    struct timeval tv;

    /* set defaults */
    trans = 0;
    clockupdate = 1;
    width = height = win_x = win_y = 0;
    face_w = 2;
    tm_w = 6;
    tm_l = 7;
    hh_w = 8;
    hh_l = 60;
    mh_w = 6;
    mh_l = 75;
    sh_w = 2;
    sh_l = 91;
    have_rootp = 0;

    char *loc;
    loc = setlocale(LC_ALL, "");
    if (!loc || !strcmp(loc, "C") || !strcmp(loc, "POSIX") || !XSupportsLocale())
        fputs("WallClock :: LOCALE FAILED\n", stderr);
    // Read in RC_FILE
    sprintf(RCFILE, "%s/.config/wallclock/rc.conf", getenv("HOME"));

    screen_num = DefaultScreen(dis);
    root = RootWindow(dis,screen_num);
    XSetErrorHandler(xerror);
    //background = None; //BlackPixel(dis, screen_num);
    border = WhitePixel(dis, screen_num);

    reload();
    if(trans == 1) get_background();

    win = XCreatePixmap(dis, root, width, height, DefaultDepth(dis, screen_num));
    win_face = XCreatePixmap(dis, root, width, height, DefaultDepth(dis, screen_num));

    realwin = XCreateSimpleWindow(dis, root, win_x,win_y,
                  width,height,0,border,None);

    // This returns the FD of the X11 display
    x11_fd = ConnectionNumber(dis);

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
    XSelectInput(dis, root, PropertyChangeMask);

    XMapWindow(dis, realwin);

    KeyCode keyQ = XKeysymToKeycode(dis, XK_q);
    KeyCode keyR = XKeysymToKeycode(dis, XK_r);

    drawface();
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
        while(XPending(dis) != 0) {
            XNextEvent(dis, &ev);
            switch(ev.type){
            case Expose:
                if(ev.xexpose.count > 0) continue;
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
            case PropertyNotify:
                if(trans != 1 || ev.xproperty.atom != id) {
                    update_hands();
                    break;
                }
                get_background();
                fputs(":: WallClock :: background updated\n", stderr);
                drawface();
                break;
            case KeyPress:
                if(ev.xkey.keycode == keyQ) {
                    quit();
                    return(0);
                } else if(ev.xkey.keycode == keyR) {
                    reload();
                }
                break;
            case ButtonPress:
                if(ev.xbutton.button != Button3) break;
                quit();
                return(0);
            case ClientMessage:
                if(ev.xclient.window != realwin) break;
                quit();
                return(0);
                break;
            default: break;
            }
        }
    }
    return(0);
}
