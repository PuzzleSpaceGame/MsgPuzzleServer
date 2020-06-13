#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "puzzles.h"

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include "utils.h"

#include <glib.h>

#define CLASSNAME thegame.name

struct color {
    int r;
    int g;
    int b;
};

struct frontend {
    const game *game;
    midend *me;
    struct color *colours;
    int timer;
    struct preset_menu *preset_menu;
    struct preset_menuitemref *preset_menuitems;
    int n_preset_menuitems;
    struct font *fonts;
    int nfonts, fontsize;
    config_item *cfg;
    struct cfg_aux *cfgaux;
    int cfg_which, dlg_done;
    bool help_running;
    enum { DRAWING, PRINTING, NOTHING } drawstatus;
    int printcount, printw, printh;
    bool printsolns, printcurr, printcolour;
    float printscale;
    int printoffsetx, printoffsety;
    float printpixelscale;
    int fontstart;
    int linewidth;
    bool linedotted;
    drawing *dr;
    int xmin, ymin;
    float puzz_scale;
};

void fatal(const char *fmt, ...){
    //printf("fatal, msg: %s\n", fmt);
    exit(1);
}
//TODO: Generate an actual random number
void get_random_seed(void **randseed, int *randseedsize)
{
    printf("get_random_seed\n");
    *randseed = smalloc(64);
    fflush(stdout);
    fgets(*randseed,64,stdin);
    * randseedsize = 64;
}
// TODO: Make an actual default color
void frontend_default_colour(frontend *fe, float *output)
{
    printf("frontend_default_colour\n");
    output[0] = 0.1;
    output[1] = 0.1;
    output[2] = 0.1;
}
void deactivate_timer(frontend *fe)
{
    if (!fe)
	return;			       /* for non-interactive midend */
    printf("deactivate_timer\n");
    fe->timer = 0;
}

void activate_timer(frontend *fe)
{
    if (!fe)
	return;			       /* for non-interactive midend */
    if (!fe->timer) {
        printf("activate_timer\n");
    }
}
static void msg_draw_text(void *handle, int x, int y, int fonttype,
			  int fontsize, int align, int colour,
                          const char *text)
{
    GQueue *drawhandle = handle;
    g_queue_push_tail(drawhandle,g_strdup_printf("{draw:text,\n" 
            "x: %d,\n" 
            "y: %d,\n" 
            "fonttype: %d,\n"
            "fontsize: %d,\n"
            "align: %d,\n"
            "colour: %d,\n"
            "text: %s},\n",
            x,y,fonttype,fontsize,align,colour,text));
}

static void msg_draw_rect(void *handle, int x, int y, int w, int h, int colour)
{
    GQueue *drawhandle = handle;
    g_queue_push_tail(drawhandle,g_strdup_printf("{draw:rect,\n"
            "x: %d,\n"
            "y: %d,\n"
            "w: %d,\n"
            "h: %d,\n"
            "colour: %d},\n",
            x,y,w,h,colour));
}

static void msg_draw_line(void *handle, int x1, int y1, int x2, int y2, int colour)
{ 
    GQueue *drawhandle = handle;
    g_queue_push_tail(drawhandle,g_strdup_printf("{draw:line,\n"
            "x1: %d,\n"
            "y1: %d,\n"
            "x2: %d,\n"
            "y2: %d,\n"
            "colour: %d},\n",
            x1,y1,x2,y2,colour));
}

static void msg_draw_polygon(void *handle, int *coords, int npoints,
		     int fillcolour, int outlinecolour)
{ 
    GQueue *drawhandle = handle;
    //convert coords array to string
    char str[128];
    int i = 0;
    int index = 0;
    index += sprintf(&str[index],"[%d,",coords[0]);
    for(i=1;i<npoints;i++){
        index += sprintf(&str[index], ",%d", coords[i]);
    }
    index += sprintf(&str[index], "]");
    g_queue_push_tail(drawhandle,g_strdup_printf(
                "{draw:polygon,\n"
                "coords: %s,\n"
                "npoints: %d,\n"
                "fillcolour: %d,\n"
                "outlinecolour: %d},\n",
                str,npoints,fillcolour,outlinecolour));
}

static void msg_draw_circle(void *handle, int cx, int cy, int radius,
			 int fillcolour, int outlinecolour)
{
    GQueue *drawhandle = handle;
    g_queue_push_tail(drawhandle,g_strdup_printf("draw:circle,\n"
            "cx: %d,\n"
            "cy: %d,\n"
            "radius: %d,\n"
            "fillcolour: %d,\n"
            "outlinecolour: %d},\n",
            cx,cy,radius,fillcolour,outlinecolour));
}

static void msg_draw_update(void *handle, int x, int y, int w, int h)
{
    GQueue *drawhandle = handle;
    g_queue_push_tail(drawhandle,g_strdup_printf("]},\ndraw_update:{\n"
            "x: %d,\n"
            "y: %d,\n"
            "w: %d,\n"
            "h: %d,\n"
            "cmds: [",
            x,y,w,h));
}

static void msg_clip(void *handle, int x, int y, int w, int h)
{
    GQueue *drawhandle = handle;
    g_queue_push_tail(drawhandle,g_strdup_printf("clip:{\n"
            "x: %d,\n"
            "y: %d,\n"
            "w: %d,\n"
            "h: %d,\n"
            "cmds:[\n",
            x,y,w,h));
}

static void msg_unclip(void *handle)
{
    GQueue *drawhandle = handle;
    g_queue_push_tail(drawhandle,g_strdup_printf("]},"));
}

static void msg_start_draw(void *handle)
{
}

static void msg_end_draw(void *handle)
{
}

static void msg_status_bar(void *handle, const char *text)
{
    printf("status_bar, text: %s\t", text);
}

struct blitter {
   frontend *fe;
   int x, y, w, h;
};

static blitter *msg_blitter_new(void *handle, int w, int h){    
    fatal("blitter func called");
}

static void msg_blitter_free(void *handle, blitter *bl)
{
    fatal("blitter func called");
}
static void msg_blitter_save(void *handle, blitter *bl, int x, int y)
{
    fatal("blitter func called");
}

static void msg_blitter_load(void *handle, blitter *bl, int x, int y)
{
    fatal("blitter func called");
}

static void msg_begin_doc(void *handle, int pages)
{
    fatal("print func called");
}

static void msg_begin_page(void *handle, int number)
{
    fatal("print func called");
}

static void msg_begin_puzzle(void *handle, float xm, float xc,
			  float ym, float yc, int pw, int ph, float wmm)
{
    fatal("print func called");
}

static void msg_end_puzzle(void *handle)
{
    fatal("print func called");
}

static void msg_end_page(void *handle, int number)
{
    fatal("print func called");
}

static void msg_end_doc(void *handle)
{
    fatal("print func called");
}

static void msg_line_width(void *handle, float width)
{
    fatal("print func called");
}

static void msg_line_dotted(void *handle, bool dotted)
{
    fatal("print func called");
}

char *msg_text_fallback(void *handle, const char *const *strings, int nstrings)
{
    return dupstr(strings[0]);
}

const struct drawing_api msg_drawing = {
    msg_draw_text,
    msg_draw_rect,
    msg_draw_line,
    msg_draw_polygon,
    msg_draw_circle,
    msg_draw_update,
    msg_clip,
    msg_unclip,
    msg_start_draw,
    msg_end_draw,
    msg_status_bar,
    msg_blitter_new,
    msg_blitter_free,
    msg_blitter_save,
    msg_blitter_load,
    msg_begin_doc,
    msg_begin_page,
    msg_begin_puzzle,
    msg_end_puzzle,
    msg_end_page,
    msg_end_doc,
    msg_line_width,
    msg_line_dotted,
    msg_text_fallback,
};

static frontend *frontend_new()
{
    frontend *fe;
    const char *nogame = "Puzzles (no game selected)";

    fe = snew(frontend);

    fe->game = NULL;
    fe->me = NULL;

    fe->timer = 0;

    fe->help_running = false;

    fe->drawstatus = NOTHING;
    fe->dr = NULL;
    fe->fontstart = 0;

    fe->fonts = NULL;
    fe->nfonts = fe->fontsize = 0;

    fe->colours = NULL;

    fe->puzz_scale = 1.0;

    return fe;
}

static bool savefile_read(void *wctx, void *buf, int len)
{
    FILE *fp = (FILE *)wctx;
    int ret;

    ret = fread(buf, 1, len, fp);
    return (ret == len);
}

static midend *midend_for_new_game(frontend *fe, const game *cgame,
                                   char *arg, bool maybe_game_id,
                                   bool maybe_save_file, char **error)
{
    midend *me = NULL;

    if (!arg) {
        if (me) midend_free(me);
        me = midend_new(fe, cgame, &msg_drawing, fe);
        midend_new_game(me);
    } else {
        FILE *fp;
        const char *err_param, *err_load;

        /*
         * See if arg is a valid filename of a save game file.
         */
        err_load = NULL;
        if (maybe_save_file && (fp = fopen(arg, "r")) != NULL) {
            const game *loadgame;

            loadgame = cgame;
            if (!err_load) {
                if (me) midend_free(me);
                me = midend_new(fe, loadgame, &msg_drawing, fe);
                err_load = midend_deserialise(me, savefile_read, fp);
            }
        } else {
            err_load = "Unable to open file";
        }

        if (maybe_game_id && (!maybe_save_file || err_load)) {
            /*
             * See if arg is a game description.
             */
            if (me) midend_free(me);
            me = midend_new(fe, cgame, &msg_drawing, fe);
            err_param = midend_game_id(me, arg);
            if (!err_param) {
                midend_new_game(me);
            } else {
                if (maybe_save_file) {
                    *error = snewn(256 + strlen(arg) + strlen(err_param) +
                                   strlen(err_load), char);
                    sprintf(*error, "Supplied argument \"%s\" is neither a"
                            " game ID (%s) nor a save file (%s)",
                            arg, err_param, err_load);
                } else {
                    *error = dupstr(err_param);
                }
                midend_free(me);
                sfree(fe);
                return NULL;
            }
        } else if (err_load) {
            *error = dupstr(err_load);
            midend_free(me);
            sfree(fe);
            return NULL;
        }
    }

    return me;
}

static void get_max_puzzle_size(frontend *fe, int *x, int *y){
    *x = *y = INT_MAX;
}

static int fe_set_midend(frontend *fe, midend *me)
{
    int x, y;

    if (fe->me) {
        midend_free(fe->me);
        fe->preset_menu = NULL;
        sfree(fe->preset_menuitems);
    }
    fe->me = me;
    fe->game = midend_which_game(fe->me);

    {
	int i, ncolours;
        float *colours;

        colours = midend_colours(fe->me, &ncolours);

        if (fe->colours) sfree(fe->colours);

	fe->colours = snewn(ncolours,struct color);
	for (i = 0; i < ncolours; i++) {
        fe->colours->r = 255 * colours[i*3+0];
        fe->colours->g = 255 * colours[i*3+1];
        fe->colours->b = 255 * colours[i*3+2];
	}
        sfree(colours);
    }


    get_max_puzzle_size(fe, &x, &y);
    midend_size(fe->me, &x, &y, false);


    {

        fe->preset_menu = midend_get_presets(
            fe->me, &fe->n_preset_menuitems);
    }


    return 0;
}

void write_to_STDOUT(void *ctx, const void *buf, int len){
    printf("%s",(char *)buf);
}

void pipe_serialized(struct midend *me){
    midend_serialise(me,&write_to_STDOUT,NULL);
    printf("\n");
}
bool read_serialized(void *ctx, void *buf, int len){
    int got = fread(buf,1,len,stdin);
    //printf("DBG: len: %d, got: %d\n",len,got,buf);
    return got == len;
}

enum commands {
    SERVER_ERROR,
    SERVER_KILL,
    SERVER_NEW_GAME,
    SERVER_REDRAW_GAME,
    SERVER_UPDATE_GAME,
    SERVER_GET_CONFIG,
    SERVER_ECHO
};

int getserverstate(void){
    //TODO get message and decide on what state to be in
    return SERVER_ERROR;
}

int gameloop(amqp_connection_state_t pwq_conn amqp_connection_state_t adm_conn ){
    printf("SERVER STARTING\n");
    //Setup For this function
    bool keepgoing = true;
    int i;
    int j;
    int xi,yi,bi;

    //Setup For SimonTPPC
    char * starterror = NULL;
    const char * error = NULL;
    const game *gg;
    frontend *fe;
    midend *me;
    
    gg = &thegame;
    fe = frontend_new();

    me = midend_for_new_game(fe, gg, NULL, false, false, &starterror);
    fe_set_midend(fe, me);
    
    if (!me) {
        char buf[128];
        sprintf(buf, "%.100s Error", gg->name);
        return 1;
    }

    int x = INT_MAX;
    int y = INT_MAX; 
    GQueue *drawhandle;
    config_item *cfg;
    char **garbage;
    while(keepgoing){
        switch(getserverstate()){
            case SERVER_KILL: 
                keepgoing = false;
                break;
            case SERVER_NEW_GAME:
                midend_new_game(me);
                break;
            case SERVER_UPDATE_GAME:
                midend_process_key(me,xi,yi,bi);
                break;
            case SERVER_REDRAW_GAME:
                x = INT_MAX;
                y = INT_MAX;
                midend_size(me, &x, &y, false);
                drawhandle = g_queue_new();
                fe->dr = drawing_new(&msg_drawing,me,drawhandle);
                g_queue_push_tail(drawhandle,g_strdup_printf("{draw:true,size: x: %d,\n y %d,\n discard:[{",x,y));
                midend_force_redraw(me);
                g_queue_push_tail(drawhandle,g_strdup("}]}"));
                g_queue_free(drawhandle);
                drawing_free(fe->dr);
                break;
            case SERVER_GET_CONFIG:
                cfg = midend_get_config(me,CFG_SETTINGS,garbage);
                drawhandle = g_queue_new();
                g_queue_push_tail(drawhandle,g_strdup("{config:true,\nopts:[\n"));
                i=0;
                while(cfg[i].type != C_END){
                    g_queue_push_tail(drawhandle,g_strdup_printf("{name:%s,\n",cfg[i].name));
                    if(cfg[i].type == C_STRING){
                        g_queue_push_tail(drawhandle,g_strdup("type:string,\n"));
                    } else if(cfg[i].type == C_BOOLEAN){
                        g_queue_push_tail(drawhandle,g_strdup("type:boolean,\n"));
                    } else if(cfg[i].type == C_CHOICES){
                        g_queue_push_tail(drawhandle,"type:choices,\n");
                        g_queue_push_tail(drawhandle,g_strdup_printf("choices:%s,\n",cfg[i].u.choices.choicenames));
                    }
                    g_queue_push_tail(drawhandle,g_strdup("},\n"));
                }
                g_queue_push_tail(drawhandle,g_strdup("]}"));
                g_queue_free(drawhandle);
                free_cfg(cfg);
                break;
            case SERVER_ECHO:
                printf("ECHO\n");
                break;
            case SERVER_ERROR:
            default :
                printf("SERVER ERROR\n");
        }
    }
    return 0;
}

int main(int argc, char*argv[]){    
    //For Puzzle Work Queue
	char const *pwq_hostname;
	int pwq_port, pwq_status;
	char const *pwq_queuename; 
    char const *pwq_user, *pwq_pass;
    amqp_socket_t *pwq_socket = NULL;
	amqp_connection_state_t pwq_conn;
    //For Shutdown Queue
	char const *adm_hostname;
	int adm_port, adm_status;
	char const *adm_queuename; 
    char const *adm_user, *adm_pass;
    amqp_socket_t *adm_socket = NULL;
	amqp_connection_state_t adm_conn;
    //Handle Command Line Args
    if(argc % 2 != 1 || argc < 13){
        fprintf(stderr,"Usage requires the following tagged args: -pwq_host, -pwq_port, -pwq_queue and their adm counterparts");
    } 
    for(int i=1;i < argc;i+=2){
        if(argv[i] == "-pwq_host"){
		    pwq_hostname = argv[i+1];
        }
        if(argv[i] == "-pwq_port"){
            pwq_port = atoi(argv[i+1]);
        }
        if(argv[i] == "-pwq_queue"){
            pwq_queuename = argv[i+1];
        }
        if(argv[i] == "-pwq_user"){
            pwq_user = argv[i+1];
        }
        if(argv[i] == "-pwq_pass"){
            pwq_pass = argv[i+1];
        }

        if(argv[i] == "-adm_host"){
		    adm_hostname = argv[i+1];
        }
        if(argv[i] == "-adm_port"){
            adm_port = atoi(argv[i+1]);
        }
        if(argv[i] == "-adm_queue"){
            adm_queuename = argv[i+1];
        }
        if(argv[i] == "-adm_user"){
            adm_user = argv[i+1];
        }
        if(argv[i] == "-adm_pass"){
            adm_pass = argv[i+1];
        }
    }
    
    pwq_conn = amqp_new_connection();
    pwq_socket = amqp_tcp_socket_new(pwq_conn);
    if (!pwq_socket){
        die("creating PWQ TCP socket");
    }
    pwq_status = amqp_socket_open(pwq_socket,pwq_hostname,pwq_port);
    if (pwq_status){
        die("opening PWQ TCP socket");
    }

    die_on_amqp_error(amqp_login(pwq_conn, "/", 0, 131072, 0,
                AMQP_SASL_METHOD_PLAIN, pwq_user, pwq_pass),
            "Logging In to PWQ");
    amqp_channel_open(pwq_conn, 1);
    die_on_amqp_error(amqp_get_rpc_reply(pwq_conn), "Opening PWQ channel");
    amqp_basic_consume(pwq_conn, 1, amqp_cstring_bytes(pwq_queuename), amqp_empty_bytes,
                                 0, 0, 0, amqp_empty_table);
    die_on_amqp_error(amqp_get_rpc_reply(pwq_conn), "Consuming PWQ");


    adm_conn = amqp_new_connection();
    adm_socket = amqp_tcp_socket_new(adm_conn);
    if (!adm_socket){
        die("creating PWQ TCP socket");
    }
    adm_status = amqp_socket_open(adm_socket,adm_hostname,adm_port);
    if (adm_status){
        die("opening PWQ TCP socket");
    }

    die_on_amqp_error(amqp_login(adm_conn, "/", 0, 131072, 0,
                AMQP_SASL_METHOD_PLAIN, adm_user, adm_pass),
            "Logging In to PWQ");
    amqp_channel_open(adm_conn, 1);
    die_on_amqp_error(amqp_get_rpc_reply(adm_conn), "Opening PWQ channel");
    amqp_basic_consume(adm_conn, 1, amqp_cstring_bytes(adm_queuename), amqp_empty_bytes,
                                 0, 0, 0, amqp_empty_table);
    die_on_amqp_error(amqp_get_rpc_reply(adm_conn), "Consuming PWQ");
    
    return gameloop(pwq_conn,adm_conn);
}


